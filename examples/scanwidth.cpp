
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/set_interface.hpp"

#include "utils/tree_extension.hpp"
#include "utils/extension.hpp"
#include "utils/scanwidth.hpp"

using namespace PT;
 
using MyNetwork = RONetwork<>;
using MyEdge = typename MyNetwork::Edge;
using LabelMap = typename MyNetwork::LabelMap;
using SWIter = SecondIterator<std::unordered_map<PT::Node, uint32_t>>;

 
bool read_from_stream(std::ifstream& in, EdgeVec& el, LabelMap& names)
{
  try{
    DEBUG3(std::cout << "trying to read newick..." <<std::endl);
    PT::parse_newick(in, el, names);
  } catch(const MalformedNewick& nw_err){
    DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
    try{
      in.seekg(0);
      PT::parse_edgelist(in, el, names);
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
  description["-lm"] = {0,0};
  description["-m"] = {1,1};
  description[""] = {1,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      \tcompute the scanwidth (+extension and/or extension tree) of the network described in file (extended newick or edgelist format)\n\
      FLAGS:\n\
      \t-v\tverbose output, prints network\n\
      \t-e\tprint an optimal extension\n\
      \t-et\tprint an optimal extension tree (corresponds to the extension)\n\
      \t-lm\tuse low-memory data structures when doing dynamic programming (uses 25% of the space at the cost of factor |V(N)| running time)\n\
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

void print_extension(const MyNetwork& N, const Extension& ex)
{
  std::cout << "extension: " << ex << std::endl;

  // compute scanwidth of ex
  const auto sw = ex.sw_map(N);

  std::cout << "sw: "<< sw << " --- (max: "<<*(std::max_element(seconds(sw)))<<")"<<std::endl;

  std::vector<MyEdge> gamma_el;
  ext_to_tree(N, ex, gamma_el);

  std::cout << "constructing extension tree\n";
  CompatibleROTree<MyNetwork> Gamma(gamma_el, N.labels());
  const OutDegreeMap gamma_sw = ext_tree_sw_map(Gamma, N);
  
  std::cout << "extension tree:\n" << Gamma << std::endl << "(sw = "<< *std::max_element(seconds(gamma_sw))<<")"<<std::endl;
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  std::ifstream in(options[""][0]);

  EdgeVec el;
  typename MyNetwork::LabelMap names;

  std::cout << "reading network..."<<std::endl;
  if(!read_from_stream(in, el, names)){
    std::cerr << "could not read network from "<<options[""][0]<<std::endl;
    exit(EXIT_FAILURE);
  }

  DEBUG5(std::cout << "building N ("<<names.size()<<" nodes) from edges: "<<el<< std::endl);
  MyNetwork N(el, names);

  if(test(options, "-v"))
    std::cout << "N: " << std::endl << N << std::endl;

//  if(contains(options, "-pp") sw_preprocess(N);

  std::cout << "\n ==== computing silly post-order extension ===\n";
  Extension ex;
  for(const auto& u: N.get_nodes<postorder>()) append(ex, u);
  print_extension(N, ex);

  std::cout << "\n ==== computing optimal extension ===\n";
  
  Extension ex_opt;
  if(test(options, "-lm"))
    compute_min_sw_extension<true>(N, ex_opt);
  else
    compute_min_sw_extension<false>(N, ex_opt);
  
  std::cout << "\n ==== optimal extension found ===\n";
  print_extension(N, ex_opt);

  std::cout << "The End\n";
//  if(contains(options, "-e")) sw_print_extension();
//  if(contains(options, "-et")) sw_print_extension_tree();
}



