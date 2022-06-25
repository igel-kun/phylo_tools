
#pragma once

#include "optional_tuple.hpp"
#include "set_interface.hpp"
#include "predicates.hpp"
#include "types.hpp"
#include "tags.hpp"

namespace PT {
  // preorder: emit a node before all nodes below it
  // inorder: emit a node between each two consecutive subtrees below it (f.ex. node 0 with children 1, 2, 3 --> 1 0 2 0 3)
  // postorder: emit a node after all nodes below it
  // add node_traversal or edge_traversal to decide on the traversal type
  // NOTE: these can be combined freely!
  enum TraversalType: uint8_t {
    preorder = 0x01, inorder = 0x02, postorder = 0x04,
    pre_and_inorder = 0x03, pre_and_post_order = 0x05, in_and_post_order = 0x06,
    tail_postorder = 0x08, // NOTE: this is only for use with AllEdgesTraversal
    reverse_traversal = 0x10,
    // edge traversals
    edge_traversal = 0x20,
    // all edge traversals
    all_edge_traversal = 0x40,
    // NOTE: all-edge-tail-postorder is a special all-edge-postorder in which the tails occur in node-post-order; it cannot be combined with other traversals
    all_edge_tail_postorder = 0x80
  };

  constexpr bool is_edge_traversal(const TraversalType tt) { return tt & edge_traversal; }
  constexpr bool is_all_edge_traversal(const TraversalType tt) { return tt & all_edge_traversal; }
  constexpr bool is_reverse_traversal(const TraversalType tt) { return tt & reverse_traversal; }
  constexpr bool is_all_edge_tail_postorder(const TraversalType tt) { return tt & all_edge_tail_postorder; }
  constexpr bool is_node_traversal(const TraversalType tt) { return !is_edge_traversal(tt) && !is_all_edge_traversal(tt) && !is_all_edge_tail_postorder(tt); }

  // this tag can be used as sentinel to avoid the ugly "tree.template node_traversal<preoder>()" notation
  struct proto_dfs_order_tag {};
  template<TraversalType> struct dfs_order_tag: public proto_dfs_order_tag {};
  template<class T>
  concept DFSOrderTag = std::derived_from<std::remove_cvref_t<T>, proto_dfs_order_tag>;

  constexpr dfs_order_tag<preorder> pre_order_t;
  constexpr dfs_order_tag<inorder> in_order_t;
  constexpr dfs_order_tag<postorder> post_order_t;

	// the default set of nodes to track is void for trees
	template<class T>
	struct _DefaultSeenSet {};
  
	template<NodeType Node>
	struct _DefaultSeenSet<Node> { using type = std::conditional_t<TreeNodeType<Node>, void, NodeSet>; };
	template<StrictPhylogenyType Network>
	struct _DefaultSeenSet<Network> { using type = std::conditional_t<TreeNodeType<typename Network::Node>, void, NodeSet>; };
	template<class T> requires (PhylogenyType<T> || NodeType<T>)
	using DefaultSeenSet = typename _DefaultSeenSet<T>::type;


	// NOTE: _SeenSet may be a reference (or even void)
  template<StrictPhylogenyType _Network,
           std::IterableType _ItemContainer,
           OptionalNodeSetType _SeenSet,
           class _Forbidden>
  struct _TraversalTraits:
    public std::optional_tuple<pred::AsContainmentPred<_Forbidden>, _SeenSet>,
    public std::my_iterator_traits<std::iterator_of_t<_ItemContainer>>
  {
    using Parent = std::optional_tuple<pred::AsContainmentPred<_Forbidden>, _SeenSet>;
    using Parent::Parent;

    // NOTE: this forwarding constructor is necessary to construct _TraversalTraits from optional_tuples
    template<class... Args>
    _TraversalTraits(Args&&... args): Parent(std::forward<Args>(args)...) {}

    static constexpr bool has_forbidden = !std::is_void_v<_Forbidden>;
    static constexpr bool has_seen = !std::is_void_v<_SeenSet>;
    static constexpr bool track_nodes = has_seen || has_forbidden;

    using Network = _Network;
    using ItemContainer  = _ItemContainer;
    using iterator_category = std::forward_iterator_tag;
   
    bool is_forbidden(const NodeDesc u) const {
      if constexpr (has_forbidden)
        return this->template get<0>()(u);
      else
        return false;
    }
    // we consider a node 'seen' if it's either seen or forbidden
    bool is_seen(const NodeDesc u) const {
      bool result = is_forbidden(u);
      if constexpr (has_seen) result |= test(this->template get<1>(), u);
      return result;
    }
    void mark_seen(const NodeDesc u) { append(this->template get<1>(), u); }
  };



  template<StrictPhylogenyType _Network, bool reverse>
  using NextNodeContainer = std::conditional_t<reverse, typename _Network::ParentContainer, typename _Network::ChildContainer>;

  template<StrictPhylogenyType _Network,
           OptionalNodeSetType _SeenSet = DefaultSeenSet<_Network>,
           bool reverse = false,
           class _Forbidden = void>
  class NodeTraversalTraits: public _TraversalTraits<_Network, NextNodeContainer<_Network, reverse>, _SeenSet, _Forbidden> {
    using Parent = _TraversalTraits<_Network, NextNodeContainer<_Network, reverse>, _SeenSet, _Forbidden>;
    using IterTraits = std::my_iterator_traits<std::iterator_of_t<NextNodeContainer<_Network, reverse>>>;
  public:
    using typename Parent::Network;
    using typename Parent::ItemContainer;
    using value_type      = const NodeDesc;
    using reference       = value_type;
    using const_reference = reference;
    using pointer         = std::pointer_from_reference<reference>;
    using const_pointer   = std::pointer_from_reference<const_reference>;
    using ItemContainerRef = ItemContainer&;
    using child_iterator  = std::auto_iter<std::iterator_of_t<ItemContainer>>;

    // if there is only one node on the stack (f.ex. if we tried putting a leaf on it), consider it empty
    static constexpr unsigned char min_stacksize = 1;

    static constexpr decltype(auto) get_next_items(const NodeDesc u) {
      if constexpr (reverse)
        return Network::parents(u);
      else return Network::children(u);
    }
    static constexpr size_t num_next_items(const NodeDesc u) {
      return reverse ? Network::in_degree(u) : Network::out_degree(u);
    }
    static constexpr NodeDesc get_node(const NodeDesc u) { return u; }
  };

  template<StrictPhylogenyType _Network, bool reverse>
  using NextEdgeContainer = std::conditional_t<reverse, typename _Network::InEdgeContainer, typename _Network::OutEdgeContainer>;

  template<StrictPhylogenyType _Network,
           OptionalNodeSetType _SeenSet = DefaultSeenSet<_Network>,
           bool reverse = false,
           class _Forbidden = void>
  class EdgeTraversalTraits: public _TraversalTraits<_Network, NextEdgeContainer<_Network, reverse>, _SeenSet, _Forbidden> {
    using Parent = _TraversalTraits<_Network, NextEdgeContainer<_Network, reverse>, _SeenSet, _Forbidden>;
    using EdgeIter = std::iterator_of_t<NextEdgeContainer<_Network, reverse>>;
    using EdgeIterTraits = std::my_iterator_traits<EdgeIter>;
  public:
    // NOTE: the DFS traversal stack will hold auto-iters for iterators into _Network::(Out)EdgeContainer (which is an IterFactory)
    //       such iterators construct edges from the child-adjacencies on the fly when they are de-referenced (rvalues instead of lvalue references).
    //       Note that these edges **DO NOT EXIST IN MEMORY** (only on the return-stack), so we also return rvalues here.
    using Parent::Parent;
    using typename Parent::Network;
    using typename Parent::ItemContainer;
    using value_type      = typename EdgeIterTraits::value_type;
    using reference       = typename EdgeIterTraits::reference;
    using const_reference = typename EdgeIterTraits::const_reference;
    using pointer         = typename EdgeIterTraits::pointer;
    using const_pointer   = typename EdgeIterTraits::const_pointer;
    using ItemContainerRef = ItemContainer;
    using Adjacency = typename _Network::Adjacency;
    using Parent::mark_seen;
    using Parent::is_seen;
    using child_iterator  = std::auto_iter<std::iterator_of_t<ItemContainer>, typename EdgeIter::UnderlyingIterator>;

    // an empty stack represents the end-iterator
    static constexpr unsigned char min_stacksize = 2;

    // NOTE: out_edges returns a temporary iterator factory, so we cannot return a reference to it!
    static constexpr decltype(auto) get_next_items(const NodeDesc u) {
      if constexpr (reverse)
        return Network::in_edges(u);
      else return Network::out_edges(u);
    }
    static constexpr size_t num_next_items(const NodeDesc u) {
      return reverse ? Network::in_degree(u) : Network::out_degree(u);
    }
    static constexpr NodeDesc get_node(const value_type& uv) {
      if constexpr (reverse)
        return uv.tail();
      else return uv.head();
    }
   
    // normally, we want to skip an edge if its head has been seen
    //NOTE: this will give us an edge-list of a DFS-tree
    bool is_seen(const value_type& uv) const { return is_seen(get_node(uv)); }
    void mark_seen(const value_type& uv) { mark_seen(get_node(uv)); }    
  };

  //NOTE: EdgeTraversalTraits gives us the edges of a dfs-tree, but the infrastructure can be used to compute all edges below a node (except some)
  //      For this, however, we'll need to differentiate between forbidden nodes and nodes discovered during the DFS, since the former should not
  //      occur as head of any emitted edge, while the latter should not occur as tail of any emitted edge! Thus, we'll need a second storage
  template<StrictPhylogenyType _Network,
           OptionalNodeSetType _SeenSet = DefaultSeenSet<_Network>,
           bool reverse = false,
           class _Forbidden = void>
  class AllEdgesTraits: public EdgeTraversalTraits<_Network, _SeenSet, reverse, _Forbidden> {
    using Parent = EdgeTraversalTraits<_Network, _SeenSet, reverse, _Forbidden>;
  public:
    using Parent::Parent;
    using typename Parent::Network;
    using typename Parent::value_type;
    using typename Parent::ItemContainerRef;
    using Parent::mark_seen;
    using Parent::is_seen;

    // if u has been seen, just return an empty (Out)EdgeContainer (because all of u's out-edges will be skipped anyways)
    ItemContainerRef get_next_items(const NodeDesc u) const {
      if(!Parent::is_seen(u)) {
        return Parent::get_next_items(u);
      } else return {};
    }

    // so now, we want to skip an edge if its head is forbidden or its tail has been seen during the DFS
    //NOTE: this will give us all edges below some node, except for those with forbidden heads
    bool is_seen(const value_type& uv) const {
      if constexpr (reverse)
        return Parent::is_seen(uv.head()) || Parent::is_forbidden(uv.tail());
      else return Parent::is_seen(uv.tail()) || Parent::is_forbidden(uv.head());
    }
  };

  template<class T>
  concept TraversalTraitsType = requires(T t, const typename T::value_type u, const NodeDesc& v) {
    typename T::Network;
    typename T::ItemContainer;
    T::track_nodes;
    T::min_stacksize;
    t.mark_seen(v);
    { t.is_seen(u) } -> std::same_as<bool>;
  };


}
