
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/mul_wrapper.hpp" // treat networks as multi-labeled trees
//#include "utils/tc_preprocess.hpp" // preprocessing

using namespace PT;

OptionMap options;
void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file1> [file2]\n\tfile1 and file2 describe two networks (either file1 contains 2 lines of extended newick or both file1 and file2 describe a network in extended newick or edgelist format)\n Unless the first network is a tree and the second is not, we try to embed the second network in the first.");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

// if we declare the network to be compatible without ROTree, then we get a vector_map label-map in both
// if we declare the network first, as RWNetwork<> and the tree to be CompatibleROTree<...>, then the label maps will both be unordered_map
using MyTree = ROTree<>;
using MyNet = CompatibleRWNetwork<MyTree>;
// in any case, both label maps have the same type
using LabelMap = typename MyTree::LabelMap;
static_assert(std::is_same_v<LabelMap, typename MyNet::LabelMap>);


bool read_from_stream(std::ifstream& in, EdgeVec& el, LabelMap& node_labels)
{
  try{
    DEBUG3(std::cout << "trying to read newick..." <<std::endl);
    parse_newick(in, el, node_labels);
  } catch(const MalformedNewick& nw_err){
    try{
      in.seekg(0);
      DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
      parse_edgelist(in, el, node_labels);
    } catch(const MalformedEdgeVec& el_err){
      return false;
    }
  }
  return true;
}

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

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);
  std::ifstream in(options[""][0]);

  EdgeVec el[2];
  LabelMap node_labels[2];

  if(!read_from_stream(in, el[0], node_labels[0])){
    std::cerr << "could not read any network from "<<options[""][0]<<std::endl;
    exit(EXIT_FAILURE);
  } else {
    if(options[""].size() > 1){
      // if we've been given a second filename, try to read that one next
      in.close();
      in.open(options[""][1]);
      if(!read_from_stream(in, el[1], node_labels[1])){
        std::cerr << "could not read any network from "<<options[""][1]<<std::endl;
        exit(EXIT_FAILURE);
      }
    } else {
      // if we have only a single filename, try to continue reading the same file
      if((in.bad() || in.eof() || !read_from_stream(in, el[1], node_labels[1]))){
        std::cerr << "could not read 2 networks from "<<options[""][0]<<" but no other useable source was found"<<std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  DEBUG5(std::cout << "building N0 from "<<el[0]<< std::endl << "building N1 from "<<el[1]<<std::endl);

  // choose which index corresponds to the host and which to the guest (we try to embed guest into host)
  const bool first_is_tree = (el[0].size() == node_labels[0].size() - 1);
  const bool second_is_tree= (el[1].size() == node_labels[1].size() - 1);
  const bool host_net_index = (first_is_tree && !second_is_tree);
  const bool guest_net_index = 1 - host_net_index;
  MyNet  N(el[host_net_index], node_labels[host_net_index]);
  MyTree T(el[guest_net_index], node_labels[guest_net_index]);

  if(test(options, "-v"))
    std::cout << N << std::endl << T << std::endl;

  if(T.is_tree()) {
    if(N.is_tree()){

    } else {
      std::cout << "sorry, can't check network-tree containment yet...\n";
    }
  } else std::cout << "sorry, can't check network-network containment yet...\n";

/*
  if(check_display(N, T))
    std::cout << "display!" << std::endl;
  else
    std::cout << "no display!" << std::endl;
*/  
}

