

#include <algorithm>

#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/generator.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/set_interface.hpp"

#include "utils/network.hpp"
#include "utils/biconnected_comps.hpp"

using namespace PT;

static_assert(std::IterableType<std::unordered_map<PT::NodeDesc, uint32_t>>);

using MyNetwork = DefaultNetwork<>;
using MyBCC = MyNetwork;
//CompatibleNetwork<MyNetwork, void, void, void>;

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

template<class Num>
void check_equality(const std::string& s1, Num&& n1, const std::string& s2, Num&& n2) {
  if(n1 != n2)
    throw std::logic_error(s1 + " = " + std::to_string(n1) + " != " + std::to_string(n2) + " = " + s2);
}
template<class T>
void check_empty(const T& x) {
  if(!x.empty()){
    std::stringstream s;
    s << "expected "<<x<<" to be empty\n";
    throw std::logic_error(s.str());
  }
}

void check_insert(auto& container, const auto& item) {
  std::cout << "--("<<item<<")--\n";
  if(!append(container, item).second)
    throw std::logic_error("double item: " + std::to_string(item));
}

int main(const int argc, const char** argv) {
  std::cout << "parsing options...\n";
  parse_options(argc, argv);

  std::cout << "reading network...\n";
  MyNetwork N(read_network(std::ifstream(options[""][0])));

  std::unordered_set<NodeDesc> nodes, leaves, cut_nodes;
  std::unordered_set<PT::Edge<>> edges, bridges;

  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ leaves -------\n";
  for(const auto& u: N.leaves()) check_insert(leaves, u);

  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ pre-order nodes -------\n";
  for(const auto& u: N.nodes_preorder()) check_insert(nodes, u);
  check_equality("nodes.size()", nodes.size(), "N.num_nodes()", N.num_nodes());

  nodes.clear();
  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ post-order nodes -------\n";
  for(const auto& u: N.nodes_postorder()) check_insert(nodes, u);
  check_equality("nodes.size()", nodes.size(), "N.num_nodes()", N.num_nodes());

  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ pre-order edges -------\n";
  for(const auto uv: N.edges_preorder()) check_insert(edges, uv);
  check_equality("edges.size()", edges.size(), "N.num_edges()", N.num_edges());

  edges.clear();
  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ tail-post-order edges -------\n";
  for(const auto uv: N.edges_tail_postorder()) check_insert(edges, uv);
  check_equality("edges.size()", edges.size(), "N.num_edges()", N.num_edges());
 
  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ vertical cut nodes -------\n";
  for(const auto& u: get_cut_nodes(N)) check_insert(cut_nodes, u);

  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ bridges -------\n";
  for(const auto uv: get_bridges(N)) {
    std::cout << "verifying bridge "<<uv<<"\n";
    check_insert(bridges, uv);
    const auto& [u, v] = uv.as_pair();
    // the endpoints of each bridge should be cut-nodes or leaves (but not both), but there might be cut-nodes that are not incident with bridges
    assert(test(cut_nodes, u));
    assert(test(cut_nodes, v) || test(leaves, v));
  }

  size_t bcc_nodes_total = 0, bcc_edges_total = 0;
  std::cout << "\n" << N << "\n\n";
  std::cout << " ------ vertical biconnected components -------\n";
  size_t num_bccs = 0;
  for(const auto& comp: get_biconnected_components<MyBCC>(N)) {
    std::cout << "BCC #"<<num_bccs<<":\n"<<comp << '\n';
    if(comp.num_edges() > 1) {
      std::cout << "checking for no bridges...\n";
      check_empty(get_bridges(comp));
    }
    bcc_nodes_total += comp.num_nodes();
    bcc_edges_total += comp.num_edges();
    ++num_bccs;
  }
  // if we sum the nodes in all bcc's except their root, we should end up with num_nodes()-1
  std::cout << "total #nodes in BCCs: "<<bcc_nodes_total<<'\n';
  std::cout << "total #edges in BCCs: "<<bcc_edges_total<<'\n';
  std::cout << "#BCCs: "<<num_bccs<<'\n';
  std::cout << "#nodes in N: "<<nodes.size()<<'\n';
  std::cout << "#edges in N: "<<edges.size()<<'\n';
  assert(bcc_nodes_total - num_bccs == nodes.size() - 1);
  // if we sum the edges in all bcc's, we should get the number of edges in N
  assert(bcc_edges_total == edges.size());

  std::cout << "The End\n";
}



