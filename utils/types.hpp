

#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <set>
#include <map>
#include <unordered_set>
#include <list>

#include "utils.hpp"
#include "iter_bitset.hpp"
#include "sorted_vector.hpp"
#include "vector_hash.hpp"
#include "vector_map.hpp"
#include "stl_utils.hpp"
#include "set_interface.hpp"
#include "singleton.hpp"

namespace PT{

  // adjacency storages
  enum StorageEnum {
    vecS,     // store items in a std::vector
    sortvecS, // store items in a sorted std::vector
    setS,     // store items in a std::set
    hashsetS, // store items in a std::unordered_set
    multisetS,// store items in a std::unordered_multiset
    vecsetS,  // store items in a std::vector_hash
    singleS  // store a single item (like the parent for a tree node)
  };

  template<StorageEnum storage, class Element> struct _StorageClass { };
  template<class Element> struct _StorageClass<vecS, Element>     { using type = std::vector<Element>; };
  template<class Element> struct _StorageClass<sortvecS, Element> { using type = std::sorted_vector<Element>; };
  template<class Element> struct _StorageClass<setS, Element>     { using type = std::set<Element>; };
  template<class Element> struct _StorageClass<hashsetS, Element> { using type = std::unordered_set<Element>; };
  template<class Element> struct _StorageClass<multisetS, Element>{ using type = std::unordered_multiset<Element>; };
  template<class Element> struct _StorageClass<vecsetS, Element>  { using type = std::vector_hash<Element>; };
  template<std::PointerType Element> struct _StorageClass<singleS, Element>  {
    using type = std::singleton_set<std::optional_by_invalid<Element, nullptr>>;
  };
  template<std::unsigned_integral Element> struct _StorageClass<singleS, Element>  {
    using type = std::singleton_set<std::optional_by_invalid<Element, Element(-1)>>;
  };
  template<class Element> struct _StorageClass<singleS, Element>  {
    using type = std::singleton_set<std::optional<Element>>;
  };
  template<StorageEnum storage, class Element> using StorageClass = typename _StorageClass<storage, Element>::type;

  template<StorageEnum storage>
  constexpr bool is_inplace_modifyable = ((storage == vecS) || (storage == singleS));
  template<StorageEnum storage>
  constexpr bool unique_elements = !((storage == vecS) || (storage == sortvecS) || (storage == multisetS));


  template<class Key,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  using HashSet = std::unordered_set<Key, Hash, KeyEqual>;
  template<class Key,
           class Value,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  using HashMap = std::unordered_map<Key, Value, Hash, KeyEqual>;

  template<class Key, class Value>
  using RawConsecutiveMap = std::raw_vector_map<Key, Value>;
  template<class Key, class Value, class InvalidElement = std::default_invalid_t<Value>>
  using ConsecutiveMap = std::vector_map<Key, Value, InvalidElement>;

  // node descriptors
#ifndef NDEBUG
  struct NodeDesc {
    uintptr_t data = reinterpret_cast<uintptr_t>(nullptr);
    constexpr NodeDesc() {} // std::cout << "creating new ND pointing to "<<data<<"\n"; }
    constexpr NodeDesc(const NodeDesc& other): data(other.data) {} // std::cout << "creating new ND pointing to "<<data<<"\n"; }
    constexpr NodeDesc(NodeDesc&& other): data(std::move(other.data)) {} //std::cout << "creating new ND pointing to "<<data<<"\n"; }

    template<class T>
    constexpr NodeDesc(const T* t): data(reinterpret_cast<uintptr_t>(t)) {} // std::cout << "created ND from pointer to "<<data<<"\n"; }
    constexpr NodeDesc(const nullptr_t n): data(reinterpret_cast<uintptr_t>(static_cast<void*>(n))) {}
    constexpr NodeDesc(const uintptr_t t): data(t) {}


    NodeDesc& operator=(const NodeDesc& other) {
      data = other.data;
      //std::cout << "assigned new ND pointing to "<<data<<"\n";
      return *this;
    }
    NodeDesc& operator=(NodeDesc&& other) {
      data = std::move(other.data);
      //std::cout << "assigned new ND pointing to "<<data<<"\n";
      return *this;
    }

    constexpr operator uintptr_t() const noexcept { return data; }
    
    // we have to forward-declare the output here to avoid having operator<< pick up the implicit conversion to uintptr_t and output the address
    friend std::ostream& operator<<(std::ostream& os, const NodeDesc nd);
  };
}

namespace std {
  template<>
  struct hash<PT::NodeDesc> {
    std::hash<uintptr_t> uinthash;
    size_t operator()(const PT::NodeDesc& x) const noexcept { return uinthash(x.data); }
  };
}

namespace PT {
#else
  using NodeDesc = uintptr_t;
#endif

  constexpr NodeDesc NoNode = NodeDesc{};
  const std::string NoName = "";

  using OptionalNodeDesc = std::optional_by_invalid<NodeDesc, NoNode>;

  template<StorageEnum storage> using NodeStorage = StorageClass<storage, NodeDesc>;

  template<class T>
  concept NodeDescType = std::is_convertible_v<std::remove_cvref_t<T>, NodeDesc>;
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
  template<class C>
  concept NodeTranslationType = (NodeMapType<C> && std::is_same_v<std::mapped_type_of_t<C>, NodeDesc>);

  template<class F> concept StrictNodeFunctionType = std::invocable<F, NodeDesc>;
  template<class F> concept NodeFunctionType = StrictNodeFunctionType<std::remove_reference_t<F>>;
  template<class F> concept OptionalNodeFunctionType = NodeFunctionType<F> || std::is_void_v<F>;

  template<class F> concept StrictNodePredicateType = std::predicate<F, NodeDesc>;
  template<class F> concept NodePredicateType = StrictNodePredicateType<std::remove_reference_t<F>>;
  template<class F> concept OptionalNodePredicateType = NodePredicateType<F> || std::is_void_v<F>;



  template<class T>
  concept StrictDataExtracterType = requires {
    { T::ignoring_node_labels } -> std::convertible_to<const bool>;
    { T::ignoring_edge_data } -> std::convertible_to<const bool>;
    { T::ignoring_node_data } -> std::convertible_to<const bool>;
  };
  template<class T> concept DataExtracterType = StrictDataExtracterType<std::remove_reference_t<T>>;


  // degrees
  using Degree = uint_fast32_t;
  using sw_t = Degree;
  using InOutDegree = std::pair<Degree, Degree>;

  // sets and containers of node descriptors
  using NodeSingleton = StorageClass<singleS, NodeDesc>;
#warning "TODO: find out why this uses std::optional and not optional_by_invalid!"

  using ConsecutiveNodeSet = std::ordered_bitset;
  template<class T>
  using NodeWith = std::pair<NodeDesc, T>;
  using NodeWithDegree = NodeWith<Degree>;
  using NodePair = NodeWith<NodeDesc>;


  using NodeVec = std::vector<NodeDesc>;
  using NodeSet = HashSet<NodeDesc>; // TODO: replace by vector_hash ?
  template<class T>
  using NodeMap = HashMap<NodeDesc, T>;
  using NameVec = std::vector<std::string>;
  using NodePairSet = HashSet<NodePair>;

  // an adjacency is something that can be converted to a NodeDesc
  template<class A>
  concept AdjacencyType = std::is_same_v<std::remove_cvref_t<A>, NodeDesc> ||
    (!std::ArithmeticType<A> && std::is_convertible_v<const A, const NodeDesc&>);

  template<class A>
  concept AdjacencyContainerType = (std::ContainerType<A> && AdjacencyType<typename std::value_type_of_t<A>>);

  // a node is something providing a bunch of types and whose predecessors and successors can be queried
  template<typename N>
  concept StrictNodeType = requires(N n) {
    typename N::PredContainer;
    typename N::SuccContainer;
    requires AdjacencyContainerType<typename N::PredContainer>;
    requires AdjacencyContainerType<typename N::SuccContainer>;
    { n.parents() } -> std::convertible_to<typename N::PredContainer>;
    { n.children() } ->std::convertible_to<typename N::SuccContainer>;
  };

  template<class N> concept NodeType = StrictNodeType<std::remove_cvref_t<N>>;
  template<class N> concept TreeNodeType = (NodeType<N> && (N::is_tree_node));

  // an Edge is something whose head and tail are Nodes
  template<class T>
  concept EdgeType = requires(T e) {
    { e.head() } -> std::convertible_to<NodeDesc>;
    { e.tail() } -> std::convertible_to<NodeDesc>;
  };
  template<class T>
  concept AdjPairType = AdjacencyType<typename std::remove_reference_t<T>::first_type> && AdjacencyType<typename std::remove_reference_t<T>::second_type>;
  // a 'loose' edge type is either an edge or a pair of AdjacencyTypes
  template<class T>
  concept LooseEdgeType = EdgeType<T> || AdjPairType<T>;

  template<class F, class Edge>
  concept EdgeFunctionType = LooseEdgeType<Edge> && std::invocable<F, Edge>;
  template<class F, class Edge>
  concept OptionalEdgeFunctionType = EdgeFunctionType<F, Edge> || std::is_void_v<F>;


  template<class P> 
  concept StrictPhylogenyType = requires(NodeDesc u) {
    typename P::Node;
    typename P::SuccContainer;
    typename P::PredContainer;
    StrictNodeType<typename P::Node>;
    { P::children(u) } -> std::same_as<typename P::SuccContainer&>;
    { P::parents(u) } -> std::same_as<typename P::PredContainer&>;
  };
  template<class P>
  concept PhylogenyType = StrictPhylogenyType<std::remove_cvref_t<P>>;
  template<class P>
  concept OptionalPhylogenyType = (std::is_void_v<std::remove_reference_t<P>> || PhylogenyType<P>);

  template<class P>
  concept StrictTreeType = (StrictPhylogenyType<P> && P::is_declared_tree);
  template<class P>
  concept TreeType = StrictTreeType<std::remove_reference_t<P>>;
  template<class P>
  concept OptionalTreeType = (std::is_void_v<std::remove_reference_t<P>> || TreeType<P>);

  using NodeTranslation = NodeMap<NodeDesc>;
  template<PhylogenyType Network>
  using NetEdgeVec = std::vector<typename Network::Edge>;
  template<PhylogenyType Network>
  using NetEdgeSet = HashSet<typename Network::Edge>;


  template<class T>
  concept EdgeContainerType = (std::ContainerType<T> && EdgeType<typename T::value_type>);

  template<class T>
  constexpr bool has_data = std::remove_reference_t<T>::has_data;

}
