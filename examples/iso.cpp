
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/label_map.hpp"
#include "solv/isomorphism.hpp"

using namespace PT;
  

// read a network from an input stream 
void read_newick_from_stream(std::ifstream& in, EdgeVec& el, std::vector<std::string>& names)
{
  std::string in_line;
  std::getline(in, in_line);

  NewickParser<EdgeVec> parser(in_line, names, el);
  parser.read_tree();
}

void read_edgelist_from_stream(std::ifstream& in, EdgeVec& el, std::vector<std::string>& names)
{
  EdgeVecParser<> parser(in, names, el);
  parser.read_tree();
}

bool read_from_stream(std::ifstream& in, EdgeVec& el, std::vector<std::string>& names)
{
  try{
    DEBUG3(std::cout << "trying to read newick..." <<std::endl);
    read_newick_from_stream(in, el, names);
  } catch(const MalformedNewick& nw_err){
    DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
    try{
      in.seekg(0);
      read_edgelist_from_stream(in, el, names);
    } catch(const MalformedEdgeVec& el_err){
      std::cout << "reading Newick failed: "<<nw_err.what()<<std::endl;
      return false;
    }
  }
  return true;
}

OptionMap options;

void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description["-mr"] = {0,0};
  description["-mt"] = {0,0};
  description["-il"] = {0,0};
  description["-ma"] = {0,0};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file1> [file2]\n\
      \tfile1 and file2 describe two networks (either file1 contains 2 lines of extended newick or both file1 and file2 describe a network in extended newick or edgelist format)\n\
      FLAGS:\n\
      \t-v\tverbose output, prints networks\n\
      \t-mr\tlabels of reticulations have to match\n\
      \t-mt\tlabels of non-leaf tree vertices have to match\n\
      \t-ma\tlabels of all vertices have to match (shortcut for -mr -mt (-ma overrides -il))\n\
      \t-il\tlabels of leaves do NOT have to match\n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  std::ifstream in(options[""][0]);

  EdgeVec el[2];
  std::vector<std::string> names[2];

  std::cout << "reading networks..."<<std::endl;
  if(!read_from_stream(in, el[0], names[0])){
    std::cerr << "could not read any network from "<<options[""][0]<<std::endl;
    exit(EXIT_FAILURE);
  } else {
    // for some reason, EOF is not detected unless we peek
    in.peek();
    if((in.bad() || in.eof() || !read_from_stream(in, el[1], names[1]))){
      if(options[""].size() > 1){
        in.close();
        in.open(options[""][1]);
        if(!read_from_stream(in, el[1], names[1])){
          std::cerr << "could not read any network from "<<options[""][1]<<std::endl;
          exit(EXIT_FAILURE);
        }
      } else {
        std::cerr << "could not read 2 networks from "<<options[""][0]<<" but no other useable source was found"<<std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  DEBUG5(std::cout << "building N0 ("<<names[0].size()<<" nodes) from edges: "<<el[0]<< std::endl);
  Network<NonGrowingNetworkAdjacencyStorage<>> N0(el[0], names[0]);
  DEBUG5(std::cout << "building N1 ("<<names[1].size()<<" nodes) from edgess: "<<el[1]<< std::endl);
  Network<> N1(el[1], names[1]);

  if(contains(options, "-v"))
    std::cout << "N0: " << std::endl << N0 << std::endl << "N1:" <<std::endl << N1 << std::endl;
  const unsigned char iso_flags = ((!contains(options, "-il")) ? FLAG_MAP_LEAF_LABELS : 0) |
                                  ((contains(options, "-mt")) ? FLAG_MAP_TREE_LABELS : 0) |
                                  ((contains(options, "-mr")) ? FLAG_MAP_RETI_LABELS : 0) |
                                  ((contains(options, "-ma")) ? FLAG_MAP_ALL_LABELS : 0);

  LabelMap *lmap = nullptr;
  lmap = build_labelmap(N0, N1, lmap);
  DEBUG3(std::cout << "label mapping: "<<*lmap<<std::endl);

  std::cout << "checking isomorphism..."<<std::endl;
  auto M = make_iso_mapper(N0, N1, *lmap, iso_flags);
  if(M.check_isomorph())
    std::cout << "isomorph!" << std::endl;
  else
    std::cout << "not isomorph!" << std::endl;

  delete lmap;
}

