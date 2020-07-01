

#pragma once
#include <vector>
#include <unordered_map>
#include <list>

#include "utils.hpp"
#include "iter_bitset.hpp"
#include "vector_map.hpp"
#include "stl_utils.hpp"

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
  template<class Key, class Value, class InvalidElement = std::default_invalid_t<Value>>
  using ConsecutiveMap = std::vector_map<Key, Value, InvalidElement>;

  using Node = uintptr_t;
  constexpr Node NoNode = std::default_invalid_t<Node>::value();

  template<class Network>
  using NodeTranslation = typename Network::template NodeSet<Node>;
  using ConsecutiveNodeSet = std::ordered_bitset;

  using Degree = uint_fast32_t;
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

 
  template<class Network>
  using NetEdgeVec = std::vector<typename Network::Edge>;
  template<class Network>
  using NetEdgeSet = HashSet<typename Network::Edge>;

}
