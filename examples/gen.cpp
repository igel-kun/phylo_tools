
#include "utils/charp.hpp"
#include "utils/token.hpp"
#include "utils/generic_data.hpp"

#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/net_gen.hpp"

using namespace PT;

enum class DataTarget { Node, Edge };

OptionMap options;

void parse_options(const int argc, const char** argv) {
  OptionDesc description;
  description["-v"] = {0,0};
  description["-n"] = {1,1};
  description["-el"] = {0,0};
  description["-r"] = {1,1};
  description["-l"] = {1,1};
  description["-s"] = {1,1};
  description["-a"] = {0,0};
  description["-L"] = {0,0};
  description["-TBR"] = {2,2};
  description["-ad"] = {1,1};
  description["-nd"] = {1,1};
  description["-ed"] = {1,1};
  description[""] = {0,1};
  const std::string help_message(std::string(argv[0]) + " <out-file>\n\
      generate or modify a network and write it to out-file in extended newick format (unless -el specified)\n\
      FLAGS:\n\
      \t[random binary network generation]\n\
      \t-r <#reti>\tnumber of reticulations in the network\n\
      \t-l <#leaf>\tnumber of leaves in the network\n\
      \t-n <#node>\tnumber of vertices in the network (this is ignored if -r and -l are present)\n\
      NOTE: if, of -n, -r, and -l, less than 2 are present, the network is assumed to have ~10% reticulations\n\
      NOTE: -n, -r, and -l are ignored if -TBR or -ad is present\n\
      NOTE: n = 99 is assumed if none are present\n\n\
      \t[network modification]\n\
      \t-TBR <file> <TBR-dist>\tgenerate a network by applying 'TBR-dist' TBR-moves to the network in 'file'\n\n\
      \t[add random data to a network]\n\
      \t-ad <file>\tgenerate node and/or edge-data for the nodes & edges of the network in <file>\n\
      \t-nd [node-data]\tgenerate node data described in 'node-data' with the format below\n\
      \t-ed [edge-data]\tgenerate edge data described in 'edge-data' with the following format:\n\
      \t\t\t UDx:y - uniformly distributed doubles between x and y\n\
      \t\t\t NDx:y - normal distributed doubles with mean x and std.deviation y\n\
      \t\t\t UIx:y - uniformly distributed ints between x and y\n\
      \t\t\t NIx:y - normal distributed ints with mean x and std.deviation y\n\
      \t\t\t BIx:y:p - binomial distribution (#successes) of ints between x and y with success probability p\n\
      \t\t\t GIx:y:p - geometric distribution (#tries before success) of ints between x and y with success probability p\n\
      \t\t\t Sx:y:k - string of chars between 'x' and 'y' of length k\n\
      \t\t example: \"-ed BI0:10:0.4,UD0.5:0.5,S:a:z:3\" adds to each edge\n\
      \t\t     (1) a binomially distributed integer between 0 & 10 with p=0.4  \n\
      \t\t     (2) a uniformly distributed double between 0.5 and 0.5 (which is always 0.5)\n\
      \t\t     (3) a length-3 string of uniformly random lower-case letters\n\
      \n\t[general]\n\
      \t-v\tverbose output, prints networks\n\
      \t-a\tappend to file1 instead of replacing its contents\n\
      \t-s <seed>\tset random seed to 'seed'\n\
      \t-el\toutput in edgelist format instead of eNewick\n\
      \t-L\tput labels on the leaves (small-letter strings in lexicographic order)\n");

  parse_options(argc, argv, description, help_message, options);

  // sanity check
  if((test(options, "-nd") || test(options, "-ed")) && !test(options, "-ad")) {
    throw std::logic_error("request to modify data requires an input file (via -ad)");
  }
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

// =============== READING Networks =====================

using MyNetwork = DefaultLabeledNetwork<mstd::DefaultDataVec, mstd::DefaultDataVec>;
using MyEdge = typename MyNetwork::Edge;
static_assert(std::is_default_constructible_v<Adjacency<mstd::DefaultDataVec>>);
static_assert(std::is_default_constructible_v<MyEdge>);

auto read_network(const std::string& filename) {
  try {
    return parse_newick<MyNetwork>(std::ifstream{filename});
  } catch(const std::exception& err){
    std::cerr << "Failed reading eNewick from "<< filename <<":\n"<<err.what()<<"\n trying edgelist...\n";
  }
  try {
    return parse_edgelist<MyNetwork>(std::ifstream{filename});
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<< filename <<":\n"<<err.what()<<'\n';
    exit(EXIT_FAILURE);
  }
}



// ============ RNG ===============

template<class StdDist, bool rounding, class... DistConstructArgs>
struct generic_dist {
  std::mt19937 _rng{std::random_device{}()};
  StdDist dist;

  template<class First, class... Args> requires (!mstd::Stringlike<First> && !mstd::TupleType<First>)
  generic_dist(First&& first, Args&&... args): dist{std::forward<First>(first), std::forward<Args>(args)...} {}

  template<mstd::TupleType Tup>
  generic_dist(Tup&& t): generic_dist(std::make_from_tuple<generic_dist>(std::forward<Tup>(t))) 
  {}
  
  generic_dist(const std::string_view s): generic_dist(mstd::read_tuple<DistConstructArgs...>(s, ':')) 
  {}
  
  auto operator()() {
    if constexpr (rounding)
      return std::round(dist(_rng));
    else return dist(_rng);
  }
};
template<class T>
using uniform_rng = generic_dist<std::conditional_t<std::is_integral_v<T>, std::uniform_int_distribution<T>, std::uniform_real_distribution<T>>, false, T, T>;
template<class T>
using normal_rng = generic_dist<std::normal_distribution<double>, std::is_integral_v<T>, double, double>;
template<class T>
using binomial_rng = generic_dist<std::binomial_distribution<int64_t>, false, int64_t, double>;
template<class T>
using geometric_rng = generic_dist<std::geometric_distribution<int64_t>, false, double>;


template<StrictPhylogenyType Phylo, class T>
void append_data(Phylo& N, const NodeDesc u, T&& data) {
  N[u].data().emplace_items(std::piecewise_construct_t{}, std::forward<T>(data));
}
template<StrictPhylogenyType Phylo, class T>
void append_data(Phylo& N, const typename Phylo::Edge& uv, T&& data) {
  uv.data().emplace_items(std::piecewise_construct_t{}, std::forward<T>(data));
}

template<DataTarget target, StrictPhylogenyType Phylo>
auto get_targets(Phylo& N) {
  if constexpr (target == DataTarget::Node) {
    return N.nodes();
  } else {
    return N.edges();
  }
}

template<DataTarget target, StrictPhylogenyType Phylo, class Distribution> requires (!mstd::Stringlike<Distribution>)
void add_random_data(Phylo& N, Distribution&& dist) {
  for(auto x: get_targets<target>(N))
    append_data(N, x, dist());
}

template<DataTarget target, template<class> class Distribution, StrictPhylogenyType Phylo>
void add_random_data(Phylo& N, char val_select, std::string_view s) {
  if(std::string{"0123456789."}.find(s[0]) == std::string::npos)
    throw mstd::MalformedInput{std::string{"Cound not parse distribution information from '"} + s + "'"};
  switch(val_select) {
    case 'I': add_random_data<target>(N, Distribution<int64_t>(s)); break;
    case 'D': add_random_data<target>(N, Distribution<double>(s)); break;
    default: throw mstd::MalformedInput{std::string{"'"} + val_select + "' does not correspond to a valid type; see -h or --help for help"};
  }
}

template<DataTarget target, StrictPhylogenyType Phylo>
void add_random_data(Phylo& N, std::string_view s) {
  DEBUG4(std::cout << "adding data described by '"<<s<<"'\n");
  if(s.size() < 2) throw mstd::MalformedInput{std::string{"cannot interpret data '"} + s + "'. Please see --help or -h for help"};
  switch(s[0]) {
    case 'U': add_random_data<target, uniform_rng>(N, s[1], s.substr(2)); break;
    case 'N': add_random_data<target, normal_rng>(N, s[1], s.substr(2)); break;
    case 'B': add_random_data<target, binomial_rng>(N, s[1], s.substr(2));; break;
    case 'G': add_random_data<target, geometric_rng>(N, s[1], s.substr(2)); break;
    case 'S': {
                const auto [x, y, k] = mstd::read_tuple<char, char, int>(s.substr(1), ':');
                uniform_rng<char> dist(x,y);
                std::cout << "generating "<<k<<" chars between "<<x << " & " << y <<" -- like this: "<<dist()<<dist()<<dist()<<"\n";
                std::string accu;
                for(auto node_or_edge: get_targets<target>(N)) {
                  accu.clear();
                  for(int i = 0; i < k; ++i) accu += dist();
                  append_data(N, node_or_edge, std::move(accu));
                }
              }
              break;
    default: throw mstd::MalformedInput{std::string{"'"} + s[0] + "' does not correspond to a valid type; see -h or --help for help"};
  }
}


int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  if(test(options, "-s"))
    std::srand(arg_from_string(options["-s"][0]));
  
  MyNetwork N;

  if(test(options, "-TBR")) {
    size_t TBR_dist = arg_from_string(options["-TBR"][1]);
    
    N = read_network(options["-TBR"][0]);
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
        sample(N.edges(), 1, &uv);
        // NOTE: we'll be in trouble if there is only 1 edge below t and we choose that one for uv, since then, we can't choose anything for xy
      } while((N.out_degree(t_root) == 1) || (uv.tail() == t_root));

      // step 1.4: choose xy among the edges reachalbe from the root above t in N-st
      while(1) {
        sample(N.edges_below(t_root), 1, &xy);
        if(xy != uv) {
#warning "TODO: continue here"
        }
      }
      throw(mstd::Unimplemented{"sampling method not yet implemented"});
/*
      // step 1.5: check whether x is below v and swap if necessary
      if(N.has_path(xy.tail(), uv.head()))
        std::swap(uv, xy);

      // step 2: apply the TBR-move
      apply_TBR_move(N, st, uv, xy);
*/
    }
  } else if(test(options, "-ad")) {
    N = read_network(options["-ad"][0]);
    // node data
    if(test(options, "-nd"))
      for(const std::string_view s: mstd::tokenize(options["-nd"][0], ','))
        add_random_data<DataTarget::Node>(N, s);
    // edge data
    if(test(options, "-ed"))
      for(const std::string_view s: mstd::tokenize(options["-ed"][0], ','))
        add_random_data<DataTarget::Edge>(N, s); 

  } else {
    long num_nodes, num_retis, num_leaves;
    get_node_numbers(num_nodes, num_retis, num_leaves);
    generate_random_binary_network(N, NumNodes(num_nodes, num_retis, num_leaves));
  }

  if(mstd::test(options,"-L"))
    generate_labels(leaf_labels_only_tag{}, N);

  if(mstd::test(options, "-v"))
    std::cout << "N: " << std::endl << ExtendedDisplay(N) << std::endl;

  std::string output_string = mstd::test(options, "-el") ? get_edgelist(N) : get_extended_newick(N);
  if(!options[""].empty()){
    std::ofstream out(options[""][0], (mstd::test(options,"-a") ? std::ios::app : std::ios::out));
    out << output_string << '\n';
  } else std::cout << output_string << '\n';
}

