

#pragma once
#include <vector>
#include <unordered_map>
#include <list>

#include "utils.hpp"
#include "iter_bitset.hpp"
#include "vector_map.hpp"

namespace PT{

  // OMG, why is contains() C++20 for unordered_sets/maps but C++17 for normal sets/maps?
  template<class Key,
           class Val,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  class HashMap: public std::unordered_map<Key, Val, Hash, KeyEqual>
  {
    using Parent = std::unordered_map<Key, Val, Hash, KeyEqual>;
  public:
    using Parent::Parent;

#if __cplusplus < 201709L
    inline bool contains(const Key& x) const { return Parent::find() != Parent::end(); }
#endif
  };
/*
  template<
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>>
  class HashSet: public std::unordered_set<Key, Hash, KeyEqual>
  {
    using Parent = std::unordered_set<Key, Hash, KeyEqual>;
  public:
    using Parent::Parent;

#if __cplusplus < 201709L
    inline bool contains(const Key& x) const { return Parent::find() != Parent::end(); }
#endif
  };
*/
  template<
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>>
  using HashSet = std::unordered_set<Key, Hash, KeyEqual>;

  template<class Key, class Value>
  using RawConsecutiveMap = std::raw_vector_map<Key, Value>;
  template<class Key, class Value, class InvalidElement = void, uintptr_t Invalid = -1ul>
  using ConsecutiveMap = std::vector_map<Key, Value, InvalidElement, Invalid>;

  using Node = uintptr_t;
  constexpr Node NoNode = static_cast<Node>(-1);

  using NodeTranslation = HashMap<Node, Node>;
  using ConsecutiveLabelMap = ConsecutiveMap<Node, std::string>;
  using ConsecutiveNodeSet = std::ordered_bitset;

  using Degree = uint_fast32_t;
  using InDegree = Degree;
  using OutDegree = Degree;
  using sw_t = Degree;
  using NodeWithDegree = std::pair<Node, Degree>;
  using InOutDegree = std::pair<Degree, Degree>;

  // indicate whether a given edgelist can be assumed to contain all nodes in consecutive order
  // (useful for tree/network construction from newick strings)
  struct consecutive_tag { };
  constexpr consecutive_tag consecutive_nodes = consecutive_tag();
  struct non_consecutive_tag { };
  constexpr non_consecutive_tag non_consecutive_nodes = non_consecutive_tag();

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
