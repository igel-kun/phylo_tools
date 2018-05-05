

#pragma once
#include <vector>
#include <unordered_map>
#include <list>
#include <queue>

#include "utils.hpp"

namespace PT{

  typedef std::pair<uint32_t, const std::string&> LabeledNode;
  typedef std::vector<LabeledNode> LNodeVec;
  typedef std::list<uint32_t> IndexList;
  typedef std::pair<uint32_t, uint32_t> IndexPair;
  typedef std::vector<uint32_t> IndexVec;
  typedef std::unordered_map<uint32_t, IndexVec> NeighborMap;
 
  typedef std::pair<uint32_t, uint32_t> Edge;
  typedef std::list<Edge> EdgeList;
  typedef std::vector<Edge> EdgeVec;
  typedef std::deque<Edge> EdgeQueue;
 
  // weights
  typedef std::pair<uint32_t, float> WNode;
  typedef std::vector<WNode> WIndexVec;
  typedef std::unordered_map<uint32_t, WIndexVec> WNeighborMap;

  typedef std::pair<Edge, float> WEdge;
  typedef std::list<WEdge> WEdgeList;
  typedef std::vector<WEdge> WEdgeVec;
  typedef std::deque<WEdge> WEdgeQueue;

  inline uint32_t& get_head(Edge& e) { return e.second; }
  inline uint32_t& get_tail(Edge& e) { return e.first; }
  inline uint32_t& get_head(WEdge& e) { return e.first.second; }
  inline uint32_t& get_tail(WEdge& e) { return e.first.first; }



#warning if there are less than 128 vertices, use a bitset here!
  typedef std::unordered_set<uint32_t> IndexSet;

  typedef std::unordered_map<std::string, IndexPair> LabelMap;
  typedef std::unordered_map<std::string, std::pair<IndexVec, uint32_t>> MULabelMap;
  typedef std::unordered_map<uint32_t, IndexVec> DisplayMap;

  typedef std::vector<std::string> NameVec;
}
