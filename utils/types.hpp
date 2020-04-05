

#pragma once
#include <vector>
#include <unordered_map>
#include <list>

#include "utils.hpp"
#include "iter_bitset.hpp"
#include "vector_map.hpp"

namespace PT{

  template<class Key,
           class Val,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  using HashMap = std::unordered_map<Key, Val, Hash, KeyEqual>;

  template<class Key, class Value>
  using ConsecutiveMap = std::vector_map<Key, Value>;

  template<
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>>
  using HashSet = std::unordered_set<Key, Hash, KeyEqual>;

  using Node = void*;
  const void* const NoNode(reinterpret_cast<const void* const>(-1));

  using NodeTranslation = HashMap<Node, Node>;
  using ConsecutiveLabelMap = ConsecutiveMap<Node, std::string>;

  using Degree = uint_fast32_t;
  using InOutDeg = std::pair<Degree, Degree>;
  using NodeWithDegree = std::pair<Node, Degree>;
  using InDegreeMap = HashMap<Node, Degree>;
  using OutDegreeMap = HashMap<Node, Degree>;
  using InOutDegreeMap = HashMap<Node, InOutDeg>;
  

  // indicate whether a given edgelist can be assumed to contain all nodes in consecutive order
  // (useful for tree/network construction from newick strings)
  struct consecutive_edgelist_tag { };
  constexpr consecutive_edgelist_tag consecutive_edgelist = consecutive_edgelist_tag();
  struct non_consecutive_edgelist_tag { };
  constexpr non_consecutive_edgelist_tag non_consecutive_edgelist = non_consecutive_edgelist_tag();

  template<class Property = const std::string>
  using LabeledNode = std::pair<Node, Property>;
  template<class Property = const std::string>
  using LabeledNodeVec = std::vector<LabeledNode<Property>>;

  using NameVec = std::vector<std::string>;

  using NodePair = std::pair<Node, Node>;
  using NodeVec = std::vector<Node>;
  using NodeSet = HashSet<Node>;

 
  template<class Network>
  using NetEdgeVec = std::vector<typename Network::Edge>;
  template<class Network>
  using NetEdgeSet = HashSet<typename Network::Edge>;

}
