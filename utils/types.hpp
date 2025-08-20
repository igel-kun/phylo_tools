

#pragma once
#include <vector>
#include <functional>
#include <span>
#include <unordered_map>
#include <set>
#include <map>
#include <unordered_set>
#include <list>
#include <limits>

#include "utils.hpp"
#include "runes.hpp"
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
    vecsetS,  // store items in a mstd::vector_hash
    singleS  // store a single item (like the parent for a tree node)
  };

  template<StorageEnum storage, class Element> struct _StorageClass { };
  template<class Element> struct _StorageClass<vecS, Element>     { using type = std::vector<Element>; };
  template<class Element> struct _StorageClass<sortvecS, Element> { using type = mstd::sorted_vector<Element>; };
  template<class Element> struct _StorageClass<setS, Element>     { using type = std::set<Element>; };
  template<class Element> struct _StorageClass<hashsetS, Element> { using type = std::unordered_set<Element>; };
  template<class Element> struct _StorageClass<multisetS, Element>{ using type = std::unordered_multiset<Element>; };
  template<class Element> struct _StorageClass<vecsetS, Element>  { using type = mstd::vector_hash<Element>; };
  template<mstd::PointerType Element> struct _StorageClass<singleS, Element>  {
    using type = mstd::singleton_set<mstd::optional_by_invalid<Element, nullptr>>;
  };
  template<std::unsigned_integral Element> struct _StorageClass<singleS, Element>  {
    using type = mstd::singleton_set<mstd::optional_by_invalid<Element>>;
  };
  template<class Element> struct _StorageClass<singleS, Element>  {
    using type = mstd::singleton_set<std::optional<Element>>;
  };
  template<StorageEnum storage, class Element> using StorageClass = typename _StorageClass<storage, Element>::type;

  template<StorageEnum storage>
  constexpr bool is_inplace_modifyable = ((storage == vecS) or (storage == singleS));
  template<StorageEnum storage>
  constexpr bool unique_elements = not ((storage == vecS) or (storage == sortvecS) or (storage == multisetS));


  template<class Key,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  using HashSet = std::unordered_set<Key, Hash, KeyEqual>;
  template<class Key,
           class Value,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>>
  using HashMap = std::unordered_map<Key, Value, Hash, KeyEqual>;


  // node descriptors
#ifdef DEBUGNODES
  struct NodeDesc {
    uintptr_t data = reinterpret_cast<uintptr_t>(nullptr);
    constexpr NodeDesc() noexcept {} // std::cout << "creating new ND pointing to "<<data<<"\n"; }
    constexpr NodeDesc(const NodeDesc& other) noexcept: data(other.data) {} // std::cout << "creating new ND pointing to "<<data<<"\n"; }
    constexpr NodeDesc(NodeDesc&& other) noexcept: data(std::move(other.data)) {} //std::cout << "creating new ND pointing to "<<data<<"\n"; }

    template<class T>
    constexpr NodeDesc(const T* t) noexcept: data(reinterpret_cast<uintptr_t>(t)) {} // std::cout << "created ND from pointer to "<<data<<"\n"; }
    constexpr NodeDesc(const nullptr_t n) noexcept: data(reinterpret_cast<uintptr_t>(static_cast<void*>(n))) {}
    constexpr NodeDesc(const uintptr_t t) noexcept: data(t) {}


    NodeDesc& operator=(const NodeDesc& other) noexcept {
      data = other.data;
      //std::cout << "assigned new ND pointing to "<<data<<"\n";
      return *this;
    }
    NodeDesc& operator=(NodeDesc&& other) noexcept {
      data = std::move(other.data);
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

  static constexpr NodeDesc NoNode = NodeDesc{};
  static constexpr std::string NoName = "";
}
namespace mstd {
  template<>
  struct default_invalid<PT::NodeDesc> { static constexpr PT::NodeDesc value() { return PT::NoNode; } };
}
namespace PT {

  using OptionalNodeDesc = mstd::optional_by_invalid<NodeDesc>;

  template<> struct _StorageClass<singleS, NodeDesc>  {
    using type = mstd::singleton_set<OptionalNodeDesc>;
  };

  template<StorageEnum storage> using NodeStorage = StorageClass<storage, NodeDesc>;

  // an adjacency is something that can be converted to a NodeDesc
  template<class A, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept AdjacencyType = mstd::is_convertible_v<A, NodeDesc, rune> and (not mstd::is_arithmetic_v<A>);
  template<class A> concept StrictAdjacencyType = AdjacencyType<A, mstd::TR_Strict>;

  template<class A, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept HasAdjacencyValue = AdjacencyType<typename mstd::value_type_of_t<A>, rune>;

  template<class A, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept AdjacencyContainerType = mstd::ContainerType<A, rune> and HasAdjacencyValue<A, rune>;

  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept AdjPairType = (mstd::apply_rune_v<T, rune> or
    (StrictAdjacencyType<typename mstd::apply_rune_t<T, rune>::first_type> and
     StrictAdjacencyType<typename mstd::apply_rune_t<T, rune>::second_type>));


  template<class T> concept has_data = (not std::is_void_v<typename std::remove_reference_t<T>::Data>);
  template<class T> struct _DataOf { using type = void; };
  template<class T> requires has_data<T> struct _DataOf<T> { using type = T::Data; };
  template<class T> using DataOf = typename _DataOf<T>::type;

  template<class C> constexpr bool has_node_value = std::is_convertible_v<mstd::value_type_of_t<C>, NodeDesc>;
  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept HasNodeValue = mstd::apply_rune_v<C, rune> or has_node_value<mstd::apply_rune_t<C, rune>>;

  template<mstd::MapType M> constexpr bool has_node_key = std::is_convertible_v<mstd::key_type_of_t<M>, NodeDesc>;
  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept HasNodeKey = mstd::apply_rune_v<C, rune> or has_node_key<mstd::apply_rune_t<C, rune>>;
  
  template<mstd::MapType M> constexpr bool maps_to_node = std::is_convertible_v<mstd::mapped_type_of_t<M>, NodeDesc>;
  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept MapsToNode = mstd::apply_rune_v<C, rune> or maps_to_node<mstd::apply_rune_t<C, rune>>;
  template<class C> concept StrictMapsToNode = MapsToNode<C, mstd::TR_Strict>;
  template<class C> concept OptionalMapsToNode = MapsToNode<C, mstd::TR_ConstRefVoidOK>;

  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeIterableType = (mstd::IterableType<C, rune> && HasNodeValue<C, rune>);
  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeOrIterableType = (NodeIterableType<C, rune> or AdjacencyType<C, rune>);

  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeContainerType = (mstd::ContainerType<C, rune> && HasNodeValue<C, rune>);  
  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeOrContainerType = (NodeContainerType<C, rune> or AdjacencyType<C, rune>);

  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeSetType = (mstd::SetType<C, rune> && HasNodeValue<C, rune>);
  template<class C> concept StrictNodeSetType = NodeSetType<C, mstd::TR_Strict>;
  template<class C> concept OptionalNodeSetType = NodeSetType<C, mstd::TR_ConstRefVoidOK>;

  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeVecType = (mstd::VectorType<C, rune> && HasNodeValue<C, rune>);
  template<class C> concept StrictNodeVecType = NodeVecType<C, mstd::TR_Strict>;
  template<class C> concept OptionalNodeVecType = NodeVecType<C, mstd::TR_ConstRefVoidOK>;

  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeMapType = (mstd::MapType<C, rune> && HasNodeKey<C, rune>);

  // degrees
  using Degree = uint_fast32_t;
  using sw_t = Degree;
  using Degrees = std::pair<Degree, Degree>;
  constexpr Degree NoDegree = std::numeric_limits<Degree>::max();

  // sets and containers of node descriptors
  using NodeSingleton = StorageClass<singleS, NodeDesc>;

  using ConsecutiveNodeSet = mstd::ordered_bitset;
  template<class T>
  using NodeWith = std::pair<NodeDesc, T>;
  using NodeWithDegree = NodeWith<Degree>;
  using NodePair = NodeWith<NodeDesc>;


  using NodeVec = std::vector<NodeDesc>;
  using NodeSpan = std::span<NodeDesc>;
  using NodeSet = HashSet<NodeDesc>; // TODO: replace by vector_hash ?
  //using NodeSet = mstd::vector_hash<NodeDesc>; // TODO: replace by vector_hash ?
  template<class T>
  using NodeMap = HashMap<NodeDesc, T>;
  using NameVec = std::vector<std::string>;
  using NodePairSet = HashSet<NodePair>;

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

  template<class P> 
  concept StrictPhylogenyType = requires(NodeDesc u) {
    typename P::Node;
    typename P::SuccContainer;
    typename P::PredContainer;
    requires StrictNodeType<typename P::Node>;
    { P::children(u) } -> std::same_as<typename P::SuccContainer&>;
    { P::parents(u) } -> std::same_as<typename P::PredContainer&>;
  };
  template<class P>
  concept PhylogenyType = StrictPhylogenyType<std::remove_cvref_t<P>>;
  template<class P>
  concept OptionalStrictPhylogenyType = (std::is_void_v<P> || StrictPhylogenyType<P>);
  template<class P>
  concept OptionalPhylogenyType = OptionalStrictPhylogenyType<std::remove_reference_t<P>>;

  template<class P>
  concept StrictTreeType = (StrictPhylogenyType<P> && P::is_declared_tree);
  template<class P>
  concept TreeType = StrictTreeType<std::remove_reference_t<P>>;
  template<class P>
  concept OptionalTreeType = (std::is_void_v<std::remove_reference_t<P>> || TreeType<P>);

  // specialize this as you like
  template<class T> struct _NetworkOf {};
  template<StrictPhylogenyType T> struct _NetworkOf<T> { using type = T; };
  template<class T> using NetworkOf = typename _NetworkOf<T>::type;

  using NodeTranslation = NodeMap<NodeDesc>;

  template<PhylogenyType Network>
  using EdgeOf = typename std::remove_reference_t<Network>::Edge;
  template<PhylogenyType Network>
  using NetEdgeVec = std::vector<EdgeOf<Network>>;
  template<PhylogenyType Network>
  using NetEdgeSet = HashSet<EdgeOf<Network>>;

  template<PhylogenyType Network>
  using AdjacencyOf = typename std::remove_reference_t<Network>::Adjacency;
  template<PhylogenyType Network>
  using NetAdjVec = std::vector<AdjacencyOf<Network>>;
  template<PhylogenyType Network>
  using NetAdjSet = HashSet<AdjacencyOf<Network>>;

  // concepts for advanced stuff with nodes
  template<class C, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeTranslationType = (NodeMapType<C, rune> && std::is_same_v<mstd::mapped_type_of_t<C>, NodeDesc>);

  template<class F, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeFunctionType = mstd::is_invocable_v<F, rune, NodeDesc>;

  template<class F> concept StrictNodeFunctionType = NodeFunctionType<F, mstd::TR_Strict>;
  template<class F> concept OptionalNodeFunctionType = NodeFunctionType<F, mstd::TR_ConstRefVoidOK>;

  template<class F, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodePredicateType = mstd::predicate<F, rune, NodeDesc>;

  template<class F, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodePairPredicateType = mstd::predicate<F, rune, NodePair> or mstd::predicate<F, rune, NodeDesc, NodeDesc>;

  template<class F, class Net, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept EdgePredicateType = mstd::predicate<F, rune, EdgeOf<Net>> or
                              mstd::predicate<F, rune, NodeDesc, AdjacencyOf<Net>> or
                              mstd::predicate<F, rune, AdjacencyOf<Net>, NodeDesc>;

  // for LCA oracles, or subtree oracles, etc
  template<class Oracle, class Key, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept OracleType = mstd::is_invocable_v<Oracle, rune, Key> or requires(Oracle o, Key k) { o[k]; };

  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept NodeOracleType = OracleType<T, NodeDesc, rune>;


}


