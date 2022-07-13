
#include <string>
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"
#include "utils/net_gen.hpp"


using namespace PT;

// a toy struct to demonstrate attaching more complex data (that cannot even be default constructed) to nodes using pointers
struct InformedSequence {
  std::string seq;
  uint32_t num_Ns;
 
  InformedSequence(const std::string& s):
    seq(s), num_Ns(std::count(seq.begin(), seq.end(), 'N'))
  {}

  void update(const std::string& s) {
    seq = s;
    num_Ns = std::count(seq.begin(), seq.end(), 'N');
  }

  friend auto& operator<<(std::ostream& os, const InformedSequence& seq) { return os << seq.seq << " (" << seq.num_Ns <<" Ns)"; }
};

using NumberNetwork = DefaultLabeledNetwork<uint32_t>;
using ISeqNetwork = DefaultNetwork<InformedSequence>;
using ConstISeqNetwork = DefaultNetwork<const InformedSequence>;

const char* bases = "NACGT";

std::string random_seq(const size_t len) {
    std::string result;
    for(uint32_t j = len; j > 0; --j)
      result += bases[throw_die(5)];
    return result;
}


void assign_DFS_numbers(const NodeDesc root, uint32_t& current_num) {
  auto& r_node = node_of<NumberNetwork>(root);
  r_node.data() = current_num++;
  for(const NodeDesc w: r_node.children())
    assign_DFS_numbers(w, current_num);
}


int main(const int argc, const char** argv)
{
  uint32_t number = 0;
  NumberNetwork N;
  //generate_random_binary_network_nr(N, 51, 10);
  generate_random_binary_network_nr(N, 13, 2, 0.0);
  std::cout << N;
  std::cout << "\n\ndone generating network\n\n";

  std::cout << "setting node data for all nodes:\n";
  for(const NodeDesc u: N.nodes()) N[u].data() = number++;
  std::cout << "leaf-nums:\n";
  for(const NodeDesc u: N.leaves()) std::cout << u << ": "<<N[u].data()<<'\n';

  std::cout << "\n\n assigning leaf-taxa\n\n";
  PT::sequential_taxon_name sqn;
  for(const auto& u: N.leaves()) N[u].label() = sqn;
  std::cout << DisplayWithData{N};

  // play with numbered nodes: this gives each node the number in which it occurs in a DFS search (starting at 0)
  std::cout << "\n\n assigning DFS numbers\n\n";
  uint32_t DFS_counter = 0;
  assign_DFS_numbers(N.root(), DFS_counter);
  std::cout << DisplayWithData{N};

  // play with nodes that have a string attached: each node is initialized with a random string
  // to this end, we use the node-data translation function (that takes the node-data of N (aka uint32_t)
  // and returns something from which N2's node-data can be initialized (aka a string))
  std::cout << "\n\n copy & change node-data\n\n";
  ISeqNetwork N2(N, [](const NodeDesc u){ std::cout << "assigning new empty sequences to node "<<u<<"\n"; return ""; } );
  std::cout << DisplayWithData{N2} <<"\n\n";
  for(const NodeDesc& u: N2.leaves()){
    const std::string s = random_seq(10 + throw_die(10));
    auto& data = N2[u].data();
    std::cout << "assigning new data "<<s<<" to leaf "<<u<<'\n';
    data.update(s);
  }
  
  // let's see who got what sequence
  for(const NodeDesc& i: N2.nodes()){
    std::cout << "found data at "<<N2[i]<<" for "<<i<<'\n';
    if(!N2[i].data().seq.empty())
      std::cout << "node "<<i<<" has sequence: "<<N2[i].data().seq<<" with "<<N2[i].data().num_Ns<<" N's"<<std::endl;
    else
      std::cout << "node "<<i<<" has no sequence" <<std::endl;
  }
  std::cout << DisplayWithData{N2} <<"\n";


  // finally, let's see how to deal with to initialize data instead of assigning data
  std::cout << "\n initialize non-assignable node-data\n\n";
  ConstISeqNetwork N3(N, [](const auto&){ return random_seq(10 + throw_die(10)); });
  for(const NodeDesc& i: N3.nodes()){
    if(!N3[i].data().seq.empty())
      std::cout << "node "<<i<<" has sequence: "<<N3[i].data().seq<<" with "<<N3[i].data().num_Ns<<" N's"<<std::endl;
    else
      std::cout << "node "<<i<<" has no sequence" <<std::endl;
  }
  std::cout << DisplayWithData{N3} <<"\n";

  std::cout << "all done\n";
}

