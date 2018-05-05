
#include <string>
#include "utils/network.hpp"
#include "utils/generator.hpp"


using namespace PT;

typedef NetworkNodeWithData<uint32_t> NodeWithNumber;
typedef NetworkT<NodeWithNumber> NumberNetwork;

void assign_DFS_numbers(NumberNetwork& N, const uint32_t root, uint32_t& current_num)
{
  NumberNetwork::Node& v = N[root];
  v.data = current_num++;
  for(uint32_t i = 0; i < v.succ.size(); ++i)
    assign_DFS_numbers(N, v.succ[i], current_num);
}

// a toy struct to demonstrate attaching more complex data (that cannot even be default constructed) to nodes using pointers
struct InformedSequence
{
  const std::string seq;
  const uint32_t num_Ns;
 
  InformedSequence(); //<- delete default constructor to proof a point :)

  InformedSequence(const std::string& s):
    seq(s), num_Ns(std::count(seq.begin(), seq.end(), 'N'))
  {}
};

typedef NetworkNodeWithData<InformedSequence*> NodeWithISeq;
typedef NetworkT<NodeWithISeq> ISeqNetwork;

int main(const int argc, const char** argv)
{
  EdgeQueue el;
  std::vector<std::string> names;
  generate_random_binary_edgelist_nr(el, names, 1001, 25);

  // play with numbered nodes: this gives each node the number in which it occurs in a DFS search (starting at 0)
  NumberNetwork N(el, names);
  uint32_t DFS_counter = 0;
  assign_DFS_numbers(N, N.get_root(), DFS_counter);

  // play with nodes that have a string attached: each leaf gets a random sequence of NACGT (length between 5 and 20)
  ISeqNetwork N2(el, names);
  std::unordered_map<uint32_t, InformedSequence> sequences;
  const char* bases = "NACGT";

  for(uint32_t i: N2.get_leaves()){
    std::string s;
    for(uint32_t j = 10 + throw_die(10); j > 0; --j)
      s += bases[throw_die(5)];

    const auto seq_iterator = sequences.emplace(i, s).first;
    N2[i].data = &seq_iterator->second;
  }
  
  // let's see who got what sequence
//  for(

}

