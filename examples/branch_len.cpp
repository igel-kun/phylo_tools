
#include "io/newick.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"

using namespace PT;

using MyNetwork = RONetwork<void, float>;
using MyLabelMap = typename MyNetwork::LabelMap;

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
  MyLabelMap names;

  std::cout << "reading network..."<<std::endl;
  try{
    parse_newick(in, weighted_edges, names);
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }

  DEBUG5(std::cout << "building N from "<<weighted_edges<< std::endl);

  MyNetwork N(weighted_edges, names);
  //EdgeWeightedNetwork<> N(weighted_edges, names);
  //EdgeWeightedNetwork<float, void*, IndexSet, GrowingNetworkAdjacencyStorage<WEdge>> N(weighted_edges, names);

  if(test(options, "-v"))
    std::cout << N << std::endl;
 
  for(const Node u: N.nodes()){
    for(const WEdge uv: N.out_edges(u))
      std::cout << "branch "<<u<<"["<<N.get_label(u)<<"] -> "<<head(uv)<<"["<<N.get_label(uv.head())<<"] has length "<<uv.data()<<std::endl;
  }
}

