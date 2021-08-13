

#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <list>

#include "utils.hpp"
#include "iter_bitset.hpp"
#include "sorted_vector.hpp"
#include "vector_hash.hpp"
#include "vector_map.hpp"
#include "stl_utils.hpp"
#include "singleton.hpp"
#include "filter.hpp"

namespace PT{

  // adjacency storages
  enum StorageEnum {
    vecS,     // store items in a std::vector
    sortvecS, // store items in a sorted std::vector -- ATTENTION: this is inefficient if your network changes frequently
    setS,     // store items in a std::set
    hashsetS, // store items in a std::unordered_set
    vecsetS,  // store items in a std::vector_hash
    singleS  // store a single item (like the parent for a tree node)
  };

  template<StorageEnum storage, class Element> struct _StorageClass { };
  template<class Element> struct _StorageClass<vecS, Element>     { using type = std::vector<Element>; };
  template<class Element> struct _StorageClass<sortvecS, Element> { using type = std::sorted_vector<Element>; };
  template<class Element> struct _StorageClass<setS, Element>     { using type = std::set<Element>; };
  template<class Element> struct _StorageClass<hashsetS, Element> { using type = std::unordered_set<Element>; };
  template<class Element> struct _StorageClass<vecsetS, Element>  { using type = std::vector_hash<Element>; };
  template<std::PointerType Element> struct _StorageClass<singleS, Element>  { using type = std::singleton_set<std::optional_by_invalid<Element, nullptr>>; };
  template<std::unsigned_integral Element> struct _StorageClass<singleS, Element>  { using type = std::singleton_set<std::optional_by_invalid<Element, -1>>; };
  template<class Element> struct _StorageClass<singleS, Element>  { using type = std::singleton_set<std::optional<Element>>; };
  template<StorageEnum storage, class Element> using StorageClass = typename _StorageClass<storage, Element>::type;



  template<class Key,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  using HashSet = std::unordered_set<Key, Hash, KeyEqual>;

  template<class Key, class Value>
  using RawConsecutiveMap = std::raw_vector_map<Key, Value>;
  template<class Key, class Value, class InvalidElement = std::default_invalid_t<Value>>
  using ConsecutiveMap = std::vector_map<Key, Value, InvalidElement>;

  // node descriptors
#ifndef NDEBUG
  struct NodeDesc {
    uintptr_t data;
    // construct from anything
    template<class T> requires (std::integral<T> || std::is_same_v<T,std::nullptr_t>)
    constexpr NodeDesc(T&& t): data((uintptr_t)t) {}
    constexpr NodeDesc(): NodeDesc(nullptr) {}

    operator uintptr_t() const { return data; }
    
    // we have to forward-declare the output here to avoid having operator<< pick up the implicit conversion to uintptr_t and output the address
    friend std::ostream& operator<<(std::ostream& os, const PT::NodeDesc& nd);
  };
}

namespace std {
  template<>
  struct hash<PT::NodeDesc> {
    std::hash<unsigned long> uinthash;
    size_t operator()(const PT::NodeDesc& x) const { return uinthash(x.data); }
  };
}

namespace PT {
#else
  using NodeDesc = uintptr_t;
#endif

  inline constexpr NodeDesc NoNode = (uintptr_t)nullptr;
  inline const std::string NoName = "";

  template<class = void> class Edge;

  template<class C>
  concept HasNodeValue = std::is_convertible_v<typename std::iterator_traits<std::iterator_of_t<C>>::value_type, NodeDesc>;
  template<class C>
  concept HasNodeKey = std::is_convertible_v<typename std::remove_cvref_t<C>::key_type, NodeDesc>;

  template<class C>
  concept NodeIterableType = (std::IterableType<C> && HasNodeValue<C>);
  template<class C>
  concept NodeContainerType = (std::ContainerType<C> && HasNodeValue<C>);
  template<class C>
  concept NodeSetType = (std::SetType<C> && HasNodeValue<C>);
  template<class C>
  concept NodeMapType = (std::MapType<C> && HasNodeKey<C>);
  template<class C>
  concept OptionalNodeContainerType = (std::is_void_v<C> || (std::ContainerType<C> && HasNodeValue<C>));
  template<class C>
  concept OptionalNodeSetType = (std::is_void_v<C> || (std::SetType<C> && HasNodeValue<C>));
  template<class C>
  concept OptionalNodeMapType = (std::is_void_v<C> || (std::MapType<C> && HasNodeKey<C>));


  // degrees
  using Degree = uint_fast32_t;
  using sw_t = Degree;
  using InOutDegree = std::pair<Degree, Degree>;

  // sets and containers of node descriptors
  using NodeSingleton = StorageClass<singleS, NodeDesc>;
  using ConsecutiveNodeSet = std::ordered_bitset;
  template<class T>
  using NodeWith = std::pair<NodeDesc, T>;
  using NodeWithDegree = NodeWith<Degree>;
  using NodePair = NodeWith<NodeDesc>;
  template<NodeContainerType C>
  using FilteredNodeContainer = std::FilteredIterFactory<C, std::function<bool(const typename C::value_type&)>>;
  template<NodeMapType M>
  using FilteredNodeMap = std::FilteredIterFactory<M, std::function<bool(const typename M::value_type&)>>;


  using NodeVec = std::vector<NodeDesc>;
  using NodeSet = std::unordered_set<NodeDesc>; // TODO: replace by vector_hash ?
  template<class T>
  using NodeMap = std::unordered_map<NodeDesc, T>;
  using NameVec = std::vector<std::string>;

  // an adjacency is something with a NodeDesc field called nd
  template<class A>
  concept _AdjacencyType = requires(A a) {
    { a.nd } -> std::convertible_to<NodeDesc>;
  };
  template<class A>
  concept AdjacencyType = std::is_same_v<NodeDesc, std::remove_cvref_t<A>> || _AdjacencyType<A>;

  template<class A>
  concept AdjacencyContainerType = requires(A a){
    requires std::ContainerType<A>;
    requires AdjacencyType<typename std::remove_cvref_t<A>::value_type>;
  };

  // a node is something providing a bunch of types and whose predecessors and successors can be queried
  template<typename N>
  concept NodeType = requires(N n) {
    requires AdjacencyContainerType<typename std::remove_cvref_t<N>::PredContainer>;
    requires AdjacencyContainerType<typename std::remove_cvref_t<N>::SuccContainer>;
    { n.parents() } -> std::convertible_to<typename std::remove_cvref_t<N>::PredContainer>;
    { n.children() } ->std::convertible_to<typename std::remove_cvref_t<N>::SuccContainer>;
  };
  template<class N>
  concept TreeNodeType = (NodeType<N> && (N::is_tree_node));

  // an Edge is something whose head and tail are Nodes
  template<class T>
  concept EdgeType = requires(T e) {
    { e.head() } -> std::convertible_to<NodeDesc>;
    { e.tail() } -> std::convertible_to<NodeDesc>;
  };
 
  template<class T>
  concept EdgeContainerType = (std::ContainerType<T> && EdgeType<typename T::value_type>);

  template<NodeType _Node> class NodeAccess;

  // everything derived from NodeAccess is a phylogeny
  template<class P>
  concept StrictPhylogenyType = std::is_convertible_v<P, PT::NodeAccess<typename P::Node>>;
  template<class P>
  concept PhylogenyType = StrictPhylogenyType<std::remove_cvref_t<P>>;

  template<class P>
  concept StrictTreeType = (StrictPhylogenyType<P> && P::is_declared_tree);
  template<class P>
  concept TreeType = (PhylogenyType<P> && P::is_declared_tree);

  using NodeTranslation = NodeMap<NodeDesc>;
  template<PhylogenyType Network>
  using NetEdgeVec = std::vector<typename Network::Edge>;
  template<PhylogenyType Network>
  using NetEdgeSet = HashSet<typename Network::Edge>;

  template<class T>
  constexpr bool has_data = std::remove_reference_t<T>::has_data;


  // return the NodeData type of the network, unless it's 'void', in which case return 'Else'
  template<PhylogenyType Net, class Else = bool>
  using NodeDataOr = std::conditional_t<std::is_void_v<typename std::remove_cvref_t<Net>::NodeData>, Else, typename std::remove_cvref_t<Net>::NodeData>;
  // return the EdgeData type of the network, unless it's 'void', in which case return 'Else'
  template<PhylogenyType Net, class Else = bool>
  using EdgeDataOr = std::conditional_t<std::is_void_v<typename std::remove_cvref_t<Net>::EdgeData>, Else, typename std::remove_cvref_t<Net>::NodeData>;

}
