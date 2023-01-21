
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

using MyNetwork = DefaultLabeledNetwork<>;
using MyEdge = typename MyNetwork::Edge;
using SWIter = mstd::seconds_iterator<std::unordered_map<PT::NodeDesc, uint32_t>>;

OptionMap options;

void parse_options(const int argc, const char** argv) {
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
      \t-h,--help\tprint this help screen\n\
      \t-u,--unicode\tuse unicode to display some things more nicely\n\
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
      \t\t\tx = 5: simple post-order layout\n\
      \t-pp\tuse preprocessing\n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

unsigned parse_method() {
  try{
    const unsigned method = std::stoi(options.at("-m").at(0));
    if(method > 5){
      std::cerr << method << " is not a vlid method, check help screen for valid methods" <<std::endl;
      exit(1);
    } else return method;
  } catch (const std::invalid_argument& ia) {
    // -m with non-integer argument
    std::cerr << "-m expects an integer argument from 0 to 4" <<std::endl;
    exit(1);
  } catch (...) {
    // default method is 1
    return 1;
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



void print_extension(const MyNetwork& N, const Extension& ex) {
  // for educational purposes, each Node of the extension tree will store the description of the corresponding node in the network
  using GammaTree = TreeExtension<MyNetwork, const NodeDesc>;
  // since we're storing the network nodes in the nodes of Gamma, the translation of Gamma's nodes to network nodes simply returns their data
  constexpr auto Gamma_to_net = [](const NodeDesc gamma_u) { return GammaTree::node_of(gamma_u).data(); };

  std::cout << "extension: " << ex << std::endl;

  // compute scanwidth of ex
  const auto sw = ex.get_sw_map<MyNetwork>();

  std::cout << "sw: "<< sw << " --- (max: "<<std::ranges::max(mstd::seconds(sw))<<")"<<std::endl;

  std::cout << "constructing extension tree\n";
  const GammaTree Gamma(ex, [](const NodeDesc u){ return u; });
  std::cout << "extension tree:\n" << ExtendedDisplay(Gamma) << std::endl;
  
  const auto gamma_nodes = Gamma.get_sw_nodes_map(Gamma_to_net);
  std::cout << "scanwidth node-map: " << gamma_nodes << std::endl;

  const auto gamma_edges = Gamma.get_sw_edges_map(Gamma_to_net);
  std::cout << "scanwidth edge-map: " << gamma_edges << std::endl;

  const auto gamma_sw = Gamma.get_sw_map(Gamma_to_net);
  std::cout << "sw map: " << gamma_sw << std::endl;

  const size_t N_sw = std::ranges::max(mstd::seconds(gamma_sw));
  const size_t reti_count = N.retis().to_container<NodeVec>().size();
  std::cout << "sw = "<< N_sw <<" retis = "<<reti_count<<"\n";
  assert(N_sw <= reti_count + 1);
}

void list_bccs(const MyNetwork& N) {
  size_t count = 0;
  using BCC_Cuts = BCCCutIterFactory<MyNetwork>;
  std::cout << "making cut-iter factory to list BCCs...\n";
  BCC_Cuts cuts(N);
  std::cout << "deriving BCC-iterator...\n";
  auto bcc_iter = std::move(cuts).begin();
  //auto bcc_iter = cuts.begin();
  if(bcc_iter.is_valid()) std::cout << "iter is valid\n";
  while(bcc_iter.is_valid()){
    std::cout << "component #"<<++count<<"\n";
    std::cout << *bcc_iter << "\n";
    std::cout << "infos["<<*bcc_iter<<"] = " << bcc_iter.get_predicate().chain_info.at(*bcc_iter) << "\n";
    ++bcc_iter;
  }

  using Component = CompatibleNetwork<MyNetwork, NodeDesc, void, void>;
  // WARNING: if we const the bc_components, then we cannot fill the old_to_new translation and the data extracter cannot change its state during extraction
  const auto bc_components = get_biconnected_components<Component>(N, [](const NodeDesc u){ return u; });
  auto iter = bc_components.begin();
  using CompMaker = typename decltype(iter)::Transformation;
  static_assert(PhylogenyType<typename CompMaker::Component>);
  static_assert(PhylogenyType<decltype(*iter)>);
  std::cout << "================ first bcc ===============\n" << ExtendedDisplay(*iter)<<"\n";
  std::cout << "================ all bccs ================\n";
  for(const auto& bcc: bc_components){
    std::cout << "found biconnected component with "<<bcc.num_nodes()<<" nodes & "
      <<bcc.num_edges()<<" edges --> "<<bcc.num_edges()+1-bcc.num_nodes()<<" reticulations):\n" << ExtendedDisplay(bcc) <<"\n";
  }

  std::cout << "\n================ done listing BCCs =================\n";
}


int main(const int argc, const char** argv) {
  std::cout << "parsing options...\n";
  parse_options(argc, argv);

  std::cout << "reading network...\n";
  MyNetwork N(read_network(std::ifstream(options[""][0])));
  if(mstd::test(options, "-v"))
    std::cout << "N: " << std::endl << N << std::endl;

  if(N.has_cycle()) {
    std::cerr << "input not a network (has a directed cycle)!\n";
    exit(1);
  }
	//list_bccs(N);

  const bool preprocess = mstd::test(options, "-pp");

  Extension ex;
  ex.reserve(N.num_nodes());
  switch(parse_method()) {
  case 0:
    throw std::logic_error("unimplemented");
  case 1:
    std::cout << "\n ==== computing optimal extension ===\n";
    if(mstd::test(options, "-lm")){
      std::cout << "using low-memory version...\n";
      if(preprocess)
        compute_min_sw_extension<true, true>(N, ex);
      else compute_min_sw_extension<true, false>(N, ex);
      //compute_min_sw_extension<true>(N, ex); // this is equivalent
    } else {
      std::cout << "using faster, more memory hungry version...\n";
      if(preprocess)
        compute_min_sw_extension<false, true>(N, ex);
      else compute_min_sw_extension<false, false>(N, ex);
    }
    break;
  case 2:
  case 3:
  case 4:
    throw std::logic_error("unimplemented");
  case 5:
    std::cout << "\n ==== computing silly post-order extension ===\n";  
    for(const auto& x: N.nodes_postorder()) ex.push_back(x);
    break;
  default:
    throw std::logic_error("Something went wrong with the scanwidth-method selection. This should not happen!");
  }
  std::cout << ex << "\n";
  print_extension(N, ex);
//  if(contains(options, "-e")) sw_print_extension();
//  if(contains(options, "-et")) sw_print_extension_tree();
}



