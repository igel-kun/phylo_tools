
#include <ranges>
#include <utility>

#include "utils/benchmark.hpp"
#include "utils/command_line.hpp"
#include "utils/token.hpp"

#include "io/newick.hpp"
#include "utils/network.hpp"
#include "utils/diversity.hpp"
#include "utils/features.hpp"

#include "utils/net_generator.hpp"

namespace ra = std::ranges;
namespace rv = std::views;

using namespace PT;
using namespace std::literals;

// no node data, but edges are annotated with p, w, and gamma
struct EdgeData {
  float inheritance_prob = 1;
  float weight = 0;
  float gamma = 0;

  EdgeData() = default;

  // construct from a given string that's been read from the input file
  EdgeData(const std::string_view in) {
    auto iter = mstd::tokenize(in, ",;:"sv).begin();
    while(iter && (*iter == "")) ++iter;
    if(iter) weight = std::stof(*iter);
    while(++iter && (*iter == ""));
    if(iter)
      if(not mstd::try_reading_number<float>(*iter, inheritance_prob))
        inheritance_prob = 1;
  }

  friend std::ostream& operator<<(std::ostream& os, const EdgeData& ed) {
    return os << "{inh: "<<ed.inheritance_prob<<", w: "<<ed.weight<<" gam: "<<ed.gamma<<'}';
  }
};

// return the gamma, inheritence-probablility, and weight of an edge
struct UtilityFunctors {
  using Gamma = decltype(EdgeData::gamma);
  static constexpr auto& gamma(const auto& e) { return e.data().gamma; }
  static constexpr auto& iprob(const auto& e) { return e.data().inheritance_prob; }
  static constexpr auto& weight(const auto& e) { return e.data().weight; }
  static constexpr auto score(const auto& e) { return weight(e) * gamma(e); }
};

using MyNetwork = DefaultLabeledNetwork<void, EdgeData>;
using MyNode = typename MyNetwork::Node;
using MyEdge = typename MyNetwork::Edge;
using NameToNode = std::unordered_map<std::string, NodeDesc>;

NameToNode name_to_node;

mstd::OptionMap options;

void parse_options(const int argc, const char** argv) {
  mstd::OptionDesc description;
  description["-v"] = {0,0};
  description["-V"] = {0,0};
  description["-mr"] = {0,0};
  description["-ml"] = {0,0};
  description["-mb"] = {0,0};
  description["-f"] = {0,0};
  description["-l"] = {1,1};
  description["-S"] = {1,1};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " [FLAGS] <file> <<k> | -l <leaf list>>\n\
      \tLet N be the network described in file, where each leaf is annotated with its taxon name,\n\
      and each edge uv is annotated with 1-2 floating-point values\n\
      the first indicating the weight, the (possible) second indicating its inheritence probability p, if any.\n\
      This programm either computes the diversity score of the given leaves (if -l option is present),\n\
      or computes a set of k leaves maximizing the diversity score\n\
      (add % to k in order to express a number relative to the total number of leaves, e.g. 25%).\n\
      See whitepaper [TODO] for definitions.\n\
      FLAGS:\n\
      \t-v\tverbose output, prints network\n\
      \t-mr\tuse alternative diversity definition (via switchings, dynamic programming for reticulations)\n\
      \t-ml\tuse alternative diversity definition (via switchings, dynamic programming for level)\n\
      \t-mb\tuse alternative diversity definition (via switchings, brute force)\n\
      \t-V\tcheck solution(s) by brute-forcing switchings\n\
      \t-S i\tnumber i of highest-scoring solutions to return (not compatible with -l)\n\
      \t-l\tcompute the diversity score for the given list of leaves (comma separated list of taxa, no spaces)\n\
      \t-f\tinstead of phylo-diversity, compute feature-diversity of the features given as a matrix in <file>\n");

  mstd::parse_options(argc, argv, description, help_message, options);

  if(not file_exists(options[""].front()))
    cfail(std::string{"couldn't open file "} + options[""].front());

  if((not test(options, "-l")) && (options[""].size() < 2))
    cfail(std::string{"If you want me to compute a leaf-set maximizing the diversity score, you'll have to give me an upper bound k on the size of said leaf-set. Otherwise, I'll just take all the leaves and that's not what you want is it?\n\n"} + help_message);
  
  if(test(options, "-mr") + test(options, "-ml") + test(options, "-mb") > 1)
    cfail("-mr, -ml and -mb are mutually exclusive, please chose a method between brute-force (-mb), level-DP (-ml) and reticulation-DP (-mr)\n");

  if(test(options, "-l") && test(options, "-S"))
    cfail("-l and -S are mutually exclusive\n");
}

size_t parse_k(const float baseline, const std::string_view k_str) {
  const char perc = k_str.back();
  if(perc == '%') {
    return stof(k_str) * baseline / 100.0f;
  } else {
    return stof(k_str);
  }
}

NameVec parse_leaves(const std::string_view in) {
  NameVec result;
  for(const auto& s: mstd::tokenize(in, ','))
    result.emplace_back(s);
  return result;
}

// overwrite the emplacement-helper set_label function to store the Label-->NodeDesc mapping in name_to_node
struct MyHelper: public EdgeEmplacementHelper<MyNetwork, true> {
  using Parent = EdgeEmplacementHelper<MyNetwork, true>;
  using Parent::Parent;

  template<class Label>
  void set_label(const NodeDesc u, Label&& label) {
    name_to_node.emplace(label, u);
    Parent::set_label(u, std::forward<Label>(label));
  }
};

MyNetwork read_network(const std::string& in) {
  try{
    std::ifstream in_stream{std::string{in}};
    return parse_newick<MyNetwork>(in_stream, MyHelper{});
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<in<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

using FeatureMap = HashMap<std::string, PT::DefaultFeatureCollection>;

auto read_features(const std::string& filename) {
  std::ifstream in{filename};
  FeatureMap feature_map;
  PT::read_features(in,
      [&](const std::string& s) -> typename FeatureMap::mapped_type& { return feature_map[s]; },
      std::vector{1}); // skip column 1
  return feature_map;
}


int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  // how many solutions to return
  const size_t num_solutions = test(options, "-S") ? std::stoi(options["-S"][0]) : 1;

  if(test(options, "-f")) {
    std::cout << "parsing features from "<<options[""][0]<<"...\n";
    const auto feature_map = read_features(options[""][0]);
    DEBUG2(std::cout << "read feature map:\n" << feature_map << '\n');

    if(test(options, "-l")) {
      const NameVec leaf_names = parse_leaves(options["-l"][0]);
      std::cout << "computing diversity score of leaves " << leaf_names << '\n';
      auto selected_features = feature_map | std::ranges::views::filter([&](const auto& x){return test(leaf_names, x.first);})
                                           | std::ranges::views::transform([](const auto& x)->auto& { return x.second; });
      const auto score = _feature_diversity{}(selected_features);
      std::cout << "score = "<<score<<'\n';
    } else {
      const size_t k = parse_k(feature_map.size(), options[""][1]);
      const auto before = mstd::get_time();
      const auto solutions = optimize_feature_diversity(k, feature_map, num_solutions).solutions;
      const auto elapsed = mstd::ms_between(before, mstd::get_time());
      std::cout << "("<<elapsed<<"ms)\n";

      for(const auto& [feats, score]: solutions)
        std::cout << "maximum feature-diversity = " << score << ":\n" << mstd::Linewise{feats, false} << '\n';
    }
  } else {
    std::cout << "reading network...\n";
    MyNetwork N(read_network(options[""][0]));

    if(mstd::test(options, "-v")) {
      std::cout << "N ("<<N.num_nodes()<<" nodes, "<<N.num_edges()<<" edges -> reti num:" << N.num_edges()-N.num_nodes()+1<<"):" << std::endl;
      std::cout << ExtendedDisplay(N) << std::endl;
      N.print_summary(std::cout);
    }

    if(test(options, "-l")) {
      const NameVec leaf_names = parse_leaves(options["-l"][0]);
      const auto leaves_range = leaf_names | rv::transform([&](const std::string& lname){ return name_to_node.at(lname); });
      const NodeSet leaves{leaves_range.begin(), leaves_range.end()};
      std::cout << "computing diversity score of leaves " << leaf_names << '\n';
      const auto score = test(options, "-mb") ? 
        pd_score_ct(N, leaves, UtilityFunctors()) :
        ((test(options, "-ml") || test(options, "-mr")) ?
          pd_score_ct_dp(N, leaves, UtilityFunctors()) :
          pd_score_classic(N, leaves, UtilityFunctors())
        );
      std::cout << "score = "<<score<<'\n';
    } else {
      const size_t k = parse_k(N.num_leaves(), options[""][1]);
      //using T = decltype(pd_score_ct<MyNetwork, NodeSet, UtilityFunctors>);
      std::cout << "computing optimal diversity score obtainable with " << k << " leaves\n";
      const auto before = mstd::get_time();
      const auto solutions = test(options, "-mb") ? 
        optimize_diversity_brute_force(N, k, UtilityFunctors(), _pd_score_ct{}, num_solutions).solutions :
        (test(options, "-ml") ?
          optimize_displayed_tree_diversity_level(N, k, UtilityFunctors(), num_solutions).solutions :
          (test(options, "-mr") ?
            optimize_displayed_tree_diversity(N, k, UtilityFunctors(), num_solutions).solutions :
            optimize_diversity_brute_force(N, k, UtilityFunctors(), _pd_score_classic{}, num_solutions).solutions
          )
        );
      const auto elapsed = mstd::ms_between(before, mstd::get_time());
      std::cout << std::fixed << std::setprecision(0) << "("<<elapsed<<"ms)\n";
      // 
      if(test(options, "-V")) { // verify against brute-force
        DEBUG4(std::cout << "let's check solutions against brute-force...\n");
        _pd_score_ct brute_force;
        for(const auto& sol: solutions) {
          const double score = sol.second;
          const double bf_score = brute_force(N, sol.first, UtilityFunctors());
          if((bf_score < score - 0.01) or (bf_score > score + 0.01))
            cfail(std::string("Uh oh, solution ") + std::to_string(sol.first) + " scoring " + std::to_string(score) +
                " vs. " + std::to_string(bf_score) + " by brute-force\n");
        }
      }
      for(const auto& [sol, score]: solutions) {
        std::cout << std::fixed << std::setprecision(2)<< "solution with diversity "<<score<<": " << (sol | rv::transform([&](const NodeDesc u){ return N[u].label();})) <<"\n";
      }
    }
  }
}



