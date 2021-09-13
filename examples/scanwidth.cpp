
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/set_interface.hpp"

#include "utils/tree.hpp"
#include "utils/tree_extension.hpp"
#include "utils/extension.hpp"
#include "utils/scanwidth.hpp"

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
      \t-m x\tmethod to use to compute scanwidth [default: x = 3]:\n\
      \t\t\tx = 0: brute force all permutations,\n\
      \t\t\tx = 1: dynamic programming on all vertices,\n\
      \t\t\tx = 2: brute force on raising vertices only,\n\
      \t\t\tx = 3: dynamic programming on raising vertices only,\n\
      \t\t\tx = 4: heuristic\n\
      \t-pp\tuse preprocessing\n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

unsigned parse_method()
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

MyNetwork read_network(std::ifstream& in) {
  std::cout << "reading network..."<<std::endl;
  try{
    MyNetwork N(parse_newick<MyNetwork>(in));
    return N;
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}



void print_extension(const MyNetwork& N, const Extension& ex)
{
  using GammaType = CompatibleTree<const MyNetwork>;

  std::cout << "extension: " << ex << std::endl;

  // compute scanwidth of ex
  const auto sw = ex.sw_map(N);

  std::cout << "sw: "<< sw << " --- (max: "<<*(std::max_element(seconds(sw)))<<")"<<std::endl;

  std::cout << "constructing extension tree\n";
  GammaType Gamma;
  ext_to_tree(N, ex, Gamma);
  std::cout << "extension tree:\n" << Gamma << std::endl;
  
  const auto gamma_sw = ext_tree_sw_map(Gamma, N);
  std::cout << "sw map: " << gamma_sw << std::endl;

  std::cout << "(sw = "<< *std::max_element(seconds(gamma_sw))<<")"<<std::endl;
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  std::ifstream in(options[""][0]);

  std::cout << "reading network..."<<std::endl;
  MyNetwork N(read_network(in));

  if(test(options, "-v"))
    std::cout << "N: " << std::endl << N << std::endl;

//  if(contains(options, "-pp") sw_preprocess(N);

  std::cout << "\n ==== computing silly post-order extension ===\n";
  
  //const Extension ex(N.dfs().postorder());
  Extension ex;
  auto PO = N.dfs().postorder();
  auto it = PO.begin();
  while(it != PO.end()) { ex.push_back(*it); ++it; }

  std::cout << ex << "\n";
  
  std::cout << "\n ==== computing optimal extension ===\n";

  Extension ex_opt;
  if(test(options, "-lm"))
    compute_min_sw_extension<true>(N, ex_opt);
  else
    compute_min_sw_extension<false>(N, ex_opt);
  
  std::cout << "silly extension:\n";
  print_extension(N, ex);

  std::cout << "optimal extension:\n";
  print_extension(N, ex_opt);


  std::cout << "The End\n";
//  if(contains(options, "-e")) sw_print_extension();
//  if(contains(options, "-et")) sw_print_extension_tree();
}



