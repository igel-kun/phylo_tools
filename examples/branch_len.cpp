
#include "io/newick.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/label_map.hpp"
#include "solv/isomorphism.hpp"

using namespace PT;
  

// read a network from an input stream 
void read_newick_from_stream(std::ifstream& in, WEdgeVec& edges, std::vector<std::string>& names)
{
  std::string in_line;
  std::getline(in, in_line);

  NewickParser parser(in_line, names);
  parser.read_tree(edges);
}


OptionMap options;

void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file1>\n\
      \tfile contains a network (possibly with branchlengths) in extended newick format\n\
      FLAGS:\n\
      \t-v\tverbose output, prints networks\n");

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

  WEdgeVec weighted_edges;
  std::vector<std::string> names;

  std::cout << "reading network..."<<std::endl;
  try{
    read_newick_from_stream(in, weighted_edges, names);
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }

  DEBUG5(std::cout << "building N from "<<weighted_edges<< std::endl);

  NetworkT<WEdge> N(weighted_edges, names);

  if(contains(options, "-v"))
    std::cout << N << std::endl;
 
  for(uint32_t u = 0; u < N.num_nodes(); ++u)
    for(const WEdge& uv: N[u].out)
      std::cout << "branch "<<u<<"["<<N.get_name(u)<<"] -> "<<head(uv)<<"["<<N.get_name(head(uv))<<"] has length "<<uv.data<<std::endl;
}

