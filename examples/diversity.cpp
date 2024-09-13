
#include <ranges>
#include <utility>

#include "utils/benchmark.hpp"
#include "utils/command_line.hpp"
#include "utils/token.hpp"

#include "io/newick.hpp"
#include "utils/network.hpp"
#include "utils/diversity.hpp"
#include "utils/features.hpp"


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

OptionMap options;

void parse_options(const int argc, const char** argv) {
  OptionDesc description;
  description["-v"] = {0,0};
  description["-m"] = {0,0};
  description["-f"] = {0,0};
  description["-l"] = {1,1};
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
      \t-m\tuse alternative diversity definition (via switchings)\n\
      \t-l\tcompute the diversity score for the given list of leaves (comma separated list of taxa, no spaces)\n\
      \t-f\tinstead of phylo-diversity, compute feature-diversity of the features given as a matrix in <file>\n");

  parse_options(argc, argv, description, help_message, options);

  if(not file_exists(options[""].front()))
    cfail(std::string{"couldn't open file "} + options[""].front());

  if((not test(options, "-l")) && (options[""].size() < 2)) {
    std::cerr << "If you want me to compute a leaf-set maximizing the diversity score, you'll have to give me an upper bound k on the size of said leaf-set. Otherwise, I'll just take all the leaves and that's not what you want is it?\n";
    std::cerr << '\n' << help_message;
    exit(EXIT_FAILURE);
  }
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
struct MyHelper: public EdgeEmplacementHelper<true, MyNetwork> {
  using Parent = EdgeEmplacementHelper<true, MyNetwork>;
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
//      [&](const std::string& s) { return feature_map[s]; },
      std::vector{1});
  return feature_map;
}


int main(const int argc, const char** argv) {
  std::cout << "parsing options...\n";
  parse_options(argc, argv);

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
      const auto [feats, score] = optimize_feature_diversity(k, feature_map);
      std::cout << "maximum feature-diversity = " << score << ":\n" << mstd::Linewise{feats, false} << '\n';
    }
  } else {
    std::cout << "reading network...\n";
    MyNetwork N(read_network(options[""][0]));

    if(mstd::test(options, "-v")) {
      std::cout << "N:" << std::endl;
      std::cout << ExtendedDisplay(N) << std::endl;
      N.print_summary(std::cout);
    }




    if(test(options, "-l")) {
      const NameVec leaf_names = parse_leaves(options["-l"][0]);
      const auto leaves_range = leaf_names | rv::transform([&](const std::string& lname){ return name_to_node.at(lname); });
      const NodeSet leaves{leaves_range.begin(), leaves_range.end()};
      std::cout << "computing diversity score of leaves " << leaves << '\n';
      const auto score = test(options, "-m") ? 
        pd_score_ct(N, leaves, UtilityFunctors()) :
        pd_score_classic(N, leaves, UtilityFunctors());
      std::cout << "score = "<<score<<'\n';
    } else {
      const size_t k = parse_k(N.num_leaves(), options[""][1]);
      //using T = decltype(pd_score_ct<MyNetwork, NodeSet, UtilityFunctors>);
      std::cout << "computing optimal diversity score obtainable with " << k << " leaves\n";
      const auto before = mstd::get_time();
      const auto [sol, score] = test(options, "-m") ? 
        optimize_diversity_brute_force(N, k, UtilityFunctors(), _pd_score_ct{}) :
        optimize_diversity_brute_force(N, k, UtilityFunctors(), _pd_score_classic{});
      const auto elapsed = mstd::ms_between(before, mstd::get_time());
      std::cout << "solution with diversity "<<score<<": " << (sol | rv::transform([&](const NodeDesc u){ return N[u].label();})) <<"\t("<<elapsed<<"ms)\n";
    }
  }
}



