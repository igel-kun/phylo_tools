
#include <string>
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"
#include "utils/generator.hpp"


using namespace PT;

typedef RONetwork<uint32_t> NumberNetwork;

void assign_DFS_numbers(NumberNetwork& N, const Node root, uint32_t& current_num)
{
  N[root] = current_num++;
  for(const Node w: N.children(root))
    assign_DFS_numbers(N, w, current_num);
}

// a toy struct to demonstrate attaching more complex data (that cannot even be default constructed) to nodes using pointers
struct InformedSequence
{
  const std::string seq;
  const uint32_t num_Ns;
 
  InformedSequence() = delete; //<- delete default constructor to proof a point :)

  InformedSequence(const std::string& s):
    seq(s), num_Ns(std::count(seq.begin(), seq.end(), 'N'))
  {}
};

typedef RONetwork<std::shared_ptr<const InformedSequence>> ISeqNetwork;

int main(const int argc, const char** argv)
{
  EdgeVec edges;
  ConsecutiveMap<Node, std::string> names;
  generate_random_binary_edgelist_nr(edges, names, 51, 10);

  // play with numbered nodes: this gives each node the number in which it occurs in a DFS search (starting at 0)
  NumberNetwork N(edges, names);
  uint32_t DFS_counter = 0;
  assign_DFS_numbers(N, N.root(), DFS_counter);

  // play with nodes that have a string attached: each leaf gets a random sequence of NACGT (length between 5 and 20)
  ISeqNetwork N2(edges, names);
  const char* bases = "NACGT";

  for(const Node i: N2.leaves()){
    std::string s;
    for(uint32_t j = 10 + throw_die(10); j > 0; --j)
      s += bases[throw_die(5)];
    auto& data = N2[i];
    std::cout << "assigning new data "<<s<<" to "<<i<<'\n';
    data.reset(new InformedSequence(s));
  }
  
  // let's see who got what sequence
  for(const Node i: N2){
    std::cout << "found data at "<<N2[i]<<" for "<<i<<'\n';
    if(N2[i])
      std::cout << "node "<<i<<" has sequence: "<<N2[i]->seq<<" with "<<N2[i]->num_Ns<<" N's"<<std::endl;
    else
      std::cout << "node "<<i<<" has no sequence" <<std::endl;
  }
    
}

/*
  using _EdgeStorage = typename ISeqNetwork::EdgeStorage;
  using _SuccMap = typename _EdgeStorage::SuccessorMap;
  using _LeafIterFac = typename _EdgeStorage::EmptySuccIterFactory;
  using _LeafContainerRef = typename _EdgeStorage::ConstLeafContainerRef;
  using _Iterator = typename _LeafContainerRef::const_iterator;
  const _EdgeStorage& N2edges = N2.REMOVE_ME();
  const _SuccMap& N2succ = N2edges.REMOVE_ME();
  const _LeafIterFac N2_leaf_iter_fac(N2succ, N2succ);
  const _LeafContainerRef N2leaves(N2_leaf_iter_fac);
  for(_Iterator _i = N2leaves.begin(); _i != N2leaves.end(); ++_i){
//    const Node i = _Iterator::select(*_i.REMOVE_ME());
    const Node i = *_i;
//  for(const Node i: N2leaves){
    std::string s;
    for(uint32_t j = 10 + throw_die(10); j > 0; --j)
      s += bases[throw_die(5)];

    N2[i].reset(new InformedSequence(s));
  }

*/
