
#include "io/io.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/generator.hpp"
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
    for(const std::string& filename: options[""])
      if(!file_exists(filename)) {
        std::cerr << filename << " cannot be opened for reading" << std::endl;
        exit(EXIT_FAILURE);
      }
  }
}

// if we declare the network to be compatible without ROTree, then we get a vector_map label-map in both
// if we declare the network first, as RWNetwork<> and the tree to be CompatibleROTree<...>, then the label maps will both be unordered_map
using MyTree = ROTree<>;
using MyNet = CompatibleRWNetwork<MyTree>;
// in any case, both label maps have the same type
using LabelMap = typename MyTree::LabelMap;
static_assert(std::is_same_v<LabelMap, typename MyNet::LabelMap>);


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

using NetAndTree = std::pair<MyNet, MyTree>;

NetAndTree create_net_and_tree()
{
  EdgeVec el;
  std::shared_ptr<LabelMap> node_labels = std::make_shared<LabelMap>();

  const uint32_t num_internals = std::stoi(options["-r"][0]);
  const uint32_t num_leaves = std::stoi(options["-r"][1]);
  const uint32_t num_new_edges = std::stoi(options["-r"][2]);

  generate_random_tree(el, *node_labels, num_internals, num_leaves);
  
  MyNet N(el, node_labels);

  std::cout << "rolled tree:\n"<<N<<"\n";
  std::cout << "adding "<<num_new_edges<<" new edges...\n";

  add_random_edges(N, num_new_edges, num_new_edges, num_new_edges);

  return { std::piecewise_construct, std::forward_as_tuple(std::move(N)), std::forward_as_tuple(std::move(el), node_labels, consecutive_nodes)};
}

NetAndTree read_net_and_tree()
{
  std::vector<EdgesAndNodeLabels<MyTree, LabelMap>> el;
  read_edgelists(options[""][0], el);

  if(el.size() < 2){
    std::cerr << "could not read 2 networks from files "<<options[""]<<std::endl;
    exit(EXIT_FAILURE);
  }

  // choose which index corresponds to the host and which to the guest (we try to embed guest into host)
  // if we've been given 2 trees, the first is considered the host, otherwise, the network is considered the host :)
  const bool host_net_index = (el[0].is_tree() && !el[1].is_tree());
  const bool guest_net_index = 1 - host_net_index;
  return { std::piecewise_construct,
           std::forward_as_tuple(std::move(el[host_net_index].edges), std::move(el[host_net_index].labels), consecutive_tag()),
           std::forward_as_tuple(std::move(el[guest_net_index].edges), std::move(el[guest_net_index].labels), consecutive_tag()) };
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  auto NT_tuple = test(options, "-r") ?
    create_net_and_tree() :
    read_net_and_tree();

  MyNet& N = NT_tuple.first;
  MyTree& T = NT_tuple.second;

  if(test(options, "-v"))
    std::cout << "N:\n" << N << "\n" << get_extended_newick(N) << "\nT:\n" << T << "\n"<< get_extended_newick(T)<<"\n";

  std::cout << "\n\n starting the containment engine...\n\n";
  if(T.is_tree()) {
    if(N.is_tree()){
      TreeInTreeContainment tc(std::move(N.as_tree()), std::move(T));
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

