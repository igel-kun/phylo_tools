

#pragma once
#include <vector>
#include <unordered_map>
#include <list>

#include "iter_bitset.hpp"
#include "utils.hpp"

namespace PT{

  typedef std::pair<uint32_t, const std::string&> LabeledNode;
  typedef std::vector<LabeledNode> LNodeVec;
  typedef std::list<uint32_t> IndexList;
  typedef std::pair<uint32_t, uint32_t> IndexPair;
  typedef std::vector<uint32_t> IndexVec;
  typedef std::unordered_set<uint32_t> IndexSet;
  typedef std::iterable_bitset IndexBitSet;

  typedef std::unordered_map<std::string, IndexPair> LabelMap;
  typedef std::unordered_map<std::string, std::pair<IndexVec, uint32_t>> MULabelMap;
  typedef std::unordered_map<uint32_t, IndexVec> DisplayMap;

  typedef std::vector<std::string> NameVec;


}
