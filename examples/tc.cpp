
#include "io/io.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/net_gen.hpp"
#include "utils/mul_wrapper.hpp" // treat networks as multi-labeled trees
//#include "utils/tc_preprocess.hpp" // preprocessing

#include "utils/containment.hpp"

using namespace PT;

OptionMap options;
void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description["-r"] = {3,3};
  description[""] = {0,2};
  const std::string help_message(std::string(argv[0]) + " <file1> [file2]\n\
      \tfile1 and file2 describe two networks (either file1 contains 2 lines of extended newick or both file1 and file2 describe a network in extended newick or edgelist format)\n\
      \tUnless the first network is a tree and the second is not, we try to embed the second network in the first.\n\
      \n" + std::string(argv[0]) + " -r <x> <y> <z>\n\
      \trandomize a tree with x internal nodes + y leaves and add z additional edges, then check containment of the tree in the network\n");

  parse_options(argc, argv, description, help_message, options);

  if(test(options, "-r")){
    const auto r_vec = options.at("-r");
    if(stoi(r_vec[0]) == 0) {
      std::cerr << "cannot construct tree with "<<r_vec[0]<<" nodes\n";
      exit(EXIT_FAILURE);
    }
    if(stoi(r_vec[1]) <= stoi(r_vec[0])) {
      std::cerr << "cannot construct tree with "<<r_vec[0]<<" internal nodes & "<<r_vec[1]<<" leaves\n";
      exit(EXIT_FAILURE);
    }
  } else {
    if(options[""].empty()) {
      std::cerr << help_message << std::endl;
      exit(EXIT_FAILURE);
    }
    for(const std::string& filename: options[""])
      if(!file_exists(filename)) {
        std::cerr << filename << " cannot be opened for reading" << std::endl;
        exit(EXIT_FAILURE);
      }
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


NetAndTree create_net_and_tree() {
  NetAndTree result;

  const uint32_t num_internals = std::stoi(options["-r"][0]);
  const uint32_t num_leaves = std::stoi(options["-r"][1]);
  const uint32_t num_new_edges = std::stoi(options["-r"][2]);

  std::cout << "generating network with "<<num_leaves<<" leaves, "<<num_internals<<" internal nodes and "<< (num_leaves + num_internals - 1) + num_new_edges<<" edges\n";
  generate_random_tree(result.second, num_internals, num_leaves);
  generate_leaf_labels(result.second);

  std::cout << "rolled tree:\n"<<result.second<<"\n";

  std::cout << "network type is "<<std::type_name<MyNet>()<<"\n";
  std::cout << "tree has label ? "<<PT::HasDataType<Ex_node_label, MyTree><<"\n";
  std::cout << "net has label ? "<<PT::HasDataType<Ex_node_label, MyNet><<"\n";
  std::cout << "tree label type is "<<std::type_name<PT::DataTypeOf<Ex_node_label, MyTree>>()<<"\n";
  
  std::cout << "copying tree...\n";
  result.first = result.second;
  
  std::cout << "adding "<<num_new_edges<<" new edges...\n";
  add_random_edges(result.first, num_new_edges, num_new_edges, num_new_edges);

  return result;
}

template<class Input>
MyNet read_network(Input&& in_file){
  try{
    return parse_newick<MyNet>(std::forward<Input>(in_file));
  } catch(const std::exception& err){
    std::cerr << "could not read network: "<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

NetPair read_networks() {
  const auto& input_files = options[""];
  if(!input_files.empty()) {
    std::ifstream in0(input_files[0]);
    return (input_files.size() == 1) ? NetPair{read_network(in0), read_network(in0)} : NetPair{read_network(in0), read_network(input_files[1])};
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

  auto NT_tuple = test(options, "-r") ?
    create_net_and_tree() :
    read_net_and_tree();

  MyNet& N = NT_tuple.first;
  MyTree& T = NT_tuple.second;

  if(test(options, "-v")) {
    std::cout << "N:\n" << N << "\n";
    std::cout << get_extended_newick(N) << '\n';
    std::cout << "T:\n" << T << '\n';
    std::cout << get_extended_newick(T)<<'\n';
  }

  std::cout << "\n\n starting the containment engine...\n\n";
  if(T.is_tree()) {
    if(N.is_tree()){
      TreeInTreeContainment tc(std::move(N), std::move(T));
      if(tc.displayed())
        std::cout << "displayed\n"; // by subtrees rooted at: "<< tc.who_displays(T.root()).front() << "\n";
      else std::cout << "not displayed\n";
    } else {
      TreeInNetContainment tc(std::move(N), std::move(T));
      if(tc.displayed())
        std::cout << "displayed\n"; // by subtrees rooted at: "<< tc.who_displays(T.root()).front() << "\n";
      else std::cout << "not displayed\n";
    }
  } else std::cout << "sorry, can't check network-network containment yet...\n";
}
