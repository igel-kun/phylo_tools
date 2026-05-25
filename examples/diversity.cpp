


#ifdef DFSCORO

#include <ranges>
#include <utility>
#include <array>

#include "utils/benchmark.hpp"
#include "utils/command_line.hpp"
#include "utils/token.hpp"
#include "utils/generic_data.hpp" // for try_reading

#include "io/newick.hpp" // to read newick
#include "io/features.hpp" // to read feature matrices

#include "utils/tree.hpp"
#include "utils/network.hpp"
#include "utils/diversity.hpp"
#include "utils/feature_diversity.hpp"
#include "utils/dominators.hpp"

#include "utils/net_generator.hpp"

namespace ra = std::ranges;
namespace rv = std::views;

using namespace PT;
using namespace std::literals;

using Weight = float;
using Probability = float;

// no node data, but edges are annotated with p, w, and gamma
using EdgeData = pd_edge_data<Weight, Probability>;

// FeatureMap maps each leaf label to a feature-collection containing all features of that leaf
// NOTE: we'll support only rational features here, since we may have binary, integer, or rational features and they are all subsumed by the latter
//using FeatureMap = HashMap<std::string, PT::DefaultFeatureCollection>;
using FeatureMap = HashMap<std::string, PT::FeatureCollection<double>>;

using MyNetwork = DefaultLabeledNetwork<void, EdgeData>;
using MyNode = typename MyNetwork::Node;
using MyEdge = typename MyNetwork::Edge;
using SolutionAccu = mstd::SolutionAccumulator<NodeVec, Weight>;
using NameToNode = std::unordered_map<std::string, NodeDesc>;

// the nodes of the LSA-tree know their corresponding node in N; the edges know the weight of the corresponding path in N
using LSATree = Tree<vecS, NodeDesc, Weight>;


struct config_t {
  bool verbose = false;
  bool all_leaves = false;
  bool optimize = false;
  bool compute_for_leafset = false;
  bool use_clever = false;
  bool verify = false;
  bool greedy_heuristic = false;
  bool feature_diversity = false;
  bool shapeley_modified = false;
  bool shapeley_index_of_score = false;
  bool score_network_diversity = false;
  bool score_network_fair_proportion = false;
  bool score_subnet_diversity = false;
  bool score_tree_extract = false;
  unsigned char score, summary, extraction;
  uint32_t num_solutions;

  config_t() = default;

  config_t(const mstd::OptionMap& o):
    verbose{test(o, "-v")},
    all_leaves{test(o, "-s")},
    optimize{o.at("").size() >= 2},
    compute_for_leafset{test(o, "-l")},
    use_clever{test(o, "-c") or test(o,"-cv")},
    verify{test(o, "-cv")},
    greedy_heuristic{test(o, "-g")},
    feature_diversity{test(o, "-f")},
    shapeley_modified{test(o, "-ms")},
    shapeley_index_of_score{test(o, "-si")},
    score_network_diversity{test(o, "-nd")},
    score_network_fair_proportion{test(o, "-nf")},
    score_subnet_diversity{test(o, "-ns")},
    score_tree_extract{test(o, "-t")},
    num_solutions{test(o, "-S") ? stoX<uint32_t>(o.at("-S")[0]) : 1u}
  {
    if(score_tree_extract) {
      const auto tree_config = mstd::tokenize(o.at("-t")[0], ',').template to_container<std::vector<std::string_view>>();
      score = std::stoi(tree_config[0]);
      summary = std::stoi(tree_config[1]);
      extraction = std::stoi(tree_config[2]);
    }
  }
  config_t& operator=(config_t&&) noexcept = default;
  config_t& operator=(const mstd::OptionMap& o) { config_t tmp(o); *this = std::move(tmp); return *this; }
} conf;

NameToNode name_to_node;

mstd::OptionMap options;

void parse_options(const int argc, const char** argv) {
  mstd::OptionDesc description;
  description["-v"] = {0,0};
  description["-f"] = {0,0};
  description["-si"] = {0,0};
  description["-S"] = {1,1};
  description["-s"] = {0,0};
  description["-l"] = {1,1};
  description["-g"] = {0,0};
  description["-c"] = {0,0};
  description["-cv"] = {0,0};
  description["-ms"] = {0,0};
  description["-nd"] = {0,0};
  description["-nf"] = {0,0};
  description["-ns"] = {0,0};
  description["-t"] = {1,1};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " [FLAGS] <file> <<k> | -l <leaf list> | -s>\n\
Let N be the network described in file in extended Newick format such that\n\
- each leaf is annotated with its taxon name and\n\
- each edge uv is annotated with 1-2 floating-point values,\n\
     the first indicating the weight, the (possible) second indicating its inheritence probability p, if any.\n\
This programm can compute/optimize various diversity scores of N:\n\
- compute the diversity of a given list of leaves (use -l <leaf list>)\n\
- compute the diversity of each single leaf seperately (use -s)\n\
- find a set of k leaves maximizing the diversity score (provide k)\n\
     NOTE: add % to k in order to express a number relative to the total number of leaves (e.g. 25%).\n\
This program can also compute the feature diversity of a matrix M given in <file> (use -f).\n\
\n\
We consider three types of diversity scores:\n\
\t(A) direct scores working with the network:\n\
\t\t Network Diversity [vIJSSW'25a], Network Fair Proportion, Subnet Diversity\n\
\t(B) scores that summarize a tree-diversity measure applied to a set of trees extracted from the network.\n\
\t\t tree-scores:  (1) Tree Diversity [PG'05, Steel'05], (2) Fair Proportion, (3) Shapley (=Fair Prop. for singletons)\n\
\t\t summarize by: (1) Weighted Average, (2) Maximum Likelihood\n\
\t\t extraction:   (1) Lowest Stable Ancestor Tree, (2) Switching Tree \n\
\t\t\t\tNOTE: the LSA-tree is unique, but we need to summarize the paths represented by edges of the LSA-tree\n\
\t(C) Shapley Index of any of the previous scores\n\
\n\
GENERAL FLAGS:\n\
\t-v\tverbose output, prints network and mentions scores\n\
\t-h\tprint this help screen and exit\n\
\t-f\tinstead of phylo-diversity, compute feature-diversity of the features given as a matrix in <file>\n\
\t-si\tinstead of the selected diversity score, use the Shapley-Index for that score (score type (C))\n\
\t-S i\tnumber i of highest-scoring solutions to return (not compatible with -l, -s)\n\
\t-s\tcompute the diversity score for all singleton-sets (equal to -S n with k=1; incompatible with -S, -l, k)\n\
\t-l\tcompute the diversity score for the given list of leaves (comma separated list of taxa, no spaces)\n\
\t-g\tinstead of finding the best size-k set of taxa, repeatedly pick the best available taxon ('greedy')\n\
\t-c\twhere available, use a more clever implementation (f.ex. use dynamic programming with respect to level\n\
\t\t\tfor computing k leaves maximizing the weighted average diversity of a switching)\n\
\t\t\tNOTE: -g -c might make sense to compute the singleton scores more cleverly, where available\n\
\t-cv\tlike -c, but verifies against a brute-force implementation\n\
\t-ms\twhenever a Shapley-calculation occurs, use the Modified Shapley-Index [FJ'15] instead\n\
\n\
DIRECT NETWORK DIVERSITIES (score type (A)):\n\
\t-nd\tNetwork Diversity\n\
\t-nf\tNetwork Fair Proportion\n\
\t-ns\tNetwork Subnet Diversity\n\
\n\
TREE-DIVERSITY BASED METHODS (score type (B)):\n\
\t-t score,summarize,extract\tdiversity measure according to the above scheme\n\
\t\t(f.ex. -t 1,1,2 = use (1) tree diversity, summarize by (1) weighted avg of (2) all switchings)\n\
\t\t(f.ex. -t 2,2,1 = use (2) fair proportion on the (1) LSA-tree, whose branch-len's are the (2) max. likelihood paths in the network)\n\
\t\t(f.ex. -t 1,2,2 = use (1) tree diversity on the (2) switching of the network with (2) highest probability)\n\
\n\
WHITEPAPERS:\n\
[PG'05]     \thttps://doi.org/10.1371/journal.pgen.0010071\n\
[Steel'05]  \thttps://doi.org/10.1080/10635150590947023\n\
[FJ'15]     \thttps://doi.org/10.1007/s00285-014-0853-0\n\
[FW'18]     \thttps://doi.org/10.1016/j.mbs.2018.02.005\n\
[vIJSSW'25a]\tto appear in Proc. Recomb CG'25\n\
[vIJSSW'25b]\thttps://doi.org/10.4230/LIPIcs.WABI.2025.15\n");

  mstd::parse_options(argc, argv, description, help_message, options);

  const bool provided_k = (options[""].size() >= 2);

  if((test(options, "-l") + test(options, "-s") + provided_k) != 1)
    cfail("This program operates in one of the following 3 modes. You have to select exactly one of them:\n\
\t1. compute the diversity of a given list of leaves (-l <list>)\n\
\t2. compute one (or more) size-k set(s) of leaves maximizing the diversity\n\
\t\t(provide k on the command line, choose the number of sets using -S)\n\
\t3. output the diversity score of each leaf seperately\n");
  
  if(test(options, "-l") + test(options, "-s") + provided_k > 1)
    cfail("providing k, -l, and -s are mutually exclusive\n");


  if((test(options, "-l") or test(options, "-s")) and test(options, "-g"))
    cfail("The greedy heuristic (-g) only makes sense when finding the best size-k solution, it's incompatible with -l and -s.");

  if(test(options, "-nd") + test(options, "-nf") + test(options, "-ns") + test(options, "-t") + test(options, "-f") != 1)
    cfail("Please chose exactly one diversity score among {-nd,-nf,-ns,-t,-f}. You can add -si for the Shapley-Index of that score.\n");

  if((test(options, "-g") or test(options, "-c")) and test(options, "-cv"))
    cfail("-cv is incompatible with -c and -g\n");

  if(test(options, "-t")) {
    size_t count = 0;
    for(const auto tok: mstd::tokenize(options["-t"][0], ',')) {
      ++count;
      if((tok != "1") and (tok != "2") and (tok != "3"))
        cfail("subargument of -t out of range: "+tok+'\n');
    }
    if(count != 3)
      cfail("-t " + options["-t"][0] + " has " + std::to_string(count) + " subarguments, but exactly 3 are expected: score,summarize,extract\n");
  }

  if(not file_exists(options[""].front()))
    cfail(std::string{"couldn't open file "} + options[""].front());

  conf = options;
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

// overwrite the emplacement-helper set_label function to store the Label-->NodeDesc mapping in our global name_to_node
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


auto read_features(const std::string& filename) {
  std::ifstream in{filename};
  FeatureMap feature_map;
  PT::read_features(in,
      [&](const std::string& s) -> typename FeatureMap::mapped_type& { return feature_map[s]; },
      std::vector{1}); // skip column 1
  return feature_map;
}


void feature_diversity_subsystem() {
  std::cout << "parsing features from "<<options[""][0]<<"...\n";
  const auto feature_map = read_features(options[""][0]);
  DEBUG2(std::cout << "read feature map:\n" << feature_map << '\n');

  if(conf.all_leaves) {
    std::cout << "feature diversity score of each leaf:\n";
    for(const auto& [label, collection]: feature_map)
      std::cout << label <<": " << collection<<'\n';
  } else if(conf.compute_for_leafset) {
    const NameVec leaf_names = parse_leaves(options["-l"][0]);
    std::cout << "computing diversity score of leaves " << leaf_names << '\n';
    auto selected_features = feature_map | std::ranges::views::filter([&](const auto& x){return test(leaf_names, x.first);})
                                         | std::ranges::views::transform([](const auto& x)->auto& { return x.second; });
    const auto score = feature_diversity{}(selected_features);
    std::cout << "score = "<<score<<'\n';
  } else {
    const size_t k = parse_k(feature_map.size(), options[""][1]);
    const auto before = mstd::get_time();
    const auto solutions = optimize_feature_diversity(k, feature_map, conf.num_solutions).solutions;
    const auto elapsed = mstd::ms_between(before, mstd::get_time());
    std::cout << std::fixed << std::setprecision(0) << "("<<elapsed<<"ms)\n";

    for(const auto& [feats, score]: solutions)
      std::cout << "maximum feature-diversity = " << score << ":\n" << mstd::Linewise{feats, false} << '\n';
  }
}

// return the average length of a path in N, weighted by their probability
auto get_expected_path_length(const NodeDesc start, const NodeDesc finish) {
  return pd_expected_path_lengths<MyNetwork>{start}.length_to(finish).second;
}

// return the length of a most likely path in N
auto get_ML_path_length(const NodeDesc start, const NodeDesc finish) {
  return pd_ML_path_lengths<MyNetwork>{start}.length_to(finish).second;
}

auto get_lsa_tree(const MyNetwork& N, const bool expected_lengths, NodeTranslation& old_to_new) {
  // step 1: get the dominator tree topology with links to the nodes in N
  LSATree T = NaiveDominatorOracle<MyNetwork>(N).template make_dominator_tree<LSATree>(old_to_new, Ex_node_data{}, mstd::IdentityFunction<NodeDesc>{});
  // step 2: populate the branch-lengths
  for(auto uv: T.edges()) {
    const auto [u, v] = uv.as_pair();
    const NodeDesc u_in_N = LSATree::data(u);
    const NodeDesc v_in_N = LSATree::data(v);
    uv.data() = expected_lengths ? 
      get_expected_path_length(u_in_N, v_in_N):
      get_ML_path_length(u_in_N, v_in_N);
  }
  return T;
}

// NOTE: if diversity is computed by a proxy (like the LSA-tree or the ML-tree), then the solution will
//        contain NodeDesc's for the proxy, so we'll have to translate those back to original nodes
template<class SolAccu, class Translate>
  requires (not NodeContainerType<SolAccu>)
auto translate_leaves(SolAccu&& accu, Translate&& translate) {
  using Result = std::remove_cvref_t<SolAccu>;
  if constexpr (not mstd::is_arithmetic_v<SolAccu>) {
    for(auto& [sol, score]: accu.solutions)
      for(auto& v: sol)
        v = mstd::access(translate, v);
  }
  return Result(std::forward<SolAccu>(accu));
}
template<NodeIterableType Nodes, class Translate>
auto translate_leaves(Nodes&& nodes, Translate&& translate) {
  NodeVec result;
  if constexpr (mstd::HasReserve<Nodes>)
    result.reserve(nodes.size());
  for(const NodeDesc x: nodes)
    mstd::append(result, mstd::access(translate, x));
  return result;
}

// per default, the original node for a node is stored as NodeData
template<StrictPhylogenyType Net, class SolAccu>
  requires (Net::has_node_data and std::is_convertible_v<typename Net::NodeData, NodeDesc>)
auto translate_leaves(SolAccu&& solutions) {
  return translate_leaves(std::forward<SolAccu>(solutions), [](const NodeDesc v){ return Net::data(v); });
}

// we'll generate the LSA-tree and stuff that into the PD-measure
// NOTE: we'll have to be cautious, since the nodes of the LSA-tree are NOT the nodes of the network
//        this means that we'll have to translate both the input leaf-set and the output leaf-sets
template<class First, class... Args>
auto LSA_based_diversity(const MyNetwork& N, First&& first, Args&&... args) {
  const bool expected_weights = conf.summary == 1;
  NodeTranslation net_to_lsa;
  const LSATree lsa_tree = get_lsa_tree(N, expected_weights, net_to_lsa);
  DEBUG3(std::cout << "constructed LSA-tree:\n" << ExtendedDisplay(lsa_tree) << '\n' << lsa_tree.get_summary(true) << '\n';);
  if(conf.score == 1) { // tree diversity
    if(conf.verbose) std::cout << "SCORE: tree-diversity of LSA-tree with " << (expected_weights ? "expected"sv : "max-likelihood"sv) << " weights\n";
    return translate_leaves<LSATree>(pd_tree_diversity<LSATree>()(
        lsa_tree, translate_leaves(std::forward<First>(first), net_to_lsa), std::forward<Args>(args)...));
  } else if(conf.score == 2) { // Fair-Proportion index
    if(conf.verbose) std::cout << "SCORE: Fair-Proportion Index of LSA-tree with " << (expected_weights ? "expected"sv : "max-likelihood"sv) << " weights\n";
    return translate_leaves<LSATree>(pd_tree_fair_proportion<LSATree>()(
        lsa_tree, translate_leaves(std::forward<First>(first), net_to_lsa), std::forward<Args>(args)...));
    return translate_leaves<LSATree>(pd_fair_proportion<LSATree>()(
        lsa_tree, translate_leaves(std::forward<First>(first), net_to_lsa), std::forward<Args>(args)...));
  } else if(conf.score == 3) { // Shapley index
    if(conf.verbose) std::cout << "SCORE: Shapley Index of LSA-tree with " << (expected_weights ? "expected"sv : "max-likelihood"sv) << " weights\n";
    return translate_leaves<LSATree>(pd_tree_shapeley<LSATree>()(
        lsa_tree, translate_leaves(std::forward<First>(first), net_to_lsa), std::forward<Args>(args)...));
  }
  throw mstd::Unimplemented{"Selected LSA-tree-based diversity measure"};
}

template<class First, class... Args>
auto switching_based_diversity(const MyNetwork& N, First&& first, Args&&... args) {
  if(conf.summary == 1) { // expected value for a switching
    switch(conf.score) {
      case 1:
        if(conf.verbose) std::cout << "SCORE: expected tree-diversity of any switching\n";
        if(N.is_tree()) { // if the network is a tree, there is no need to run the AveragePD engine
          return pd_tree_diversity<MyNetwork>()(N, std::forward<First>(first), std::forward<Args>(args)...);
        } else if(conf.use_clever) {
          return pd_average_tree_DP<MyNetwork>()(N, std::forward<First>(first), std::forward<Args>(args)...);
        } else return pd_average_tree<MyNetwork>()(N, std::forward<First>(first), std::forward<Args>(args)...);

      case 2:
        if(conf.verbose) std::cout << "SCORE: expected Fair-Proportion Index of any switching\n";
        return pd_average_fair_proportion<MyNetwork>()(N, std::forward<First>(first), std::forward<Args>(args)...);

      case 3:
        if(conf.verbose) std::cout << "SCORE: expected Shapley Index of any switching\n";
        return pd_average_shapeley<MyNetwork>()(N, std::forward<First>(first), std::forward<Args>(args)...);
    }
  } else if(conf.summary == 2) { // value for the most likely switching
    using MLSwitching = Tree<vecS, NodeDesc, Weight>;
    NodeTranslation net_to_ml;
    const NodeVec leaves = N.leaves().to_container();
    // make the ML-tree from the maximum-probability switching edgelist
    // NOTE: the diversity measures require all leaves to be selectable, so we remove all dangling leaves from the switching
    MLSwitching ml_switching(pd_ML<MyNetwork>().get_ML_switching(leaves).get_all_edges_above(leaves), net_to_ml,
              Ex_node_data{}, mstd::IdentityFunction<NodeDesc>{}, // nodes store their original node in the network
              Ex_edge_data{}, pd_score_util_w<EdgeDataOf<MyNetwork>>()); // edges store the weight of the original edge
    DEBUG3(std::cout << "constructed ML-switching:\n" << ExtendedDisplay(ml_switching) << '\n');
    switch(conf.score){
      case 1:
        std::cout << "SCORE: tree-diversity of the most probable switching\n";
        return translate_leaves<MLSwitching>(pd_tree_diversity<MLSwitching>()(
              ml_switching, translate_leaves(std::forward<First>(first), net_to_ml), std::forward<Args>(args)...));

      case 2:
        std::cout << "SCORE: Fair-Proportion Index of the most probable switching\n";
        return translate_leaves<MLSwitching>(pd_fair_proportion<MLSwitching>()(
              ml_switching, translate_leaves(std::forward<First>(first), net_to_ml), std::forward<Args>(args)...));
      
      case 3:
        std::cout << "SCORE: Shapley Index of the most probable switching\n";
        return translate_leaves<MLSwitching>(pd_tree_shapeley<MLSwitching>()(
              ml_switching, translate_leaves(std::forward<First>(first), net_to_ml), std::forward<Args>(args)...));
    }
  }
  throw mstd::Unimplemented{"Selected switching-based diversity measure"};
}


template<class... Args>
auto pd_engine(const MyNetwork& N, Args&&... args) {
  // select DP Engine
  if(conf.score_network_diversity) return pd_network_diversity<MyNetwork>()(N, std::forward<Args>(args)...);
  else if(conf.score_network_fair_proportion) return pd_fair_proportion<MyNetwork>()(N, std::forward<Args>(args)...);
  else if(conf.score_subnet_diversity) return pd_subnet_diversity<MyNetwork>()(N, std::forward<Args>(args)...);
  else if(conf.score_tree_extract) {
    if(conf.extraction == 1) { // ------------------ extract LSA-tree ---------------------
      return LSA_based_diversity(N, std::forward<Args>(args)...);
    } else if(conf.extraction == 2) { // ----------- extract switching ------------------
      return switching_based_diversity(N, std::forward<Args>(args)...);
    }
  }
  throw mstd::Unimplemented{"Selected diversity measure"};
}

void phylo_diversity_subsystem() {
  std::cout << "reading network...\n";
  const MyNetwork N(read_network(options[""][0]));

  if(conf.verbose) {
    std::cout << "N ("<<N.num_nodes()<<" nodes, "<<N.num_edges()<<" edges -> reti num:" << N.num_edges()-N.num_nodes()+1<<"):" << std::endl;
    std::cout << ExtendedDisplay(N) << '\n' << N.get_summary(true) << '\n';
  }

  if(conf.all_leaves) {
    // output diversity score of all singletons
    std::cout << "computing diversity score of each leaf ("<<N.leaves()<<"):\n";
    for(const NodeDesc x: N.leaves())
      std::cout << MyNetwork::label(x) << ":\t" << pd_engine(N, NodeSingleton{x}) << '\n';
  } else if(conf.compute_for_leafset) {
    // output diversity score of the given leaf-set
    const NameVec leaf_names = parse_leaves(options["-l"][0]);
    const auto selected_leaves = leaf_names | rv::transform([&](const std::string& lname){ return name_to_node.at(lname); });
    const NodeSet leaves{selected_leaves.begin(), selected_leaves.end()};
    std::cout << "computing diversity score of leaves " << leaf_names << '\n';
    const auto before = mstd::get_time();
    const auto score = pd_engine(N, leaves);
    const auto elapsed = mstd::ms_between(before, mstd::get_time());
    const auto default_precision{std::cout.precision()};
    std::cout << std::fixed << std::setprecision(0) << "("<<elapsed<<"ms)\n" << std::setprecision(default_precision);
    std::cout << "score = "<<score<<'\n';
  } else {
    const size_t k = parse_k(N.num_leaves(), options[""][1]);
    std::cout << "computing optimal diversity score obtainable with " << k << " leaves\n";
    const auto before = mstd::get_time();
    const auto solutions = pd_engine(N, k, conf.num_solutions).solutions;
    const auto elapsed = mstd::ms_between(before, mstd::get_time());
    const auto default_precision{std::cout.precision()};
    std::cout << std::fixed << std::setprecision(0) << "("<<elapsed<<"ms)\n" << std::setprecision(default_precision);
    // 
    if(conf.verify) { // verify against brute-force
      DEBUG4(std::cout << "let's check solutions against brute-force...\n");

      DEBUG4(std::cout << "N is still:\n" << ExtendedDisplay(N) << '\n' << N.get_summary(true) << '\n');
      for(const auto& sol: solutions) {
        const double score = sol.second;
        const double bf_score = pd_average_tree<MyNetwork>{}(N, sol.first);
        if((bf_score < score - 0.01) or (bf_score > score + 0.01))
          cfail(std::string("Uh oh, solution ") + std::to_string(sol.first) + " scoring " + std::to_string(score) +
              " vs. " + std::to_string(bf_score) + " by brute-force\n");
      }
    }
    if(not solutions.empty()) {
      for(const auto& [sol, score]: solutions) {
        std::cout << std::fixed << std::setprecision(2)<< "solution with diversity "<<score<<": " << (sol | rv::transform([&](const NodeDesc u){ return N[u].label();})) <<"\n";
      }
    } else std::cout << "no solution\n";
  }
}


int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  // how many solutions to return
  if(conf.greedy_heuristic) throw mstd::Unimplemented{"greedy heuristics"};
  if(conf.shapeley_index_of_score) throw mstd::Unimplemented{"shapeley-index modifier"};
  if(conf.shapeley_modified) throw mstd::Unimplemented{"modified shapeley"};

  if(conf.feature_diversity) {
    feature_diversity_subsystem();
  } else phylo_diversity_subsystem();
}

#else
#error "diversity program needs to be compiled with -DDFSCORO"
#endif


