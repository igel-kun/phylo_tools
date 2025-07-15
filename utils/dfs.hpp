
#pragma once

#include "set_interface.hpp"
#include "concat_iter.hpp"
#include "trans_iter.hpp"
#include "traversal_traits.hpp"
#include "types.hpp"

namespace PT{

  template<class _Roots, mstd::TypeRune rune = mstd::TR_Strict> concept DFSRootStorageType =
    (mstd::VerifyableIter<_Roots, rune> or NodeContainerType<_Roots, rune> or mstd::is_same_v<_Roots, NodeDesc, rune>);

  // NOTE: for future reference: I tried using C++20 coroutines to simplify DFS iteration, but such coroutines are NOT copyable!
  //       Thus, any iterator based on coroutines could not be copied (only moved) which made it impossible to use range-based 'for' on them
  //       Therefore, I gave up coroutines for DFS iteration and, should anyone retry this, please be aware of the described pitfall!
  template<TraversalType o, DFSRootStorageType _Roots, TraversalTraitsType Traits>
  struct DFSIterator: public Traits {
    using Roots = std::conditional_t<std::is_same_v<_Roots, NodeDesc>, NodeSingleton, _Roots>;
    using typename Traits::Network;
    using Traits::track_nodes;
    using Traits::is_seen;
    using Traits::get_next_items;
    using Traits::get_node;
    using Traits::min_stacksize;
    using Traits::mark_seen;
  
  protected:
    using ChildIter = typename Traits::child_iterator;
    using Stack = std::vector<ChildIter>;

    Roots roots;

    // the history of descents in the network, for each descent, save the iterator of the current child and the end()-iterator
    Stack child_history;

    NodeDesc current_root() const {
      if constexpr (NodeContainerType<Roots>) {
        return roots.back();
      } else return *roots;
    }
    bool has_roots() const {
      if constexpr (NodeContainerType<Roots>) {
        return not roots.empty();
      } else return roots.is_valid();
    }
    // skip seen roots and return if an unseen root remained
    bool skip_seen_roots() {
      while(is_seen(current_root()))
        if(not next_root()) return false;
      return true;
    }

    // move to the next root, return true if there was a new root to move to, return false if there are no more roots
    bool next_root() {
      if(has_roots()) {
        if constexpr (NodeContainerType<Roots>) {
          roots.pop_back();
        } else ++roots;
        return has_roots();
      } else return false;
    }

    // dive deeper into the network, up to the next emittable node x, putting ranges on the stack (including that of x); return the current node
    // if we hit a node that is already seen, return that node
    void dive(const NodeDesc u) {
      DEBUG6(std::cout << "DFS (type "<< static_cast<int>(o)<<"): placing ref to 'children' of "<<u<<" on child_history stack\n");
      assert(u != NoNode);
      auto u_children = get_next_items(u);
      //DEBUG6(std::cout << "DFS (type "<< static_cast<int>(o)<<"): 'children' of "<<u<<": "<< u_children<<"\n");
      child_history.emplace_back(std::move(u_children));
      DEBUG6(std::cout << "DFS (type "<< static_cast<int>(o)<<"): child-stack size now "<<child_history.size()<<"\n");

      // make sure we start with an unseen child
      if constexpr(track_nodes) {
        const size_t num_skipped = skip_seen_children();
        // ATTENTION: when track_nodes is active, we may skip all but one child of u; when in in-order mode, this is problematic
        //    because we would want to output u after its last child in this case...
        //    slightly abusing our data structures, we can note this down in the seen_set...
        if constexpr(o & inorder)
          if(Traits::num_next_items(u) == num_skipped + 1) mark_seen(u);
      }

      // if a pre-order is requested, then u itself is emittable, so don't put more stuff on the stack; otherwise, keep diving to the first unseen child
      if constexpr (not (o & preorder))
        if(not current_node_finished())
          dive(get_node(*(child_history.back())));
    }

    // return whether all children of the current node have been treated
    bool current_node_finished() const {
      assert(!child_history.empty());
      return child_history.back().is_invalid();
    }

    // get the node whose child-iterators are on top of the stack
    NodeDesc node_on_top() const {
      // if there are at least 2 ranges on the stack, then dereference the second to last to get the current node, otherwise, the current node is the root
      return (child_history.size() > 1) ? get_node(*(child_history[child_history.size() - 2])) : current_root();
    }

    // when we're done treating all children, go back and continue with the parent
    void backtrack() {
      assert(current_node_finished());
      child_history.pop_back();
      // if we're not at the end, increment the parent iterator and proceed
      if(not child_history.empty()) {
        assert(!current_node_finished());

        ChildIter& current_child = child_history.back();
        // mark the node that we just finished treating as "seen"
        if constexpr(track_nodes) mark_seen(*current_child);
        ++current_child;

        // if now the child at current_iter has been seen, skip it (and all following)
        size_t num_skipped = 0;
        if constexpr(track_nodes) num_skipped = skip_seen_children();

        // if there are still unseen children then dive() into the next subtree unless inorder is requested
        if(!current_child.is_valid()){
          if constexpr (o & postorder) return; // in post-order, output the node instead of backtracking further
          // in in-order, if all but at most one child have been skipped, then output the node instead of backtrack
          if constexpr (o & inorder) {
            const NodeDesc u = node_on_top();
            if(Traits::num_next_items(u) <= num_skipped + 1) return;
            if(is_seen(u)) return;
          }
          backtrack(); // if the children are spent then keep popping end-iterators unless postorder is requested
        } else if constexpr (!(o & inorder)) dive(get_node(*(child_history.back())));
      } else {
        if constexpr (track_nodes) {
          mark_seen(current_root());
          if(skip_seen_roots())
            dive(current_root());
        } else if(next_root()) dive(current_root());
      }
    }

    // skip over all seen children of the current node
    size_t skip_seen_children() {
      size_t result = 0;
      ChildIter& current = child_history.back();
      // skip over all seen children
      DEBUG6(std::cout << "skipping seen...\n");
      while(current.is_valid() && is_seen(*current)) { ++current; ++result;}
      return result;
    }

    void advance() {
      DEBUG6(std::cout << "DFS: operator++\n");
      if(not current_node_finished()) {
        // the current node is not finished, so
        //  (b) either we're doing pre-order and we're currently diving
        //  (a) or we're doing inorder and we just returned from a subtree with backtrack()
        // in both cases, we continue with a dive()
        const auto& iter = child_history.back();
        const auto& last_child = *iter;
        dive(get_node(last_child));
      } else backtrack(); // since we're done with the node_on_top now, go backtrack()
    }

  public:
    bool is_valid() const { return child_history.size() >= min_stacksize; }

    // NOTE: use this to construct an end-iterator
    DFSIterator() { DEBUG6(std::cout << "DFS: making new DFS end-iterator (type "<< static_cast<int>(o) <<")\n"); }

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class RootInit, class... Args>
    DFSIterator(RootInit&& root_init, Args&&... args):
      Traits{std::forward<Args>(args)...},
      roots{std::forward<RootInit>(root_init)}
    {
      if(has_roots()) {
        DEBUG6(std::cout << "DFS: making new non-end DFS iterator (type "<< static_cast<int>(o) <<") roots "<<mstd::IterFactory<Roots>{roots}<<", starting at "<<current_root()<<" (tracking? "<<track_nodes<<"), root is seen? "<<is_seen(current_root())<<"\n");
        if constexpr (track_nodes) {
          if(skip_seen_roots())
            dive(current_root());
        } else dive(current_root());
      } else { DEBUG6(std::cout << "DFS: making new DFS end-iterator (type "<< static_cast<int>(o) <<")\n");}
    }

    template<class... Args>
    DFSIterator(const NodeDesc rt, Args&&... args):
      DFSIterator(mstd::singleton_set{rt}, std::forward<Args>(args)...)
    {}

    template<StrictPhylogenyType Phylo, class... Args>
    DFSIterator(const Phylo& N, Args&&... args):
      DFSIterator(N.roots(), std::forward<Args>(args)...)
    {}
    
    DFSIterator(const DFSIterator& other) = default;
    DFSIterator(DFSIterator&& other) = default; //: Traits{static_cast<Traits&&>(other)}, root{other.root}, child_history{std::move(other.child_history)} {}
    DFSIterator& operator=(const DFSIterator& other) = default;
    DFSIterator& operator=(DFSIterator&& other) = default;
    
    DFSIterator& operator++() {
      if(is_valid()) advance();
      return *this;
    }
    DFSIterator operator++(int) { DFSIterator tmp(*this); ++(*this); return tmp; }

    // NOTE: we roughly estimate that two DFSIterators are equal if they are both the end iterator or they have the same root, stack size and top node
    template<TraversalType OtherO, class OtherRoots, TraversalTraitsType OtherTraits>
    bool operator==(const DFSIterator<OtherO, OtherRoots, OtherTraits>& other) const {
      if(other.is_valid()){
        return is_valid() && (current_root() == other.current_root()) && (child_history.size() == other.child_history.size()) && (node_on_top() == other.node_on_top());
      } else return not is_valid();
    }

    static constexpr auto get_end() { return mstd::GenericEndIterator(); }

    // DFSIterators for other traversal types are our friends!
    template<TraversalType, DFSRootStorageType, TraversalTraitsType>
    friend class DFSIterator;
  };


  template<class _Roots, StrictPhylogenyType _Net> struct _RootsOr {
    // if Roots is a const container, then we'll use an auto_iter into that container
    // if Roots is a const iterator, we'll just remove the const
    using RIter = std::remove_const_t<_Roots>;
    using RContainer = std::conditional_t<std::is_const_v<_Roots>,
          std::conditional_t<mstd::SingletonSetType<_Roots>, std::remove_const_t<_Roots>, mstd::auto_iter<_Roots, void>>, _Roots>;

    static constexpr bool roots_container = mstd::ContainerType<_Roots>;
    using type = std::conditional_t<roots_container, RContainer, RIter>;
  };
  // if Roots set is not given, it's the same as the network's root set;
  template<class _Roots, StrictPhylogenyType _Net> using RootsOr = mstd::FirstNonVoid<_Roots, typename _Net::RootContainer>;


  template<TraversalType o,
           PhylogenyType _Network,
           class _Roots = void,
           class _Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> _SeenSet = typename _Network::DefaultSeen>
      requires (is_node_traversal(o) and DFSRootStorageType<_Roots, mstd::TR_VoidOK>)
  struct DFSNodeIterator:
    public DFSIterator<o, RootsOr<_Roots, _Network>, NodeTraversalTraits<_Network, _Forbidden, _SeenSet, is_reverse_traversal(o)>>
  {
    using Parent = DFSIterator<o, RootsOr<_Roots, _Network>, NodeTraversalTraits<_Network, _Forbidden, _SeenSet, is_reverse_traversal(o)>>;
    using Parent::node_on_top;
    using typename Parent::reference;
    using typename Parent::pointer;

    DFSNodeIterator() = default;
    INHERIT_ALL_CONSTRUCTORS(DFSNodeIterator, Parent)

    DFSNodeIterator& operator++() { ++static_cast<Parent&>(*this); return *this; }
    DFSNodeIterator& operator++(int) { static_cast<Parent&>(*this)++; return *this; }

    pointer operator->() const = delete;
/*    {
      const auto& result = node_on_top();
      DEBUG6(std::cout << "DFS: emitting ptr to node " << result << "\n");
      return &result;
    }
*/
    NodeDesc operator*() const {
      const NodeDesc result = node_on_top();
      DEBUG6(std::cout << "DFS: emitting node "<< result<<"\n");
      return result;
    }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           class _Roots = void,
           class _Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> _SeenSet = typename _Network::DefaultSeen,
           template<class, class, class, bool> class _Traits = EdgeTraversalTraits>
             requires ((is_edge_traversal(o) || is_all_edge_traversal(o)) and
                       DFSRootStorageType<_Roots, mstd::TR_VoidOK> and
                       TraversalTraitsType<_Traits<_Network, _Forbidden, _SeenSet, is_reverse_traversal(o)>>)
  struct DFSEdgeIterator: public DFSIterator<o, RootsOr<_Roots, _Network>, _Traits<_Network, _Forbidden, _SeenSet, is_reverse_traversal(o)>>
  {
    using Parent = DFSIterator<o, RootsOr<_Roots, _Network>, _Traits<_Network, _Forbidden, _SeenSet, is_reverse_traversal(o)>>;
    using Parent::node_on_top;
    using Parent::is_seen;
    using typename Parent::Network;
    using typename Parent::reference;
    using typename Parent::pointer;
    using Parent::child_history;

    pointer operator->() { return operator*(); }
    pointer operator->() const { return operator*(); }

    reference operator*() const {
      assert(child_history.size() > 1);
      DEBUG6(std::cout << "DFS: emitting edge "<<*(child_history[child_history.size() - 2])<<"\n");
      return *(child_history[child_history.size() - 2]);
    }

    // construct default end-iterator
    DFSEdgeIterator() = default;

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class RootInit, class... Args>
      requires (not mstd::is_same_v<RootInit, DFSEdgeIterator>)
    DFSEdgeIterator(RootInit&& root_init, Args&&... args):
      Parent(std::forward<RootInit>(root_init), std::forward<Args>(args)...)
    {
      if constexpr (o & preorder) 
        if(Parent::has_roots())
          Parent::advance();
    }
    template<class... Args>
    DFSEdgeIterator(const NodeDesc rt, Args&&... args):
      DFSEdgeIterator(mstd::singleton_set{rt}, std::forward<Args>(args)...) {}

    template<class... Args>
    DFSEdgeIterator(const _Network& N, Args&&... args):
      DFSEdgeIterator(N.roots(), std::forward<Args>(args)...) {}

    DFSEdgeIterator& operator++() { ++static_cast<Parent&>(*this); return *this; }
    DFSEdgeIterator& operator++(int) { static_cast<Parent&>(*this)++; return *this; }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           class _Roots = void,
           class _Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> _SeenSet = typename _Network::DefaultSeen>
             requires (is_all_edge_traversal(o) and DFSRootStorageType<_Roots, mstd::TR_VoidOK>)
  using DFSAllEdgesIterator = DFSEdgeIterator<o, _Network, RootsOr<_Roots, _Network>, _Forbidden, _SeenSet, AllEdgesTraits>;

  // NOTE: an all-edge-tail-postorder is just a node-postorder with an additional auto_iter<SuccContainer> for the current node
  template<PhylogenyType _Network,
           class Roots = void,
           class _Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> _SeenSet = typename _Network::DefaultSeen>
    requires DFSRootStorageType<Roots, mstd::TR_VoidOK>
  class DFSAllEdgesTailPOIterator: public DFSNodeIterator<postorder, _Network, Roots, _Forbidden, _SeenSet> {
    using Parent = DFSNodeIterator<postorder, _Network, Roots, _Forbidden, _SeenSet>;
    using Traits = AllEdgesTraits<_Network, _Forbidden, _SeenSet, false>;
    using InternalIter = mstd::auto_iter<typename _Network::SuccContainer>;
    InternalIter current_children;

    void advance_dfs_nodes() {
      if(Parent::is_valid()) {
        while(1) {
          Parent::operator++();
          if(Parent::is_valid()) {
            auto& u_node = node_of<_Network>(Parent::operator*());
            if(not u_node.is_leaf()) {
              current_children = InternalIter(u_node.children());
              return;
            }
          } else break;
        }
      }
    }

  public:
    using value_type = typename Traits::value_type;
    using reference = typename Traits::reference;
    using const_reference = reference;
    using difference_type = typename Traits::difference_type;
    using iterator_category = typename Traits::iterator_category;
    using pointer = typename Traits::pointer;
    using const_pointer = pointer;

    DFSAllEdgesTailPOIterator(): Parent() {}

    template<class First, class... Args> requires (!std::is_same_v<std::remove_cvref_t<First>, DFSAllEdgesTailPOIterator>)
    DFSAllEdgesTailPOIterator(First&& first, Args&&... args): Parent(std::forward<First>(first), std::forward<Args>(args)...)
    {
      advance_dfs_nodes();
    }

    //DFSAllEdgesTailPOIterator(const DFSAllEdgesTailPOIterator&) = default;
    //DFSAllEdgesTailPOIterator(DFSAllEdgesTailPOIterator&&) = default;
    //DFSAllEdgesTailPOIterator& operator=(const DFSAllEdgesTailPOIterator&) = default;
    //DFSAllEdgesTailPOIterator& operator=(DFSAllEdgesTailPOIterator&&) = default;
   
    bool is_valid() const { return current_children.is_valid(); }

    auto& operator++() {
      if(is_valid()) {
        ++current_children;
        if(not current_children.is_valid()) advance_dfs_nodes();
      }
      return *this;
    }

    auto& operator++(int) { DFSAllEdgesTailPOIterator result(*this); ++(*this); return result; }

    reference operator*() const { return {Parent::operator*(), *current_children}; }
    pointer operator->() = delete;
  };


  template<TraversalType o>
  struct _choose_iterator {
    template<StrictPhylogenyType _Network, class Roots, class _Forbidden, NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
    using type = DFSNodeIterator<o, _Network, Roots, _Forbidden, _SeenSet>;
  };
  template<TraversalType o> requires (is_edge_traversal(o))
  struct _choose_iterator<o> {
    template<StrictPhylogenyType _Network, class Roots, class _Forbidden, NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
    using type = DFSEdgeIterator<o, _Network, Roots, _Forbidden, _SeenSet>;
  };
  template<TraversalType o> requires (is_all_edge_traversal(o))
  struct _choose_iterator<o> {
    template<StrictPhylogenyType _Network, class Roots, class _Forbidden, NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
    using type = DFSAllEdgesIterator<o, _Network, Roots, _Forbidden, _SeenSet>;
  };
  template<>
  struct _choose_iterator<all_edge_tail_postorder> {
    template<StrictPhylogenyType _Network, class Roots, class _Forbidden, NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
    using type = DFSAllEdgesTailPOIterator<_Network, Roots, _Forbidden, _SeenSet>;
  };
  template<TraversalType o, StrictPhylogenyType _Network, class Roots, class _Forbidden, NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
  using choose_iterator = typename _choose_iterator<o>::template type<_Network, Roots, _Forbidden, _SeenSet>;


  // this guy is our factory; begin()/end() can be called on it
  //NOTE: Traversal has its own _SeenSet so that multiple calls to begin() can be given the same set of forbidden nodes
  //      Thus, you can either call begin() - which uses the Traversal's _SeenSet, or you call begin(bla, ...) to construct a new _SeenSet from bla, ...
  //NOTE: per default, the Traversal's SeenSet is used direcly by the DFS iterator, so don't call begin() twice with a non-empty SeenSet!
  //      if you want to reuse the same SeenSet for multiple DFS' you have the following options:
  //      (a) reset the SeenSet between calls to begin()
  //      (b) call begin(bla, ...) instead of begin()
#warning "TODO: check if this can be an auto_iter"
  template<TraversalType o,
           PhylogenyType Network,
           DFSRootStorageType _Roots,
           class _Forbidden,
           NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
  struct Traversal:
    public DFSSupportSets<_Forbidden, _SeenSet>,
    public mstd::iterator_traits<choose_iterator<o, Network, _Roots,
      typename DFSSupportSets<_Forbidden, _SeenSet>::ForbiddenRef,
      typename DFSSupportSets<_Forbidden, _SeenSet>::SeenSetRef>>
  {
    using Helper = DFSSupportSets<_Forbidden, _SeenSet>;
    using Roots = std::conditional_t<std::is_same_v<_Roots, NodeDesc>, NodeSingleton, _Roots>;
    using typename Helper::Forbidden;
    using typename Helper::SeenSet;
    using typename Helper::SeenSetRef;
    using typename Helper::ForbiddenRef;

    Roots roots;

    template<class T>
    void set_roots(T&& x) { roots.clear(); append(roots, std::forward<T>(x)); }

    template<class RootInit, class... Args>
    Traversal(RootInit&& root_init, Args&&... args):
      Helper(std::forward<Args>(args)...),
      roots(std::forward<RootInit>(root_init))
    {}
    template<PhylogenyType Phylo>
    Traversal(Phylo&& N):
      Traversal(std::forward<Phylo>(N).roots())
    {}


    using Iter = choose_iterator<o, Network, Roots, ForbiddenRef, SeenSetRef>;
    using OwningIter = choose_iterator<o, Network, Roots, _Forbidden, _SeenSet>;
    using iterator = Iter;
    using const_iterator = iterator;
   
    static constexpr bool track_nodes = iterator::track_nodes;


    // if we are traversing nodes, then empty() means that roots is empty
    // if we are traversing edges, then empty() means that there are no edges; to determine this, we have to check if all roots are leaves
    bool empty() const { return begin() == end(); }

    auto begin() & { return Iter(roots, static_cast<Helper&>(*this)); }
    // if we have a seenset, we cannot expect the traversal to keep it constant...
    auto begin() const & requires (std::is_void_v<SeenSet> or std::is_pointer_v<SeenSet>)
    {
      return Iter(roots, static_cast<const Helper&>(*this));
    }
    // if we are going out of scope (which will be most of the cases), then move our seen-set into the constructed iterator
    auto begin() && { return OwningIter(std::move(roots), static_cast<Helper&&>(*this)); }

    // if called with one or more arguments, begin() constructs a new iterator using these arguments
//    template<class... Args> requires (sizeof...(Args) != 0)
//    auto begin(Args&&... args) { return Iter(std::piecewise_construct, std::forward_as_tuple(roots), std::forward_as_tuple(std::forward<Args>(args)...)); }

    static constexpr auto end() { return mstd::GenericEndIterator(); }

    // allow the user to play with the SeenSet and Forbiddeniacte at all times
    //NOTE: this gives you the power to change the SeenSet while the DFS is running, and with great power comes great responsibility ;] so be careful!
    auto& seen_nodes() { return mstd::default_deref{}(Helper::template get<1>()); }
    const auto& seen_nodes() const { return mstd::default_deref{}(Helper::template get<1>()); }
    auto& get_forbidden() { return Helper::template get<0>(); }
    const auto& get_forbidden() const { return Helper::template get<0>(); }

    template<mstd::ContainerType Container>
    Container& append_to(Container& c) { append(c, *this); return c; }
    template<mstd::ContainerType Container = std::vector<typename Iter::value_type>>
    Container to_container() { Container c; append(c, *this); return c; }
  };

/*
  template<TraversalType o,
           PhylogenyType Network,
           class Forbidden,
           NodeSetType<mstd::TR_PtrVoidOK> SeenSet>
  struct DFSSupportSets<o, Network, Forbidden, SeenSet>:
    public ProtoDFSSupportSets<Forbidden, SeenSet>
  {
    using Helper = ProtoDFSSupportSets<Forbidden, SeenSet>;
    using typename Helper::Forbidden;
    using typename Helper::SeenSetRef;
    using typename Helper::ForbiddenRef;

    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using Iter = choose_iterator<o, Network, ForbiddenRef, SeenSetRef>;
    using OwningIter = choose_iterator<o, Network, Forbidden, SeenSet>;

    NodeDesc root;

    template<class... Args>
    DFSSupportSets(const NodeDesc _root, Args&&... args):
      Helper(std::forward<Args>(args)...),
      root(_root)
    {}
    template<class... Args>
    DFSSupportSets(const NodeSingleton& _root, Args&&... args):
      Helper(std::forward<Args>(args)...),
      root(_root.empty() ? NoNode : front(_root))
    {}
    template<class... Args>
    DFSSupportSets(const Network& N, Args&&... args):
      Helper(std::forward<Args>(args)...),
      root(N.root())
    {}

    bool empty() const {
      if constexpr (!is_node_traversal(o))
        return (root == NoNode) || (Network::is_leaf(root));
      else return root == NoNode;
    }

    auto begin() & { return Iter(root, static_cast<Helper&>(*this)); }
    // if we are const, then our seen-set is const and so, the iterator we create cannot reference our seen-set; thus, we'll have to copy it :/
    auto begin() const & { return OwningIter(root, static_cast<const Parent&>(*this)); }
    // if we are going out of scope (which will be most of the cases), then move our seen-set into the constructed iterator
    auto begin() && { return OwningIter(root, static_cast<Parent&&>(*this)); }
    // if called with one or more arguments, begin() constructs a new iterator using these arguments
    template<class... Args>
    auto begin(const owning_tag, Args&&... args) { return OwningIter(root, std::forward<Args>(args)...); }
    template<class... Args>
    auto begin(const non_owning_tag, Args&&... args) { return Iter(root, std::forward<Args>(args)...); }

    static constexpr auto end() { return mstd::GenericEndIterator(); }
  };




  template<TraversalType o,
           PhylogenyType Network,
           class _Roots,
           class Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> SeenSet = typename Network::DefaultSeen>
    requires (not std::is_reference_v<SeenSet>)
  struct Traversal:
    public DFSSupportSets<o, Network, _Roots, Forbidden, SeenSet>,
    public mstd::iterator_traits<typename DFSSupportSets<o, Network, _Roots, Forbidden, SeenSet>::Iter>
  {
    using Roots = _Roots;
    using Parent = DFSSupportSets<o, Network, Roots, Forbidden, SeenSet>;
    using Traits = mstd::iterator_traits<typename Parent::Iter>;
    using iterator = typename Parent::Iter;
    using const_iterator = iterator;
    using Parent::Parent;

    static constexpr bool track_nodes = iterator::track_nodes;
  };
*/

  // these convenience declarations allow saying EdgeTraversal<postorder, ...> while, normally, "postorder" means node-postorder
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           class Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> SeenSet = typename Network::DefaultSeen>
  using NodeTraversal = Traversal<o, Network, RootsOr<Roots, Network>, Forbidden, SeenSet>;
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           class Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> SeenSet = typename Network::DefaultSeen>
  using EdgeTraversal = Traversal<TraversalType(o | edge_traversal), Network, RootsOr<Roots, Network>, Forbidden, SeenSet>;
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           class Forbidden = void,
           NodeSetType<mstd::TR_PtrVoidOK> SeenSet = Network::DefaultSeen>
  using AllEdgesTraversal = Traversal<TraversalType(o | all_edge_traversal), Network, RootsOr<Roots, Network>, Forbidden, SeenSet>;

}// namespace


// to use C++20 ranges with our traversals, we'll need to tell the range library that they are borrowed ranges
template<PT::TraversalType o, PT::PhylogenyType Network, class Roots, class Forbidden, PT::NodeSetType<mstd::TR_PtrVoidOK> SeenSet>
constexpr bool std::ranges::enable_borrowed_range<PT::Traversal<o, Network, Roots, Forbidden, SeenSet>> = true;


