
#pragma once

#include "stl_utils.hpp"
#include "optional_tuple.hpp"

#include "iter_factory.hpp"

namespace PT {
  using TraversalType = uint8_t;

  constexpr TraversalType preorder = 0x01; // preorder traversal will output a node before any of its descendants
  constexpr TraversalType postorder = 0x02; // postorder traversal will output a node after all its descendants
  constexpr TraversalType inorder = 0x04; // inorder traversal will output a node before *EVERY* child except the first and if there is no child
  constexpr TraversalType pre_and_post_order = preorder + postorder;
  constexpr TraversalType reverse_traversal = 0x10; // reverse traversals go bottom-up instead of top-down
  constexpr TraversalType edge_traversal = 0x20; // an edge traversal produces the edges of a DFS-tree
  constexpr TraversalType all_edge_traversal = 0x40; // an all-edge traversal produces all edges of the network

#ifdef DFSCORO
  constexpr TraversalType depth_last_traversal = 0x80; // a depth-last traversal is only allowed to process a node when all parents have been processed
  constexpr bool is_depth_last_traversal(const TraversalType tt) { return tt & depth_last_traversal; }
#else
  constexpr TraversalType all_edge_tail_postorder = 0x80;
  constexpr bool is_depth_last_traversal(const TraversalType tt) { return false; }
#endif

  constexpr bool is_preorder_traversal(const TraversalType tt) { return tt & preorder; }
  constexpr bool is_postorder_traversal(const TraversalType tt) { return tt & postorder; }
  constexpr bool is_inorder_traversal(const TraversalType tt) { return tt & inorder; }

  constexpr bool is_edge_traversal(const TraversalType tt) { return tt & edge_traversal; }
  constexpr bool is_all_edge_traversal(const TraversalType tt) { return tt & all_edge_traversal; }
  constexpr bool is_reverse_traversal(const TraversalType tt) { return tt & reverse_traversal; }
  constexpr bool is_node_traversal(const TraversalType tt) { return not (is_edge_traversal(tt)) and (not is_all_edge_traversal(tt)); }

  auto spell_out_traversal(const TraversalType tt) {
    std::ostringstream os;
    if(tt & reverse_traversal) os << "reverse ";
    if(tt & edge_traversal) os << "edge ";
    if(tt & all_edge_traversal) os << "all-edge ";
    if(is_node_traversal(tt)) os << "node ";
#ifdef DFSCORO    
    if(tt & depth_last_traversal) os << "depth-last ";
#endif
    if(tt & preorder) os << "preorder ";
    if(tt & postorder) os << "postorder ";
    if(tt & inorder) os << "inorder ";
    return std::move(os).str();
  }


  // this tag can be used as sentinel to avoid the ugly "tree.template node_traversal<preoder>()" notation
  struct proto_dfs_order_tag {};
  template<TraversalType> struct dfs_order_tag: public proto_dfs_order_tag {};
  template<class T>
  concept DFSOrderTag = std::derived_from<std::remove_cvref_t<T>, proto_dfs_order_tag>;

  constexpr dfs_order_tag<preorder> pre_order_t;
  constexpr dfs_order_tag<inorder> in_order_t;
  constexpr dfs_order_tag<postorder> post_order_t;


  // the seen set may be a set of nodes or a map indexed by nodes
  template<class S, mstd::TypeRune rune = mstd::TR_PtrVoidOK>
  concept DFSSeenType = NodeSetType<S, rune> or NodeMapType<S, rune>;

	// the default set of nodes to track is void for trees
	template<class T, TraversalType tt> struct DefaultSeenSet_ {};
	template<class T, TraversalType tt> struct DefaultSeenMap_ {};
  
	template<NodeType Node, TraversalType tt>
	struct DefaultSeenSet_<Node, tt> { using type = std::conditional_t<TreeNodeType<Node> and not is_reverse_traversal(tt), void, NodeSet>; };
	template<StrictPhylogenyType Network, TraversalType tt>
	struct DefaultSeenSet_<Network, tt>: public DefaultSeenSet_<typename Network::Node, tt> {};

  template<NodeType Node, TraversalType tt>
	struct DefaultSeenMap_<Node, tt> { using type = std::conditional_t<TreeNodeType<Node> and not is_reverse_traversal(tt), void, NodeMap<Degree>>; };
	template<StrictPhylogenyType Network, TraversalType tt>
	struct DefaultSeenMap_<Network, tt>: public DefaultSeenMap_<typename Network::Node, tt> {};

	template<class T, TraversalType tt> requires (PhylogenyType<T> or NodeType<T>)
	using DefaultSeenSet = std::conditional_t<is_depth_last_traversal(tt),
        typename DefaultSeenMap_<std::remove_cvref_t<T>, tt>::type, typename DefaultSeenSet_<T, tt>::type>;

  // the root storage is either a non-owning auto_iter if we don't own the roots, or a poppable root container if we do
  // NOTE: const root containers are not allowed! You should pass them as pointer to indicate that the traversal DOES NOT own the roots!
  template<class Roots>  struct ProtoDFSRootStorage {};
  // pointer
  template<mstd::IterableType<mstd::TR_ConstOK> Roots>
  struct ProtoDFSRootStorage<Roots*> { using type = mstd::IterFactory<Roots>; };
  // container
  template<mstd::ContainerType<mstd::TR_Strict> Roots> requires mstd::is_poppable<Roots>
  struct ProtoDFSRootStorage<Roots> { using type = Roots; };
  // non-container iterables
  template<mstd::IterableType<mstd::TR_Strict> Roots> requires (not mstd::ContainerType<Roots, mstd::TR_Strict> or not mstd::is_poppable<Roots>)
  struct ProtoDFSRootStorage<Roots> { using type = mstd::IterFactory<Roots>; };
  // nodes
  template<> struct ProtoDFSRootStorage<NodeDesc> { using type = NodeSingleton; };
  template<> struct ProtoDFSRootStorage<NodeDesc*> { using type = mstd::IterFactory<NodeDesc*>; };

  template<NodeOrIterableType<mstd::TR_PtrOK> Roots>
  using DFSRootStorage = typename ProtoDFSRootStorage<Roots>::type;





  // store roots, forbidden and seen
  template<NodeOrIterableType<mstd::TR_PtrOK> Roots_,
           class Forbidden_,
           DFSSeenType SeenSet_>
    requires (std::is_pointer_v<Roots_> or mstd::is_poppable<DFSRootStorage<Roots_>>)
  struct DFSInfo: 
    public mstd::optional_tuple<Forbidden_, SeenSet_>
  {
    // ------- static stuff --------
    using Forbidden = Forbidden_;
    using SeenSet = SeenSet_;
    using Parent = mstd::optional_tuple<Forbidden, SeenSet>;

    static constexpr bool has_forbidden = not std::is_void_v<Forbidden>;
    static constexpr bool has_seen = not std::is_void_v<SeenSet_>;

    // if Roots_ is a NodeDesc*, we'll still just use a NodeSingleton, so no indirection
    static constexpr bool roots_indirect = std::is_pointer_v<Roots_>;
    static constexpr bool forbidden_indirect = std::is_pointer_v<Forbidden>;
    static constexpr bool seen_indirect = std::is_pointer_v<SeenSet>;

    using Roots = DFSRootStorage<Roots_>;
    static_assert(mstd::IterableType<Roots>);
    static_assert(not std::is_const_v<Roots>);
    static_assert(not std::is_pointer_v<Roots_> or mstd::is_derived_from_template_v<Roots, mstd::_auto_iter>);

    // ------- members --------
    Roots roots;


    // ------- construction & desctruction ---------
    DFSInfo() = default;

    template<StrictPhylogenyType Phylo, class... Args> requires (not mstd::is_same_v<Roots_, NodeDesc*>)
    DFSInfo(const Phylo& N, Args&&... args):
      Parent{std::forward<Args>(args)...},
      roots(N.roots())
    {}

    template<NodeOrIterableType RootsInit, class... Args> requires (not mstd::is_same_v<Roots_, NodeDesc*>)
    DFSInfo(RootsInit&& _roots, Args&&... args):
      Parent{std::forward<Args>(args)...},
      roots(std::forward<RootsInit>(_roots))
    {}

    // if our root storage is just a NodeDesc*, then we'll only accept a NodeDesc* and we'll set the end of the auto_iter to one after _roots
    template<class... Args> requires (mstd::is_same_v<Roots_, NodeDesc*>)
    DFSInfo(const NodeDesc* _roots, Args&&... args):
      Parent{std::forward<Args>(args)...},
      roots(_roots, _roots + 1)
    {}

    // initialization of indirections from containers is already implemented in mstd::optional_tuple,
    // and we'll forbid initializing containers from indirections (for now)
    template<NodeOrIterableType<mstd::TR_PtrOK> OtherRoots_,
            class OtherForbidden_,
            DFSSeenType OtherSeenSet_>
      requires (not std::is_same_v<DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<OtherRoots_>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) or (forbidden_indirect >= std::is_pointer_v<OtherForbidden_>)) and
          ((not has_seen) or (seen_indirect >= std::is_pointer_v<OtherSeenSet_>)))
    DFSInfo(const DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>& other):
      Parent(static_cast<const typename DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>::Parent&>(other)),
      roots(other.roots)
    {}
    template<NodeOrIterableType<mstd::TR_PtrOK> OtherRoots_,
            class OtherForbidden_,
            DFSSeenType OtherSeenSet_>
      requires (not std::is_same_v<DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<OtherRoots_>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) or (forbidden_indirect >= std::is_pointer_v<OtherForbidden_>)) and
          ((not has_seen) or (seen_indirect >= std::is_pointer_v<OtherSeenSet_>)))
    DFSInfo(DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>& other):
      Parent(static_cast<typename DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>::Parent&>(other)),
      roots(other.roots)
    {}

    template<NodeOrIterableType<mstd::TR_PtrOK> OtherRoots_,
            class OtherForbidden_,
            DFSSeenType OtherSeenSet_>
      requires (not std::is_same_v<DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<OtherRoots_>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) || (forbidden_indirect >= std::is_pointer_v<OtherForbidden_>)) and
          ((not has_seen) || (seen_indirect >= std::is_pointer_v<OtherSeenSet_>)))
    DFSInfo(DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>&& other):
      Parent(static_cast<typename DFSInfo<OtherRoots_, OtherForbidden_, OtherSeenSet_>::Parent&&>(other)),
      roots(std::move(other.roots))
    {}
    
    // ------- operators --------
    bool operator!=(const DFSInfo& other) const {
      if constexpr (has_seen)
        if(get_seen() != other.get_seen()) return true;
      return roots != other.roots;
    }
    bool operator==(const DFSInfo& other) const { return not operator!=(other); }

    // ------- methods: initialization --------
    // ------- methods: query --------
  public:
    const auto& get_roots() const { return roots; }
    const auto& get_forbidden() const requires (has_forbidden) { return mstd::access(this->template get<0>()); }
    auto& get_forbidden() requires (has_forbidden) { return mstd::access(this->template get<0>()); }
    const auto& get_seen() const requires (has_seen) { return mstd::access(this->template get<1>()); }

    bool is_forbidden(const auto x) const {
      if constexpr (has_forbidden) {
        return mstd::test(get_forbidden(), x);
      } else return false;
    }
    
    bool is_seen(const NodeDesc u) const {
      if constexpr (has_seen) {
        return mstd::test(get_seen(), u);
      } else return false;
    }

    auto get_current_root() const {
      if constexpr (roots_indirect) {
        return *get_roots();
      } else if constexpr (mstd::HasBack<Roots> and mstd::HasPopBack<Roots>) {
        return mstd::back(get_roots());
      } else return mstd::front(get_roots());
    }
    bool roots_spent() const {
      if constexpr (roots_indirect or mstd::VerifyableIter<Roots>)
        return get_roots().is_invalid();
      else return get_roots().empty();
    }

  protected:
    auto& get_seen() requires (has_seen) { return mstd::access(this->template get<1>()); }
    auto& get_roots() { return roots; }

    // ------- methods: modification --------
  protected:
    template<class... Args>
    void mark_seen(const NodeDesc u, Args&&... args) { 
      if constexpr (has_seen) {
        mstd::append(get_seen(), u, std::forward<Args>(args)...);
        DEBUG6(std::cout << "seen: "<< get_seen() << '\n');
      }
    }

    void pop_root() {
      if constexpr (roots_indirect) // if our roots are indirect, then we have an auto_iter onto the container, so advance that one
        ++get_roots();
      else mstd::pop_back(get_roots()); // if our roots are direct, then pop the last root
    }
  };

  static_assert(std::is_copy_constructible_v<DFSInfo<NodeDesc, void, NodeSet*>>);
  static_assert(std::is_copy_assignable_v<DFSInfo<NodeDesc, void, NodeSet*>>);
  static_assert(std::is_move_constructible_v<DFSInfo<NodeDesc, void, NodeSet*>>);
  static_assert(std::is_move_assignable_v<DFSInfo<NodeDesc, void, NodeSet*>>);


  // helper structure for resuming the iteration; it has to be the same for all template-instanciations, so it needs to be outside the class
  template<bool doing_inorder_traversal>
  struct resume_info_t {
    uint8_t current_pos = 0; // this contains the point at which we want to resume iteration
  };
  template<>
  struct resume_info_t<true>:
    public resume_info_t<false> 
  {
    // in inorder traversals, we have to keep track of the number of successful descents because:
    //    if we have 0 successful descents, then we want to output the parent on the ascension, but not before the next descent
    //    if we have 1 successful descent, then we want to output the parent on the ascension, and before the next descent
    //    if we have >1 successful descent, then we don't want to output the parent on the ascension, but before the next descent
    std::vector<uint8_t> num_successful_children;
  };



}
