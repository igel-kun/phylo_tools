
#pragma once

#include "set_interface.hpp"
#include "types.hpp"

namespace PT {
  // preorder: emit a node before all nodes below it
  // inorder: emit a node between each two consecutive subtrees below it (f.ex. node 0 with children 1, 2, 3 --> 1 0 2 0 3)
  // postorder: emit a node after all nodes below it
  // NOTE: these can be combined freely!
  enum TraversalType {preorder = 1, inorder = 2, postorder = 4, pre_and_in_order = 3, pre_and_post_order = 5, in_and_post_order = 6, all_order = 7};

  // this template can be used as sentinel to avoid the ugly "tree.template node_traversal<preoder>()" notation
  template<TraversalType> struct order{};

	// the default set of nodes to track is void for trees
	template<class T>
	struct _DefaultSeenSet {};
  
	template<NodeType Node>
	struct _DefaultSeenSet<Node> {
		using type = std::conditional_t<TreeNodeType<Node>, void, NodeSet>;
	};
	template<PhylogenyType Network>
	struct _DefaultSeenSet<Network> {
		using type = std::conditional_t<TreeNodeType<typename Network::Node>, void, typename Network::NodeSet>;
	};
	template<class T> requires (PhylogenyType<T> || NodeType<T>)
	using DefaultSeenSet = typename _DefaultSeenSet<T>::type;


	// NOTE: _SeenSet and _ForbiddenSet may both be references (or even void)
  template<PhylogenyType _Network,
           std::IterableType _ItemContainer,
           OptionalNodeSetType _SeenSet,
           OptionalNodeSetType _ForbiddenSet>
  class _TraversalTraits: public std::my_iterator_traits<std::iterator_of_t<_ItemContainer>> {
  protected:
    _SeenSet seen;
    _ForbiddenSet forbidden;
  public:
    static constexpr bool track_seen = true;
    using Network = _Network;
    using ItemContainer  = _ItemContainer;
    using child_iterator    = std::iterator_of_t<ItemContainer>;
    using iterator_category = std::forward_iterator_tag;

    template<class FSet, class... Args>
    _TraversalTraits(FSet&& _forbidden, Args&&... args): forbidden(std::forward<FSet>(_forbidden)), seen(std::forward<Args>(args)...) {}
   
    bool is_seen(const NodeDesc u) const { return test(seen, u) || test(forbidden, u); }
    void mark_seen(const NodeDesc u) { if(node_of<Network>(u).in_degree() > 1) append(seen, u); }
  };

  template<PhylogenyType _Network,
           std::IterableType _ItemContainer,
           OptionalNodeSetType _SeenSet>
  class _TraversalTraits<_Network, _ItemContainer, _SeenSet, void>: public std::my_iterator_traits<std::iterator_of_t<_ItemContainer>> {
  protected:
    _SeenSet seen;
  public:
    static constexpr bool track_seen = true;
    using Network = _Network;
    using ItemContainer  = _ItemContainer;
    using child_iterator    = std::iterator_of_t<ItemContainer>;
    using iterator_category = std::forward_iterator_tag;

    template<class... Args>
    _TraversalTraits(Args&&... args): seen(std::forward<Args>(args)...) {}
   
    bool is_seen(const NodeDesc u) const { return test(seen, u); }
    void mark_seen(const NodeDesc u) { if(node_of<Network>(u).in_degree() > 1) append(seen, u); }
  };

  template<PhylogenyType _Network,
           std::IterableType _ItemContainer,
           OptionalNodeSetType _ForbiddenSet>
  class _TraversalTraits<_Network, _ItemContainer, void, _ForbiddenSet>: public std::my_iterator_traits<std::iterator_of_t<_ItemContainer>> {
  protected:
    _ForbiddenSet forbidden;
  public:
    static constexpr bool track_seen = true;
    using Network = _Network;
    using ItemContainer  = _ItemContainer;
    using child_iterator    = std::iterator_of_t<ItemContainer>;
    using iterator_category = std::forward_iterator_tag;

    template<class... Args>
    _TraversalTraits(Args&&... args): forbidden(std::forward<Args>(args)...) {}
   
    bool is_seen(const NodeDesc u) const { return test(forbidden, u); }
    static constexpr void mark_seen(const NodeDesc u) { }
  };

  template<PhylogenyType _Network,
           std::IterableType _ItemContainer>
  class _TraversalTraits<_Network, _ItemContainer, void, void>: public std::my_iterator_traits<std::iterator_of_t<_ItemContainer>> {
  public:
    static constexpr bool track_seen = false;
    using Network = _Network;
    using ItemContainer  = _ItemContainer;
    using child_iterator    = std::iterator_of_t<ItemContainer>;
    using iterator_category = std::forward_iterator_tag;

    template<class... Args>
    _TraversalTraits(Args&&... args) {}
   
    static constexpr bool is_seen(const NodeDesc u) { return false; }
    static constexpr void mark_seen(const NodeDesc u) { }
  };






  template<PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = DefaultSeenSet<_Network>,
           OptionalNodeSetType _ForbiddenSet = void>
  class NodeTraversalTraits: public _TraversalTraits<_Network, typename _Network::ChildContainer, _SeenSet, _ForbiddenSet>
  {
    using Parent = _TraversalTraits<_Network, typename _Network::ChildContainer, _SeenSet, _ForbiddenSet>;
  public:
    using Parent::Parent;
    using typename Parent::Network;
    using typename Parent::ItemContainer;
    // node traversals return fabricated rvalue nodes, no reason to fiddle with adjacency-data here, we're doing a Node traversal!
    using value_type      = NodeDesc;
    using reference       = value_type;
    using const_reference = value_type;
    using pointer         = std::self_deref<value_type>;
    using const_pointer   = pointer;

    // if there is only one node on the stack (f.ex. if we tried putting a leaf on it), consider it empty
    static constexpr unsigned char min_stacksize = 1;

    static constexpr ItemContainer& get_children(const Network& N, const NodeDesc u) { return N.children(u); }
    static constexpr NodeDesc get_node(const NodeDesc u) { return u; }
  };

  template<PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = DefaultSeenSet<_Network>,
           OptionalNodeSetType _ForbiddenSet = void>
  class EdgeTraversalTraits: public _TraversalTraits<_Network, typename _Network::OutEdgeContainerRef, _SeenSet, _ForbiddenSet>
  {
    using Parent = _TraversalTraits<_Network, typename _Network::OutEdgeContainerRef, _SeenSet, _ForbiddenSet>;
  public:
    using Parent::Parent;
    using typename Parent::Network;
    using typename Parent::ItemContainer;
    using value_type      = typename Network::Edge;
    using reference       = value_type;
    using const_reference = const value_type;
    using pointer         = std::self_deref<value_type>;
    using const_pointer   = pointer;

    // an empty stack represents the end-iterator
    static constexpr unsigned char min_stacksize = 2;

    // NOTE: out_edges returns a temporary iterator factory, so we cannot return a reference to it!
    static constexpr ItemContainer get_children(Network& N, const NodeDesc u) { return N.out_edges(u); }
    static constexpr NodeDesc get_node(const value_type& uv) { return uv.head(); }
   
    // normally, we want to skip an edge if its head has been seen
    //NOTE: this will give us an edge-list of a DFS-tree
    bool is_seen(const value_type& uv) const { return Parent::is_seen(uv.head()); }
    void mark_seen(const value_type& uv) { Parent::mark_seen(uv.head()); }    
  };

  //NOTE: EdgeTraversalTraits gives us the edges of a dfs-tree, but the infrastructure can be used to compute all edges below a node (except some)
  //      For this, however, we'll need to differentiate between forbidden nodes and nodes discovered during the DFS, since the former should not
  //      occur as head of any emitted edge, while the latter should not occur as tail of any emitted edge! Thus, we'll need a second storage
  template<PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = DefaultSeenSet<_Network>,
           OptionalNodeSetType _ForbiddenSet = void>
  class AllEdgesTraits: public EdgeTraversalTraits<_Network, _SeenSet, _ForbiddenSet>
  {
    using Parent = EdgeTraversalTraits<_Network, _SeenSet, _ForbiddenSet>;
  public:
    using Parent::Parent;
    using typename Parent::Network;
    using typename Parent::value_type;
    using typename Parent::ItemContainer;

    // if u has been seen, just return an empty OutEdgeContainer (because all of u's out-edges will be skipped anyways)
    ItemContainer get_children(Network& N, const NodeDesc u) const { if(Parent::is_seen(u)) return {}; else return N.out_edges(u); }

    // so now, we want to skip an edge if its head is forbidden or its tail has been seen during the DFS
    //NOTE: this will give us all edges below some node, except for those with forbidden heads
    bool is_seen(const value_type& uv) const { return Parent::is_seen(uv.tail()); }
  };

  template<class T>
  concept TraversalTraitsType = requires(T&& t, const typename T::value_type u) {
    typename T::Network;
    typename T::ItemContainer;
    T::track_seen;
    T::min_stacksize;
    { t.mark_seen(u) } -> std::same_as<void>;
    { t.is_seen(u) } -> std::same_as<bool>;
  };


}
