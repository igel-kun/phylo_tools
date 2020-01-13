
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/set_interface.hpp"

#include "utils/tree_extension.hpp"
#include "utils/extension.hpp"
#include "utils/scanwidth.hpp"

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
  description["-e"] = {0,0};
  description["-et"] = {0,0};
  description["-pp"] = {0,0};
  description["-m"] = {1,1};
  description[""] = {1,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      \tcompute the scanwidth (+extension and/or extension tree) of the network described in file (extended newick or edgelist format)\n\
      FLAGS:\n\
      \t-v\tverbose output, prints network\n\
      \t-e\tprint an optimal extension\n\
      \t-et\tprint an optimal extension tree (corresponds to the extension)\n\
      \t-m <int>\tmethod to use to compute scanwidth: 0 = brute force all permutations, 1 = dynamic programming on all vertices, 2 = brute force on raising vertices only, 3 = dynamic programming on raising vertices only, 4 = heuristic [default=3]\n\
      \t-pp\tuse preprocessing\n\
      \n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

const unsigned parse_method()
{
  try{
    const unsigned method = std::stoi(options["-m"][0]);
    if(method > 4){
      std::cout << method << " is not a vlid method, check help screen for valid methods" <<std::endl;
      exit(1);
    } else return method;
  } catch (const std::invalid_argument& ia) {
    // -m with non-integer argument
    std::cout << "-m expects an integer argument from 0 to 4" <<std::endl;
    exit(1);
  } catch (...) {
    // default method is 3
    return 3;
  } 
}

using MyNetwork = Network<NonGrowingNetworkAdjacencyStorage<>>;
using MyNode = typename MyNetwork::Node;
using MyEdge = typename MyNetwork::Edge;
using SWIter = ConstSecondIterator<std::unordered_map<MyNode, uint32_t>>;

void print_extension(const MyNetwork& N, const Extension<>& ex)
{
  
  std::cout << "extension: " << ex << std::endl;

  // compute scanwidth of ex
  const std::unordered_map<MyNode, uint32_t> sw = ex.sw_map(N);

  std::cout << "sw: "<< sw << " --- (max: "<<*(std::max_element(SWIter(sw.begin()), SWIter(sw.end())))<<")"<<std::endl;

  std::vector<MyEdge> gamma_el;
  ext_to_tree(N, ex, gamma_el);

  std::cout << "constructing extension tree\n";
  Tree<> Gamma(gamma_el, N.get_names());
  const std::unordered_map<MyNode, uint32_t> gamma_sw = ext_tree_sw_map(Gamma, N);
  
  std::cout << "extension tree:\n" << Gamma << std::endl
    << "(sw = "<< *std::max_element(SWIter(gamma_sw.begin()), SWIter(gamma_sw.end()))<<")"<<std::endl;
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  std::ifstream in(options[""][0]);

  EdgeVec el;
  std::vector<std::string> names;

  std::cout << "reading network..."<<std::endl;
  if(!read_from_stream(in, el, names)){
    std::cerr << "could not read network from "<<options[""][0]<<std::endl;
    exit(EXIT_FAILURE);
  }

  DEBUG5(std::cout << "building N ("<<names.size()<<" nodes) from edges: "<<el<< std::endl);
  MyNetwork N(el, names);

  if(contains(options, "-v"))
    std::cout << "N: " << std::endl << N << std::endl;

//  if(contains(options, "-pp") sw_preprocess(N);


  Extension<> ex;
  for(const auto& u: N.get_nodes<postorder>())
    ex.push_back(u);
  print_extension(N, ex);
  std::cout << "\n ==== computing optimal extension ===\n";
  Extension<> ex_opt;
  compute_min_sw_extension_brute_force(N, ex_opt);
  print_extension(N, ex_opt);

  std::cout << "The End\n";
//  if(contains(options, "-e")) sw_print_extension();
//  if(contains(options, "-et")) sw_print_extension_tree();
}



