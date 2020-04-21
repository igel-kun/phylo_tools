
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/isomorphism.hpp"

using NetworkA = PT::RONetwork<>;
// to demonstrate that isomorphism checks work with different network types, we declare the second network RW
using NetworkB = PT::RWNetwork<>;

template<class _LabelMap>
bool read_from_stream(std::ifstream& in, PT::EdgeVec& el, _LabelMap& names)
{
  try{
    DEBUG3(std::cout << "trying to read newick..." <<std::endl);
    PT::parse_newick(in, el, names);
  } catch(const PT::MalformedNewick& nw_err){
    std::cout << "reading newick failed: "<<nw_err.what()<<std::endl;
    DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
    try{
      in.seekg(0);
      PT::parse_edgelist(in, el, names);
    } catch(const PT::MalformedEdgeVec& el_err){
      std::cout << "reading edgelist failed: "<<el_err.what()<<std::endl;
      return false;
    }
  }
  return true;
}

PT::OptionMap options;

void parse_given_options(const int argc, const char** argv)
{
  PT::OptionDesc description;
  description["-v"] = {0,0};
  description["-mr"] = {0,0};
  description["-mt"] = {0,0};
  description["-il"] = {0,0};
  description["-ma"] = {0,0};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file1> [file2]\n\
      \tfile1 and file2 describe two networks (either file1 test 2 lines of extended newick or both file1 and file2 describe a network in extended newick or edgelist format)\n\
      FLAGS:\n\
      \t-v\tverbose output, prints networks\n\
      \t-mr\tlabels of reticulations have to match\n\
      \t-mt\tlabels of non-leaf tree vertices have to match\n\
      \t-ma\tlabels of all vertices have to match (shortcut for -mr -mt (-ma overrides -il))\n\
      \t-il\tlabels of leaves do NOT have to match\n");

  PT::parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

int main(const int argc, const char** argv)
{
  parse_given_options(argc, argv);

  std::ifstream in(options[""][0]);

  PT::EdgeVec el0,el1;
  PT::LabelMapOf<NetworkA> names0;
  PT::LabelMapOf<NetworkB> names1;

  std::cout << "reading networks..."<<std::endl;
  if(!read_from_stream(in, el0, names0)){
    std::cerr << "could not read any network from "<<options[""][0]<<std::endl;
    exit(EXIT_FAILURE);
  } else {
    // for some reason, EOF is not detected unless we peek
    in.peek();
    if((in.bad() || in.eof() || !read_from_stream(in, el1, names1))){
      if(options[""].size() > 1){
        in.close();
        in.open(options[""][1]);
        if(!read_from_stream(in, el1, names1)){
          std::cerr << "could not read any network from "<<options[""][1]<<std::endl;
          exit(EXIT_FAILURE);
        }
      } else {
        std::cerr << "could not read 2 networks from "<<options[""][0]<<" but no other useable source was found"<<std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  DEBUG5(std::cout << "building N0 ("<<names0.size()<<" nodes) from edges: "<<el0<< std::endl);
  NetworkA N0(el0, names0);
  DEBUG5(std::cout << "building N1 ("<<names1.size()<<" nodes) from edgess: "<<el1<< std::endl);
  NetworkB N1(el1, names1);

  if(test(options, "-v"))
    std::cout << "N0: " << std::endl << N0 << std::endl << "N1:" <<std::endl << N1 << std::endl;
  const unsigned char iso_flags = ((!test(options, "-il")) ? FLAG_MAP_LEAF_LABELS : 0) |
                                  ((test(options, "-mt")) ? FLAG_MAP_TREE_LABELS : 0) |
                                  ((test(options, "-mr")) ? FLAG_MAP_RETI_LABELS : 0) |
                                  ((test(options, "-ma")) ? FLAG_MAP_ALL_LABELS : 0);

  std::cout << "checking isomorphism..."<<std::endl;
  auto M = make_iso_mapper(N0, N1, iso_flags);
  if(M.check_isomorph())
    std::cout << "isomorph!" << std::endl;
  else
    std::cout << "not isomorph!" << std::endl;
}

