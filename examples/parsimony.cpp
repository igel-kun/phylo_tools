
#include "io/newick.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/set_interface.hpp"

#include "utils/tree.hpp"
#include "utils/tree_extension.hpp"
#include "utils/extension.hpp"
#include "utils/scanwidth.hpp"
#include "utils/parsimony.hpp"

using namespace PT;

using MyNetwork = DefaultLabeledNetwork<>;
using MyEdge = typename MyNetwork::Edge;
using SWIter = mstd::seconds_iterator<std::unordered_map<PT::NodeDesc, uint32_t>>;
using CharacterStates = NodeMap<uint8_t>;

OptionMap options;

void parse_options(const int argc, const char** argv) {
  OptionDesc description;
  description["-v"] = {0,0};
  description["-e"] = {0,0};
  description["-et"] = {0,0};
  description["-pp"] = {0,0};
  description["-lm"] = {0,0};
  description["-m"] = {1,1};
  description["-s"] = {1,1};
  description[""] = {1,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      \tcompute the scanwidth (+extension and/or extension tree) of the network described in file (extended newick or edgelist format)\n\
      FLAGS:\n\
      \t-s x\tnumber of character states to generate (between 2 and 256) [default: x = 2]\n\
      \t-v\tverbose output, prints network\n\
      \t-e\tprint an optimal extension\n\
      \t-et\tprint an optimal extension tree (corresponds to the extension)\n\
      \t-lm\tuse low-memory data structures for computing scanwidth (uses 25% of the space at the cost of factor |V(N)| running time)\n\
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

size_t parse_method() {
  if(test(options, "-m"))
    return ConstraintIntParser{options["-m"], 0, 4}.parse_next_argument();
  else return 3;
}

size_t parse_num_states() {
  if(test(options, "-s"))
    return ConstraintIntParser{options["-s"], 2, 256}.parse_next_argument();
  else return 2;
}

MyNetwork read_network(std::ifstream&& in) {
  try{
    return parse_newick<MyNetwork>(in);
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}



size_t get_parsimony_score(const MyNetwork& N, const Extension& ex, CharacterStates& cs, const size_t num_states) {
  // for educational purposes, each Node of the extension tree will store the description of the corresponding node in the network
  std::cout << "extension: " << ex << '\n';
  std::cout << "using character-state map "<<cs<<'\n';
  const auto solution = make_parsimony_HW_DP(N, ex, cs, num_states);
  assert(&N == &(solution.N)); // make sure it didn't make its own copy of N
  std::cout << "extended to character-state map "<<cs<<'\n';
  return solution.score();
}



int main(const int argc, const char** argv) {
  std::cout << "parsing options...\n";
  parse_options(argc, argv);

  std::cout << "reading network...\n";
  MyNetwork N(read_network(std::ifstream(options[""][0])));
  if(mstd::test(options, "-v"))
    std::cout << "N: " << std::endl << N << std::endl;

  const size_t num_states = parse_num_states();
  CharacterStates cs;
  std::cout << "putting random character-states...\n";
  for(const NodeDesc x: N.leaves()) cs.emplace(x, rand() % num_states);


//  if(mstd::test(options, "-pp") sw_preprocess(N);

  std::cout << "\n ==== computing silly post-order extension ===\n";  
  Extension ex;
  CharacterStates silly_cs{cs};
  ex.reserve(N.num_nodes());
  for(const auto& x: N.nodes_postorder()) ex.push_back(x);
  const size_t hw_score = get_parsimony_score(N, ex, silly_cs, num_states);

  std::cout << "\n ==== computing optimal extension ===\n";
  Extension ex_opt;
  ex_opt.reserve(N.num_nodes());
  if(mstd::test(options, "-lm")){
    std::cout << "using low-memory version...\n";
    compute_min_sw_extension<true>(N, [&](const NodeDesc u){ ex_opt.push_back(u); });
    //compute_min_sw_extension<true>(N, ex_opt); // this is equivalent
  } else {
    std::cout << "using faster, more memory hungry version...\n";
    compute_min_sw_extension<false>(N, ex_opt);
  }
  std::cout << "computed "<<ex_opt << "\n";  
  const size_t hw_score2 = get_parsimony_score(N, ex, cs, num_states);
  std::cout << "HW score (silly extension): "<<hw_score <<'\n';
  std::cout << "HW score (optimal extension): "<<hw_score2 <<'\n';

  assert(hw_score == hw_score2);

}



