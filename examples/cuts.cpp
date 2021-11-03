
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/set_interface.hpp"

#include "utils/network.hpp"
#include "utils/bridges.hpp"

using namespace PT;

static_assert(std::IterableType<std::unordered_map<PT::NodeDesc, uint32_t>>);

using MyNetwork = DefaultNetwork<>;
using MyEdge = typename MyNetwork::Edge;
using SWIter = std::seconds_iterator<std::unordered_map<PT::NodeDesc, uint32_t>>;

OptionMap options;

void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description[""] = {1,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      \toutput all vertical cut-nodes, bridges, and vcn-outedges (edges uv s.t. u is a vertical cut-node and v is separated from the root by u)\n\
      FLAGS:\n\
      \t-v\tverbose output, prints network\n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

MyNetwork read_network(std::ifstream&& in) {
  try{
    return parse_newick<MyNetwork>(in);
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

int main(const int argc, const char** argv)
{
  std::cout << "parsing options...\n";
  parse_options(argc, argv);

  std::cout << "reading network...\n";
  MyNetwork N(read_network(std::ifstream(options[""][0])));

  if(test(options, "-v"))
    std::cout << "N: " << std::endl << N << std::endl;

  for(const auto& u: get_cut_nodes(N)) std::cout << u << '\n';
  std::cout << "\n" << N << "\n\n";
  for(const auto& u: get_bridges(N)) std::cout << u << '\n';
  std::cout << "\n" << N << "\n\n";
  for(const auto& u: get_vcn_outedges(N)) std::cout << u << '\n';

  std::cout << "The End\n";
}



