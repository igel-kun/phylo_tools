

#pragma once
#include <vector>
#include <unordered_map>
#include <list>

#include "iter_bitset.hpp"
#include "utils.hpp"

namespace PT{

  // indicate whether a given edgelist can be assumed to contain all nodes in consecutive order
  // (useful for tree/network construction from newick strings)
  struct consecutive_edgelist_tag { };
  constexpr consecutive_edgelist_tag consecutive_edgelist = consecutive_edgelist_tag();
  struct non_consecutive_edgelist_tag { };
  constexpr non_consecutive_edgelist_tag non_consecutive_edgelist = non_consecutive_edgelist_tag();


  typedef std::pair<uint32_t, uint32_t> UIntPair;
  typedef std::pair<uint32_t, const std::string&> LabeledNode;
  typedef std::vector<LabeledNode> LNodeVec;
  typedef std::list<uint32_t> IndexList;
  typedef UIntPair IndexPair;
  typedef std::vector<uint32_t> IndexVec;
  typedef std::unordered_set<uint32_t> IndexSet;
  typedef std::unordered_bitset IndexBitSet;

  typedef std::unordered_map<std::string, IndexPair> LabelMap;
  typedef std::unordered_map<std::string, std::pair<IndexVec, uint32_t>> MULabelMap;
  typedef std::unordered_map<uint32_t, IndexVec> DisplayMap;

  typedef std::vector<std::string> NameVec;

}
