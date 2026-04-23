
#include "io/io.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/net_sample.hpp"
#include "utils/mul_wrapper.hpp" // treat networks as multi-labeled trees
//#include "utils/tc_preprocess.hpp" // preprocessing

#include "utils/containment.hpp"
#include "utils/switching_iter.hpp"
#include "utils/isomorphism.hpp"

using namespace PT;
using namespace std::literals;

mstd::OptionMap options;
void parse_options(const int argc, const char** argv)
{
  mstd::OptionDesc description;
  description["-v"] = {0,0};
  description["-t"] = {0,0};
  description["-s"] = {0,0};
  description["-r"] = {3,3};
  description[""] = {0,2};
  const std::string help_message(std::string(argv[0]) + " [args] <file1> [files...]\n\
      \t where either\n\
      \t (a) file1 describes a network N and files... describe trees Ti or\n\
      \t (b) file1 contains a network N in the first line and trees Ti in each following line\n\n\
      other arguments:\n\
      \t -r <x> <y>\trandomize a tree with x internal nodes and y leaves, then check containment of the tree in the network in file1\n\
      \t -t\t\tlist all non-isomorphic trees contained in N\n\
      \t -s\t\tlist all switchings of N (may contain unlabelled leaves)\n\
      ");

  mstd::parse_options(argc, argv, description, help_message, options);

  if(mstd::test(options, "-r") + mstd::test(options, "-t") + mstd::test(options, "-s") > 1)
    cfail("only one of -r, -t, -s may be specified\n");


  if(mstd::test(options, "-r")){
    const auto r_vec = options.at("-r");
    if(stoi(r_vec[0]) == 0) cfail("cannot construct tree without nodes\n");
    if(stoi(r_vec[1]) <= stoi(r_vec[0])) cfail("cannot construct tree with "sv + std::to_string(r_vec[0]) + " internal nodes & "sv + std::to_string(r_vec[1]) + " leaves\n"sv);
  } else {
    if(options[""].empty()) cfail(help_message);
    for(const std::string& filename: options[""])
      if(!file_exists(filename)) cfail(filename + " cannot be opened for reading\n"sv);
  }
}

using MyTree = DefaultLabeledTree<>;
using MyNet = CompatibleNetwork<MyTree>;

using NetPair = std::array<MyNet, 2>;
using NetAndTree = std::pair<MyNet, MyTree>;

/*
// check containment for a Network or MUL-Tree
bool check_display(MyNet& N, MyTree& T)
{
  if(N.is_multi_labeled()){
    std::cout << "running MUL-mapper"<<std::endl;
    MULNetworkMapper<Network> mapper(N, T);
    std::cout << "constructed mapper, now doing the hard work" << std::endl;
    return mapper.verify_display();
  } else {
    std::cout << "running network mapper"<<std::endl;
    NetworkMapper<Network> mapper(N, T);
    std::cout << "constructed mapper, now doing the hard work" << std::endl;
    return mapper.verify_display();
  }
}
*/

struct RandomDecider {

  template<class T>
  T operator()(const T& lower, const T& upper) const { return lower + mstd::throw_die(upper - lower + 1); }
};

MyTree create_tree() {
  using TreeBuilder = LeafReplacementTreeBuilder<MyTree, RandomDecider>;
  
  MyTree result;

  const int num_internals = std::stoi(options["-r"][0]);
  const int num_leaves = std::stoi(options["-r"][1]);

  std::cout << "generating tree with "<<num_leaves<<" leaves, "<<num_internals<<" internal nodes and "<< (num_leaves + num_internals - 1) <<" edges\n";
  TreeBuilder{result, ReducedNodeNums{num_internals, 0, num_leaves, 0.0f}}.build_phylogeny();

  std::cout << "rolled tree:\n"<<result<<"\n";

  std::cout << "tree type is "<<mstd::type_name<MyNet>()<<"\n";
  std::cout << "tree has label ? "<<PT::HasDataType<Ex_node_label, MyTree><<"\n";
  std::cout << "tree label type is "<<mstd::type_name<PT::DataTypeOf<Ex_node_label, MyTree>>()<<"\n";
  
  return result;
}

template<StrictPhylogenyType Phylo>
Phylo read_network(auto&& in){
  try{
    return parse_newick<Phylo>(in);
  } catch(const std::exception& err){
    cfail("could not read phylogeny: "sv + err.what() + "\n"sv);
  }
}

int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  const MyNet N = read_network<MyNet>(std::ifstream{options[""][0]});

  const NodeSet cycle = N.get_cycle();
  if(not cycle.empty()) cfail("Network contains a cycle on NodeSet " + std::to_string(cycle));

  if(mstd::test(options, "-v"))
    std::cout << "N:\n" << N << '\n' << get_extended_newick(N) << '\n' << N.get_summary();

  if(mstd::test(options, "-s")) {
    // list all switchings of N
    using Switchings = SwitchingFactory<MyNet>;
    for(auto sw: Switchings{N}) {
      auto traversal = N.edges(sw); // TODO: turn this into a pre-order to cause less confusion for the poor edge-emplacer juggling the root
      const MyTree T(traversal, DefaultDataExtracter<MyNet>{});
      std::cout << get_extended_newick(T) << '\n';
    }
  } else if(mstd::test(options, "-t")) {
    using Switchings = SwitchingFactory<MyNet>;

    // we'll store all previous trees in order to check isomorphism
    std::vector<MyTree> previous_trees;
    previous_trees.reserve(1 << N.reticulation_number());

    const NodeVec leaves = N.leaves().to_container();
    for(auto sw: Switchings{N}) {
      auto traversal = N.edges_above(leaves, sw);
      append(previous_trees, traversal, DefaultDataExtracter<MyNet>{});
      MyTree& T = previous_trees.back();
      NodeVec deg2 = T.nodes_with_preorder([](const NodeDesc x){ return MyTree::is_suppressible(x); }).to_container();
      while(not deg2.empty())
        T.contract_up(mstd::value_pop_back(deg2));
      // check isomorphism
      size_t i = previous_trees.size() - 1;
      while(i > 0) {
        --i;
        IsomorphismMapper<MyTree, MyTree> M(previous_trees[i], T, FLAG_MAP_LEAF_LABELS);
        if(M.check_isomorph()) break;
      }
#warning "TODO: check isomorphism against previous trees"
      if(i == 0) std::cout << get_extended_newick(T) << '\n';
    }
    // list all contained trees of N
  } else {
    const MyTree T = mstd::test(options, "-r") ? create_tree() :
      ((options[""].size() > 1) ? read_network<MyTree>(std::ifstream{options[""][1]}) : MyTree{});

    if(mstd::test(options, "-v"))
      std::cout << "T:\n" << T << '\n' << get_extended_newick(T)<<'\n' << T.get_summary();

    std::cout << "\n\n starting the containment engine...\n\n";
    if(T.is_tree()) {
      if(N.is_tree()){
        TreeInTreeContainment tc(N, T);
        if(tc.displayed())
          std::cout << "displayed\n"; // by subtrees rooted at: "<< tc.who_displays(T.root()).front() << "\n";
        else std::cout << "not displayed\n";
      } else {
        TreeInNetContainment tc(N, T);
        if(tc.displayed())
          std::cout << "displayed\n"; // by subtrees rooted at: "<< tc.who_displays(T.root()).front() << "\n";
        else std::cout << "not displayed\n";
      }
    } else throw mstd::Unimplemented{"sorry, can't check network-network containment yet...\n"};
  }
}


