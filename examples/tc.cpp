
#include "io/io.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/net_sample.hpp"
#include "utils/mul_wrapper.hpp" // treat networks as multi-labeled trees
//#include "utils/tc_preprocess.hpp" // preprocessing

#include "utils/containment.hpp"

using namespace PT;
using namespace std::literals;

mstd::OptionMap options;
void parse_options(const int argc, const char** argv)
{
  mstd::OptionDesc description;
  description["-v"] = {0,0};
  description["-r"] = {3,3};
  description[""] = {0,2};
  const std::string help_message(std::string(argv[0]) + " [args] <file1> [files...]\n\
      \t where either\n\
      \t (a) file1 describes a network N and files... describe trees Ti or\n\
      \t (b) file1 contains a network N in the first line and trees Ti in each following line\n\n\
      other arguments:\n\
      \t -r <x> <y> <z>\t\trandomize a tree with x internal nodes and y leaves and add z additional edges, then check containment of the tree in the network in file1\n\
      \t -t\t\tlist all trees contained in N\n\
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

auto create_net_and_tree() {
  using TreeBuilder = LeafReplacementTreeBuilder<MyTree, RandomDecider>;
  using NetBuilder = TreeBasedNetworkBuilder<MyNet, RandomDecider>;

  NetAndTree result;

  const int num_internals = std::stoi(options["-r"][0]);
  const int num_leaves = std::stoi(options["-r"][1]);
  const int num_new_edges = std::stoi(options["-r"][2]);

  std::cout << "generating network with "<<num_leaves<<" leaves, "<<num_internals<<" internal nodes and "<< (num_leaves + num_internals - 1) + num_new_edges<<" edges\n";
  TreeBuilder{result.second, ReducedNodeNums{num_internals, 0, num_leaves, 0.0f}}.build_phylogeny();

  std::cout << "rolled tree:\n"<<result.second<<"\n";

  std::cout << "network type is "<<mstd::type_name<MyNet>()<<"\n";
  std::cout << "tree has label ? "<<PT::HasDataType<Ex_node_label, MyTree><<"\n";
  std::cout << "net has label ? "<<PT::HasDataType<Ex_node_label, MyNet><<"\n";
  std::cout << "tree label type is "<<mstd::type_name<PT::DataTypeOf<Ex_node_label, MyTree>>()<<"\n";
  
  std::cout << "copying tree...\n";
  result.first = result.second;
  
  std::cout << "adding "<<num_new_edges<<" new edges...\n";
  NetBuilder{result.first, ReducedNodeNums{num_internals, 0, num_leaves, 0.0f}}.add_random_edges(num_new_edges);

  return result;
}

MyNet read_network(auto&& in){
  try{
    return parse_newick<MyNet>(in);
  } catch(const std::exception& err){
    cfail("could not read network: "sv + err.what() + "\n"sv);
  }
}

NetPair read_networks() {
  const auto& input_files = options[""];
  if(!input_files.empty()) {
    std::ifstream in0(input_files[0]);
    return (input_files.size() == 1) ?
      NetPair{read_network(in0), read_network(in0)} :
      NetPair{read_network(in0), read_network(std::ifstream{input_files[1]})};
  } else throw std::invalid_argument("no input files");
}

NetAndTree read_net_and_tree() {
  NetPair nets = read_networks();
  // choose which index corresponds to the host and which to the guest (we try to embed guest into host)
  // if we've been given 2 trees, the first is considered the host, otherwise, the network is considered the host :)
  const bool host_net_index = (nets[0].is_tree() && !nets[1].is_tree());
  const bool guest_net_index = 1 - host_net_index;
  return NetAndTree(std::move(nets[host_net_index]), std::move(nets[guest_net_index]));
}

int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  auto NT_tuple = mstd::test(options, "-r") ?
    create_net_and_tree() :
    read_net_and_tree();

  MyNet& N = NT_tuple.first;
  MyTree& T = NT_tuple.second;

  if(mstd::test(options, "-v")) {
    std::cout << "N:\n" << N << "\n";
    std::cout << get_extended_newick(N) << '\n';
    std::cout << "T:\n" << T << '\n';
    std::cout << get_extended_newick(T)<<'\n';
  }

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
  } else std::cout << "sorry, can't check network-network containment yet...\n";
}
