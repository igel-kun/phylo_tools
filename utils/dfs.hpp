
#pragma once

#include "set_interface.hpp"
#include "concat_iter.hpp"
#include "trans_iter.hpp"

#include "traversal_traits.hpp"
#include "types.hpp"

namespace PT{

  template<class Roots_, mstd::TypeRune rune = mstd::TR_Strict> concept DFSRootStorageType =
    (mstd::VerifyableIter<Roots_, rune> or NodeContainerType<Roots_, rune> or mstd::is_same_v<Roots_, NodeDesc, rune>);

  // NOTE: for future reference: I tried using C++20 coroutines to simplify DFS iteration, but such coroutines are NOT copyable!
  //       Thus, any iterator based on coroutines could not be copied (only moved) which made it impossible to use range-based 'for' on them
  //       Therefore, I gave up coroutines for DFS iteration and, should anyone retry this, please be aware of the described pitfall!
  template<TraversalType tt, DFSRootStorageType Roots_, TraversalTraitsType Traits>
  struct DFSIterator: public Traits {
    using Roots = std::conditional_t<std::is_same_v<Roots_, NodeDesc>, NodeSingleton, Roots_>;
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
      DEBUG6(std::cout << "DFS (type "<< static_cast<int>(tt)<<"): placing ref to 'children' of "<<u<<" on child_history stack\n");
      assert(u != NoNode);
      auto u_children = get_next_items(u);
      //DEBUG6(std::cout << "DFS (type "<< static_cast<int>(tt)<<"): 'children' of "<<u<<": "<< u_children<<"\n");
      child_history.emplace_back(std::move(u_children));
      DEBUG6(std::cout << "DFS (type "<< static_cast<int>(tt)<<"): child-stack size now "<<child_history.size()<<"\n");

      // make sure we start with an unseen child
      if constexpr(track_nodes) {
        const size_t num_skipped = skip_seen_children();
        // ATTENTION: when track_nodes is active, we may skip all but one child of u; when in in-order mode, this is problematic
        //    because we would want to output u after its last child in this case...
        //    slightly abusing our data structures, we can note this down in the seen_set...
        if constexpr(tt & inorder)
          if(Traits::num_next_items(u) == num_skipped + 1) mark_seen(u);
      }

      // if a pre-order is requested, then u itself is emittable, so don't put more stuff on the stack; otherwise, keep diving to the first unseen child
      if constexpr (not (tt & preorder))
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
          if constexpr (tt & postorder) return; // in post-order, output the node instead of backtracking further
          // in in-order, if all but at most one child have been skipped, then output the node instead of backtrack
          if constexpr (tt & inorder) {
            const NodeDesc u = node_on_top();
            if(Traits::num_next_items(u) <= num_skipped + 1) return;
            if(is_seen(u)) return;
          }
          backtrack(); // if the children are spent then keep popping end-iterators unless postorder is requested
        } else if constexpr (!(tt & inorder)) dive(get_node(*(child_history.back())));
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
    DFSIterator() { DEBUG6(std::cout << "DFS: making new DFS end-iterator (type "<< static_cast<int>(tt) <<")\n"); }

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class RootInit, class... Args>
    DFSIterator(RootInit&& root_init, Args&&... args):
      Traits{std::forward<Args>(args)...},
      roots{std::forward<RootInit>(root_init)}
    {
      if(has_roots()) {
        DEBUG6(std::cout << "DFS: making new non-end DFS iterator (type "<< static_cast<int>(tt) <<") roots "<<mstd::IterFactory<Roots>{roots}<<", starting at "<<current_root()<<" (tracking? "<<track_nodes<<"), root is seen? "<<is_seen(current_root())<<"\n");
        if constexpr (track_nodes) {
          if(skip_seen_roots())
            dive(current_root());
        } else dive(current_root());
      } else { DEBUG6(std::cout << "DFS: making new DFS end-iterator (type "<< static_cast<int>(tt) <<")\n");}
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


  template<class Roots_, StrictPhylogenyType Net_> struct RootsOr_ {
    // if Roots is a const container, then we'll use an auto_iter into that container
    // if Roots is a const iterator, we'll just remove the const
    using RIter = std::remove_const_t<Roots_>;
    using RContainer = std::conditional_t<std::is_const_v<Roots_>,
          std::conditional_t<mstd::SingletonSetType<Roots_>, std::remove_const_t<Roots_>, mstd::auto_iter<Roots_, void>>, Roots_>;

    static constexpr bool roots_container = mstd::ContainerType<Roots_>;
    using type = std::conditional_t<roots_container, RContainer, RIter>;
  };
  // if Roots set is not given, it's the same as the network's root set;
  template<class Roots_, StrictPhylogenyType Net_> using RootsOr = mstd::FirstNonVoid<Roots_, typename Net_::RootContainer>;


  template<TraversalType tt,
           PhylogenyType Network_,
           class Roots_ = void,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
      requires (is_node_traversal(tt) and DFSRootStorageType<Roots_, mstd::TR_VoidOK>)
  struct DFSNodeIterator:
    public DFSIterator<tt, RootsOr<Roots_, Network_>, NodeTraversalTraits<tt,Network_, Forbidden_, SeenSet_>>
  {
    using Parent = DFSIterator<tt, RootsOr<Roots_, Network_>, NodeTraversalTraits<tt, Network_, Forbidden_, SeenSet_>>;
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

  template<TraversalType tt,
           PhylogenyType Network_,
           DFSRootStorageType<mstd::TR_VoidOK> Roots_ = void,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>,
           template<TraversalType, class, class, class> class Traits_ = EdgeTraversalTraits>
  struct DFSEdgeIterator:
    public DFSIterator<tt, RootsOr<Roots_, Network_>, Traits_<tt, Network_, Forbidden_, SeenSet_>>
  {
    using Parent = DFSIterator<tt, RootsOr<Roots_, Network_>, Traits_<tt, Network_, Forbidden_, SeenSet_>>;
    using Parent::node_on_top;
    using Parent::is_seen;
    using typename Parent::Network;
    using typename Parent::reference;
    using typename Parent::pointer;
    using Parent::child_history;
      
    static_assert(is_edge_traversal(tt) or is_all_edge_traversal(tt));
    static_assert(TraversalTraitsType<Traits_<tt, Network_, Forbidden_, SeenSet_>>);

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
      if constexpr (tt & preorder) 
        if(Parent::has_roots())
          Parent::advance();
    }
    template<class... Args>
    DFSEdgeIterator(const NodeDesc rt, Args&&... args):
      DFSEdgeIterator(mstd::singleton_set{rt}, std::forward<Args>(args)...) {}

    template<class... Args>
    DFSEdgeIterator(const Network_& N, Args&&... args):
      DFSEdgeIterator(N.roots(), std::forward<Args>(args)...) {}

    DFSEdgeIterator& operator++() { ++static_cast<Parent&>(*this); return *this; }
    DFSEdgeIterator& operator++(int) { static_cast<Parent&>(*this)++; return *this; }
  };

  template<TraversalType tt,
           PhylogenyType Network_,
           class Roots_ = void,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
             requires (is_all_edge_traversal(tt) and DFSRootStorageType<Roots_, mstd::TRVoidOK_>)
  using DFSAllEdgesIterator = DFSEdgeIterator<tt, Network_, RootsOr<Roots_, Network_>, Forbidden_, SeenSet_, AllEdgesTraits>;

  // NOTE: an all-edge-tail-postorder is just a node-postorder with an additional auto_iter<SuccContainer> for the current node
  template<PhylogenyType Network_,
           class Roots = void,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, postorder | all_edge_traversal>>
    requires DFSRootStorageType<Roots, mstd::TR_VoidOK>
  class DFSAllEdgesTailPOIterator: public DFSNodeIterator<postorder, Network_, Roots, Forbidden_, SeenSet_> {
    using Parent = DFSNodeIterator<postorder, Network_, Roots, Forbidden_, SeenSet_>;
    using Traits = AllEdgesTraits<postorder | all_edge_traversal, Network_, Forbidden_, SeenSet_>;
    using InternalIter = mstd::auto_iter<typename Network_::SuccContainer>;
    InternalIter current_children;

    void advance_dfs_nodes() {
      if(Parent::is_valid()) {
        while(1) {
          Parent::operator++();
          if(Parent::is_valid()) {
            auto& u_node = node_of<Network_>(Parent::operator*());
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


  template<TraversalType tt>
  struct _choose_iterator {
    template<StrictPhylogenyType Network_, class Roots, class Forbidden_, DFSSeenType SeenSet_>
    using type = DFSNodeIterator<tt, Network_, Roots, Forbidden_, SeenSet_>;
  };
  template<TraversalType tt> requires (is_edge_traversal(tt))
  struct _choose_iterator<tt> {
    template<StrictPhylogenyType Network_, class Roots, class Forbidden_, DFSSeenType SeenSet_>
    using type = DFSEdgeIterator<tt, Network_, Roots, Forbidden_, SeenSet_>;
  };
  template<TraversalType tt> requires (is_all_edge_traversal(tt))
  struct _choose_iterator<tt> {
    template<StrictPhylogenyType Network_, class Roots, class Forbidden_, DFSSeenType SeenSet_>
    using type = DFSAllEdgesIterator<tt, Network_, Roots, Forbidden_, SeenSet_>;
  };
  template<>
  struct _choose_iterator<all_edge_tail_postorder> {
    template<StrictPhylogenyType Network_, class Roots, class Forbidden_, DFSSeenType SeenSet_>
    using type = DFSAllEdgesTailPOIterator<Network_, Roots, Forbidden_, SeenSet_>;
  };
  template<TraversalType tt, StrictPhylogenyType Network_, class Roots, class Forbidden_, DFSSeenType SeenSet_>
  using choose_iterator = typename _choose_iterator<tt>::template type<Network_, Roots, Forbidden_, SeenSet_>;


  // this guy is our factory; begin()/end() can be called on it
  //NOTE: Traversal has its own SeenSet_ so that multiple calls to begin() can be given the same set of forbidden nodes
  //      Thus, you can either call begin() - which uses the Traversal's SeenSet_, or you call begin(bla, ...) to construct a new SeenSet_ from bla, ...
  //NOTE: per default, the Traversal's SeenSet is used direcly by the DFS iterator, so don't call begin() twice with a non-empty SeenSet!
  //      if you want to reuse the same SeenSet for multiple DFS' you have the following options:
  //      (a) reset the SeenSet between calls to begin()
  //      (b) call begin(bla, ...) instead of begin()
#warning "TODO: check if this can be an auto_iter"
  template<TraversalType tt,
           PhylogenyType Network,
           NodeOrIterableType Roots_ = typename Network::RootContainer,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network, tt>>
  struct Traversal:
    public DFSSupportSets<Forbidden_, SeenSet_>,
    public mstd::iterator_traits<choose_iterator<tt, Network, Roots_,
      typename DFSSupportSets<Forbidden_, SeenSet_>::ForbiddenRef,
      typename DFSSupportSets<Forbidden_, SeenSet_>::SeenSetRef>>
  {
    using Helper = DFSSupportSets<Forbidden_, SeenSet_>;
    using Roots = std::conditional_t<std::is_same_v<Roots_, NodeDesc>, NodeSingleton, Roots_>;
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


    using Iter = choose_iterator<tt, Network, Roots, ForbiddenRef, SeenSetRef>;
    using OwningIter = choose_iterator<tt, Network, Roots, Forbidden_, SeenSet_>;
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
  template<TraversalType tt,
           PhylogenyType Network,
           class Forbidden,
           DFSSeenType SeenSet>
  struct DFSSupportSets<tt, Network, Forbidden, SeenSet>:
    public ProtoDFSSupportSets<Forbidden, SeenSet>
  {
    using Helper = ProtoDFSSupportSets<Forbidden, SeenSet>;
    using typename Helper::Forbidden;
    using typename Helper::SeenSetRef;
    using typename Helper::ForbiddenRef;

    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using Iter = choose_iterator<tt, Network, ForbiddenRef, SeenSetRef>;
    using OwningIter = choose_iterator<tt, Network, Forbidden, SeenSet>;

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
      if constexpr (!is_node_traversal(tt))
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




  template<TraversalType tt,
           PhylogenyType Network,
           class Roots_,
           class Forbidden = void,
           DFSSeenType SeenSet = DefaultSeenSet<Network, tt>>
    requires (not std::is_reference_v<SeenSet>)
  struct Traversal:
    public DFSSupportSets<tt, Network, Roots_, Forbidden, SeenSet>,
    public mstd::iterator_traits<typename DFSSupportSets<tt, Network, Roots_, Forbidden, SeenSet>::Iter>
  {
    using Roots = Roots_;
    using Parent = DFSSupportSets<tt, Network, Roots, Forbidden, SeenSet>;
    using Traits = mstd::iterator_traits<typename Parent::Iter>;
    using iterator = typename Parent::Iter;
    using const_iterator = iterator;
    using Parent::Parent;

    static constexpr bool track_nodes = iterator::track_nodes;
  };
*/

  // these convenience declarations allow saying EdgeTraversal<postorder, ...> while, normally, "postorder" means node-postorder
  template<TraversalType tt,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           class Forbidden = void,
           DFSSeenType SeenSet = DefaultSeenSet<Network, tt>>
  using NodeTraversal = Traversal<tt, Network, RootsOr<Roots, Network>, Forbidden, SeenSet>;
  template<TraversalType tt,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           class Forbidden = void,
           DFSSeenType SeenSet = DefaultSeenSet<Network, tt>>
  using EdgeTraversal = Traversal<TraversalType(tt | edge_traversal), Network, RootsOr<Roots, Network>, Forbidden, SeenSet>;
  template<TraversalType tt,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           class Forbidden = void,
           DFSSeenType SeenSet = Network::DefaultSeen>
  using AllEdgesTraversal = Traversal<TraversalType(tt | all_edge_traversal), Network, RootsOr<Roots, Network>, Forbidden, SeenSet>;

}// namespace


// to use C++20 ranges with our traversals, we'll need to tell the range library that they are borrowed ranges
template<PT::TraversalType tt, PT::PhylogenyType Network, class Roots, class Forbidden, PT::DFSSeenType SeenSet>
constexpr bool std::ranges::enable_borrowed_range<PT::Traversal<tt, Network, Roots, Forbidden, SeenSet>> = true;


