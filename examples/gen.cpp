
#include "io/newick.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/net_gen.hpp"

using namespace PT;
  

OptionMap options;

void parse_options(const int argc, const char** argv) {
  OptionDesc description;
  description["-v"] = {0,0};
  description["-n"] = {1,1};
  description["-r"] = {1,1};
  description["-l"] = {1,1};
  description["-s"] = {1,1};
  description["-a"] = {0,0};
  description["-L"] = {0,0};
  description["-TBR"] = {2,2};
  description[""] = {0,1};
  const std::string help_message(std::string(argv[0]) + " [file]\n\
      generate a random binary network and write it to file (stdout if omitted) in extended newick format\n\
      FLAGS:\n\
      \t[network properties]\n\
      \t-r <#reti>\tnumber of reticulations in the network\n\
      \t-l <#leaf>\tnumber of leaves in the network\n\
      \t-n <#node>\tnumber of vertices in the network (this is ignored if -r and -l are present)\n\
      \n\t[general]\n\
      \t-v\tverbose output, prints networks\n\
      \t-a\tappend to file1 instead of replacing its contents\n\
      \t-s <seed>\tset random seed to 'seed'\n\
      \t-L\tput labels on the leaves (small-letter strings in lexicographic order)\n\
      \t-TBR <file> <TBR-dist>\tgenerate a network by applying 'TBR-dist' TBR-moves to the network in 'file' (extended newick)\
      NOTE: if, of -n, -r, and -l, less than 2 are present, the network is assumed to have ~10% reticulations\n\
      NOTE: n = 99 is assumed if none are present\n");

  parse_options(argc, argv, description, help_message, options);
}


void get_node_numbers(long& num_nodes, long& num_retis, long& num_leaves) {
  //NOTE: in a binary network, we have n = t + r + l, but also l + r - 1 = t (together, n = 2t + 1 and n = 2l + 2r - 1)
  const int total_input = mstd::test(options, "-n") + mstd::test(options, "-r") + mstd::test(options, "-l");
  try{
    if(total_input == 0){
      num_nodes = 99;
      num_retis = 10;
      num_leaves = l_from_nr(num_nodes, num_retis);
    } else if(total_input == 1){
      // if we only have one input, we assume that 10r = n and, thus, 9r = t + l and l + r - 1 = t (togeher 8r = 2l - 1)
      if(mstd::test(options, "-n")){
        num_nodes = arg_from_string(options["-n"][0]);
        num_retis = num_nodes / 10;
        num_leaves = l_from_nr(num_nodes, num_retis);
      } else if(mstd::test(options, "-r")){
        num_retis = arg_from_string(options["-r"][0]);
        num_nodes = 10 * num_retis + 1;
        num_leaves = l_from_nr(num_nodes, num_retis);
      } else {
        num_leaves = arg_from_string(options["-l"][0]);
        num_retis = (2 * num_leaves - 1) / 8;
        num_nodes = n_from_rl(num_retis, num_leaves);
      }
    } else if(total_input == 2){
      if(!mstd::test(options, "-l")){
        num_retis = arg_from_string(options["-r"][0]);
        num_nodes = arg_from_string(options["-n"][0]);
        num_leaves = l_from_nr(num_nodes, num_retis);
      } else if(!mstd::test(options, "-n")){
        num_retis = arg_from_string(options["-r"][0]);
        num_leaves = arg_from_string(options["-l"][0]);
        num_nodes = n_from_rl(num_retis, num_leaves);
      } else {
        num_nodes = arg_from_string(options["-n"][0]);
        num_leaves = arg_from_string(options["-l"][0]);
        num_retis  = r_from_nl(num_nodes, num_leaves);
      }
    } else {
      num_retis = arg_from_string(options["-r"][0]);
      num_nodes = arg_from_string(options["-n"][0]);
      num_leaves = arg_from_string(options["-l"][0]);
      if(l_from_nr(num_nodes,  num_retis) != num_leaves)
        throw std::logic_error("there is no binary network with "+std::to_string(num_nodes)+" vertices, "+std::to_string(num_retis)+" reticulations and "+std::to_string(num_leaves)+" leaves");
    }
  } catch(const std::logic_error& err) {
    std::cerr << "cannot generate such a network: "<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

using MyNetwork = DefaultLabeledNetwork<>;
using MyEdge = typename MyNetwork::Edge;

auto read_network(auto&& in) {
  try{
    return parse_newick<MyNetwork>(in);
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

MyEdge advance_and_return(auto& iter, const size_t dist) {
  std::advance(iter);
  return *iter;
}

int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  if(test(options, "-s"))
    std::srand(arg_from_string(options["-s"][0]));
  
  MyNetwork N;

  if(test(options, "-TBR")) {
    size_t TBR_dist = arg_from_string(options["-TBR"][1]);
    
    N = read_network(std::ifstream(options["-TBR"][0]));
    while(TBR_dist--) {
      if(N.num_edges() < 3) 
        throw std::logic_error("cannot make TBR-moves on a network with less than 3 edges");

      // step 1: we'll need 3 DISTINCT random edges: st, uv, and xy such that
      //  1. x is not below v (if so, swap uv and xy) since, otherwise, inserting the new edge will create a cycle
      //  2. both s and t must be weakly connected to at least one of u, v, x, y in N-st since, otherwise, the result is disconnected

      // step 1.1: get st, note that neither s nor t shall have degree one as, otherwise, we won't be able to reconnect it
      auto edges_traversal = N.edges();
      MyEdge st, uv, xy;
      do {
        st = *(get_random_iterator(edges_traversal, N.num_edges()));
      } while((N.degree(st.head()) == 1) || (N.degree(st.tail()) == 1));
      const NodeDesc t = st.head();

      // step 1.2: remove st from N
      N.remove_edge_no_cleanup(st);
      
      
      // step 1.3: choose uv among the edges reachable from the root of N-st
      const NodeDesc t_root = N.is_root(t) ? t : N.root();
      do {
        uv = sample<NodeSingleton>(N.edges(), 1).front();
        // NOTE: we'll be in trouble if there is only 1 edge below t and we choose that one for uv, since then, we can't choose anything for xy
      } while((N.out_degree(t_root) == 1) || (uv.tail() == t_root));

      // step 1.4: choose xy among the edges reachalbe from the root above t in N-st
      while(1) {
        xy = sample<NodeSingleton>(N.edges_below(t_root), 1).front();
        if(xy != uv) {
        }
      }

      const auto st = advance_and_return(edge_it, three_ints[0]);
      auto _uv = advance_and_return(edge_it, three_ints[1] - three_ints[0]);
      auto _xy = advance_and_return(edge_it, three_ints[2] - three_ints[1]);

      // step 1.5: check whether x is below v and swap if necessary
      const bool needs_swap = N.has_path(xy.tail(), uv.head());
      const MyEdge xy = needs_swap ? std::move(_uv) : std::move(_xy);
      const MyEdge uv = needs_swap ? std::move(_xy) : std::move(_uv);

      // step 2: apply the TBR-move
      apply_TBR_move(N, st, uv, xy);
    }
  } else {
    long num_nodes, num_retis, num_leaves;
    get_node_numbers(num_nodes, num_retis, num_leaves);
    const long num_tree_nodes = num_nodes - num_retis - num_leaves;

    if((num_nodes < 0) || (num_retis < 0) || (num_leaves < 0) || (num_tree_nodes < 0))
      throw std::logic_error("network geometry implied by your parameters is invalid: "+
          std::to_string(num_tree_nodes)+" tree nodes, "+
          std::to_string(num_retis)+" reticulations, "+
          std::to_string(num_leaves)+" leaves = "+
          std::to_string(num_nodes)+" nodes in total");
    std::cout << "constructing network with "<<num_nodes<<" vertices: "<<num_tree_nodes<<" tree nodes, "<<num_retis<<" reticulations and "<<num_leaves<<" leaves"<<std::endl;

    generate_random_binary_network_trl(N, num_tree_nodes, num_retis, num_leaves, 0.0);
  }

  if(mstd::test(options,"-a"))
    generate_labels(leaf_labels_only_tag(), N,);

  if(mstd::test(options, "-v"))
    std::cout << N << std::endl;

  const std::string nw_string = get_extended_newick(N);
  if(!options[""].empty()){
    std::ofstream out(options[""][0], (mstd::test(options,"-a") ? std::ios::app : std::ios::out));
    out << nw_string << '\n';
  } else std::cout << nw_string;

}

