
#include <string>
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"
#include "utils/generator.hpp"


using namespace PT;

// a toy struct to demonstrate attaching more complex data (that cannot even be default constructed) to nodes using pointers
struct InformedSequence
{
  const std::string seq;
  const uint32_t num_Ns;
 
  InformedSequence(const std::string& s):
    seq(s), num_Ns(std::count(seq.begin(), seq.end(), 'N'))
  {}
};

using NumberNetwork = DefaultNetwork<uint32_t>;
using ISeqNetwork = DefaultNetwork<std::shared_ptr<const InformedSequence>>;


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
  generate_random_binary_network_nr(N, 51, 10);
  PT::sequential_taxon_name sqn;
  for(const NodeDesc u: N.nodes()) N[u].data() = number++;
  for(const NodeDesc u: N.leaves()) N[u].data() = number++;
  auto&& X = N.nodes();
  std::vector<NodeDesc> Nnodes;
  X.append_to(Nnodes);
  std::cout << Nnodes.size() << '\n';
/*
  for(auto iter = X.begin(); iter != X.end(); ++iter)
    N[*iter].name() = sqn;
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
    auto& data = N2[i].data();
    std::cout << "assigning new data "<<s<<" to "<<i<<'\n';
    data = s;
  }
  
  // let's see who got what sequence
  for(const Node i: N2){
    std::cout << "found data at "<<N2[i]<<" for "<<i<<'\n';
    if(N2[i])
      std::cout << "node "<<i<<" has sequence: "<<N2[i]->seq<<" with "<<N2[i]->num_Ns<<" N's"<<std::endl;
    else
      std::cout << "node "<<i<<" has no sequence" <<std::endl;
  }
  */
}

