
#include <string>
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"
#include "utils/net_gen.hpp"


using namespace PT;

// a toy struct to demonstrate attaching more complex data (that cannot even be default constructed) to nodes using pointers
struct InformedSequence
{
  std::string seq;
  uint32_t num_Ns;
 
  InformedSequence(const std::string& s):
    seq(s), num_Ns(std::count(seq.begin(), seq.end(), 'N'))
  {}

  void update(const std::string& s) {
    seq = s;
    num_Ns = std::count(seq.begin(), seq.end(), 'N');
  }
};

using NumberNetwork = DefaultLabeledNetwork<uint32_t>;
using ISeqNetwork = DefaultNetwork<InformedSequence>;
using ConstISeqNetwork = DefaultNetwork<const InformedSequence>;


void assign_DFS_numbers(NumberNetwork& N, const NodeDesc root, uint32_t& current_num)
{
  N[root].data() = current_num++;
  for(const NodeDesc w: N.children(root))
    assign_DFS_numbers(N, w, current_num);
}

int main(const int argc, const char** argv)
{
  uint32_t number = 0;
  NumberNetwork N;
  //generate_random_binary_network_nr(N, 51, 10);
  generate_random_binary_network_nr(N, 13, 2);
  N.print_subtree(std::cout);

  for(const NodeDesc u: N.nodes()) N[u].data() = number++;
  std::cout << "leaf-nums:\n";
  for(const NodeDesc u: N.leaves()) std::cout << u << ": "<<N[u].data()<<'\n';

  PT::sequential_taxon_name sqn;
  for(const auto& u: N.nodes())
    N[u].label() = sqn;
  // play with numbered nodes: this gives each node the number in which it occurs in a DFS search (starting at 0)
  uint32_t DFS_counter = 0;
  
  std::cout << "\n\ndone generating network\n\n";
  assign_DFS_numbers(N, N.root(), DFS_counter);

  // play with nodes that have a string attached: each leaf gets a random sequence of NACGT (length between 5 and 20)
  ISeqNetwork N2(N);
  const char* bases = "NACGT";

  for(const NodeDesc u: N2.leaves()){
    std::string s;
    for(uint32_t j = 10 + throw_die(10); j > 0; --j)
      s += bases[throw_die(5)];
    auto& data = N2[u].data();
    std::cout << "assigning new data "<<s<<" to "<<u<<'\n';
    data.update(s);
  }
  
  // let's see who got what sequence
  for(const NodeDesc i: N2.nodes()){
    std::cout << "found data at "<<N2[i]<<" for "<<i<<'\n';
    if(!N2[i].data().seq.empty())
      std::cout << "node "<<i<<" has sequence: "<<N2[i].data().seq<<" with "<<N2[i].data().num_Ns<<" N's"<<std::endl;
    else
      std::cout << "node "<<i<<" has no sequence" <<std::endl;
  }


  // finally, let's see how to deal with to initialize data instead of assigning data
  ConstISeqNetwork N3(N);
  for(const NodeDesc i: N3.nodes()){
    std::cout << "found data at "<<N3[i]<<" for "<<i<<'\n';
    if(!N3[i].data().seq.empty())
      std::cout << "node "<<i<<" has sequence: "<<N3[i].data().seq<<" with "<<N3[i].data().num_Ns<<" N's"<<std::endl;
    else
      std::cout << "node "<<i<<" has no sequence" <<std::endl;
  }
}

