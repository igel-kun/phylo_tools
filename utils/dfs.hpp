
#pragma once

#include "set_interface.hpp"
#include "auto_iter.hpp"
#include "concat_iter.hpp"
#include "trans_iter.hpp"
#include "traversal_traits.hpp"
#include "types.hpp"

namespace PT{

  // NOTE: for future reference: I tried using C++20 coroutines to simplify DFS iteration, but such coroutines are NOT copyable!
  //       Thus, any iterator based on coroutines could not be copied (only moved) which made it impossible to use range-based 'for' on them
  //       Therefore, I gave up coroutines for DFS iteration and, should anyone retry this, please be aware of the described pitfall!

  template<TraversalType o, TraversalTraitsType Traits>
  class DFSIterator: public Traits {
  public:
    using typename Traits::Network;
    using Traits::track_nodes;
   
  protected:
    using ChildIter = typename Traits::child_iterator;
    using Stack = std::vector<ChildIter>;
    using Traits::get_next_items;
    using Traits::get_node;
    using Traits::min_stacksize;
    using Traits::mark_seen;
    using Traits::is_seen;
    using ChildContainerRef = typename Traits::ItemContainerRef;
  
    NodeDesc root;
    
    // the history of descents in the network, for each descent, save the iterator of the current child and the end()-iterator
    Stack child_history;

    // dive deeper into the network, up to the next emittable node x, putting ranges on the stack (including that of x); return the current node
    // if we hit a node that is already seen, return that node
    void dive(const NodeDesc u) {
      DEBUG6(std::cout << "DFS (type "<< static_cast<int>(o)<<"): placing ref to 'children' of "<<u<<" on child_history stack\n");
      assert(u != NoNode);
      ChildContainerRef u_children = get_next_items(u);
      DEBUG6(std::cout << "DFS (type "<< static_cast<int>(o)<<"): 'children' of "<<u<<": "<< u_children<<"\n");
      child_history.emplace_back(u_children);
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
      if constexpr (!(o & preorder)){
        if(!current_node_finished())
          dive(get_node(*(child_history.back())));
      }
    }

    // return whether all children of the current node have been treated
    bool current_node_finished() const {
      assert(!child_history.empty());
      return child_history.back().is_invalid();
    }

    // get the node whose child-iterators are on top of the stack
    NodeDesc node_on_top() const {
      // if there are at least 2 ranges on the stack, dereference the second to last to get the current node, otherwise, it's root
      //std::cout << "node on top is " << result << "\n";
      return (child_history.size() > 1) ? get_node(*(child_history[child_history.size() - 2])) : root;
    }

    // when we're done treating all children, go back and continue with the parent
    void backtrack() {
      assert(current_node_finished());
      child_history.pop_back();
      // if we're not at the end, increment the parent iterator and proceed
      if(!child_history.empty()){
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
          const NodeDesc u = node_on_top();
          if((o & inorder) && (Traits::num_next_items(u) <= num_skipped + 1)) return;
          if((o & inorder) && is_seen(u)) return;
          backtrack(); // if the children are spent then keep popping end-iterators unless postorder is requested
        } else if constexpr (!(o & inorder)) dive(get_node(*(child_history.back())));
      } else if constexpr(track_nodes) mark_seen(root);
    }

    // skip over all seen children of the current node
    size_t skip_seen_children() {
      size_t result = 0;
      ChildIter& current = child_history.back();
      // skip over all seen children
      while(current.is_valid() && is_seen(*current)) { ++current; ++result;}
      return result;
    }

    void advance() {
      DEBUG6(std::cout << "DFS: operator++\n");
      if(current_node_finished()){
        // since we're done with the node_on_top now, go backtrack()
        backtrack();
      } else {
        // the current node is not finished, so
        //  (b) either we're doing pre-order and we're currently diving
        //  (a) or we're doing inorder and we just returned from a subtree with backtrack()
        // in both cases, we continue with a dive()
        const auto& iter = child_history.back();
        const auto& last_child = *iter;
        dive(get_node(last_child));
      }
    }

  public:
    // NOTE: use this to construct an end-iterator
    DFSIterator(): Traits(), root(NoNode) {
      DEBUG6(std::cout << "DFS: making new DFS end-iterator (type "<< static_cast<int>(o) <<")\n");
    }

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSIterator(const NodeDesc _root, Args&&... args):
      Traits(std::forward<Args>(args)...), root(_root)
    {
      DEBUG6(std::cout << "DFS: making new non-end DFS iterator (type "<< static_cast<int>(o) <<") starting at "<<_root<<" (tracking? "<<track_nodes<<"), root is seen? "<<is_seen(root)<<"\n");
      if(root != NoNode) {
        if constexpr (track_nodes) {
          if(!is_seen(root)) dive(root);
        } else dive(root);
      }
    }
    template<StrictPhylogenyType Phylo, class... Args>
    DFSIterator(const Phylo& N, Args&&... args):
      DFSIterator(N.root(), std::forward<Args>(args)...)
    {}
    //DFSIterator(const DFSIterator& other) = default;
    //DFSIterator(DFSIterator&& other) = default;
    //DFSIterator& operator=(const DFSIterator& other) = default;
    //DFSIterator& operator=(DFSIterator&& other) = default;


    DFSIterator& operator++() {
      if(is_valid()) advance();
      return *this;
    }
    DFSIterator operator++(int) { DFSIterator tmp(*this); ++(*this); return tmp; }


    // NOTE: we roughly estimate that two DFSIterators are equal if they are both the end iterator or they have the same root, stack size and top node
    template<TraversalType OtherO, TraversalTraitsType OtherTraits>
    bool operator==(const DFSIterator<OtherO, OtherTraits>& other) const {
      if(other.is_valid()){
        return is_valid() && (root == other.root) && (child_history.size() == other.child_history.size()) && (node_on_top() == other.node_on_top());
      } else return !is_valid();
    }

    bool is_valid() const { return child_history.size() >= min_stacksize; }
    static constexpr auto get_end() { return mstd::GenericEndIterator(); }

    // DFSIterators for other traversal types are our friends!
    template<TraversalType, TraversalTraitsType>
    friend class DFSIterator;
  };



  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void>
             requires (is_node_traversal(o))
  class DFSNodeIterator: public DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet, is_reverse_traversal(o), _Forbidden>> {
    using Parent = DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet, is_reverse_traversal(o), _Forbidden>>;
  public:
    using Parent::Parent;
    using Parent::node_on_top;
    using typename Parent::reference;
    using typename Parent::pointer;

    //DFSNodeIterator() = default;
    //DFSNodeIterator(const DFSNodeIterator&) = default;
    //DFSNodeIterator(DFSNodeIterator&&) = default;
    //DFSNodeIterator& operator=(const DFSNodeIterator&) = default;
    //DFSNodeIterator& operator=(DFSNodeIterator&&) = default;

    DFSNodeIterator& operator++() { ++static_cast<Parent&>(*this); return *this; }
    DFSNodeIterator& operator++(int) { static_cast<Parent&>(*this)++; return *this; }

    pointer operator->() const {
      const auto& result = node_on_top();
      DEBUG6(std::cout << "DFS: emitting ptr to node " << result << "\n");
      return &result;
    }
    NodeDesc operator*() const {
      const NodeDesc result = node_on_top();
      DEBUG6(std::cout << "DFS: emitting node "<< result<<"\n");
      return result;
    }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void,
           template<class, class, bool, class> class _Traits = EdgeTraversalTraits>
             requires ((is_edge_traversal(o) || is_all_edge_traversal(o)) &&
                       TraversalTraitsType<_Traits<_Network, _SeenSet, is_reverse_traversal(o), _Forbidden>>)
  class DFSEdgeIterator: public DFSIterator<o, _Traits<_Network, _SeenSet, is_reverse_traversal(o), _Forbidden>> {
    using Parent = DFSIterator<o, _Traits<_Network, _SeenSet, is_reverse_traversal(o), _Forbidden>>;
  public:
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


    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSEdgeIterator(const NodeDesc _root, Args&&... args):
      Parent(_root, std::forward<Args>(args)...)
    {
      assert(_root != NoNode);
      if constexpr (o & preorder) 
        if(!is_seen(_root))
          Parent::advance();
    }
    template<class... Args>
    DFSEdgeIterator(const _Network& N, Args&&... args):
      DFSEdgeIterator(N.root(), std::forward<Args>(args)...) {}

    DFSEdgeIterator& operator++() { ++static_cast<Parent&>(*this); return *this; }
    DFSEdgeIterator& operator++(int) { static_cast<Parent&>(*this)++; return *this; }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void>
             requires (is_all_edge_traversal(o))
  using DFSAllEdgesIterator = DFSEdgeIterator<o, _Network, _SeenSet, _Forbidden, AllEdgesTraits>;

  // NOTE: an all-edge-tail-postorder is just a node-postorder with an additional auto_iter<SuccContainer> for the current node
  template<PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void>
  class DFSAllEdgesTailPOIterator: public DFSNodeIterator<postorder, _Network, _SeenSet, _Forbidden> {
    using Parent = DFSNodeIterator<postorder, _Network, _SeenSet, _Forbidden>;
    using Traits = AllEdgesTraits<_Network, _SeenSet, false, _Forbidden>;
    using InternalIter = mstd::auto_iter<typename _Network::SuccContainer>;
    InternalIter current_children;

    void advance_dfs_nodes() {
      if(Parent::is_valid()){
        while(1){
          Parent::operator++();
          if(!Parent::is_valid()) break;
          auto& u_node = node_of<_Network>(Parent::operator*());
          if(!(u_node.is_leaf())) {
            current_children = InternalIter(u_node.children());
            return;
          }
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
        if(!current_children.is_valid()) advance_dfs_nodes();
      }
      return *this;
    }

    auto& operator++(int) { DFSAllEdgesTailPOIterator result(*this); ++(*this); return result; }

    reference operator*() const { return {Parent::operator*(), *current_children}; }
    pointer operator->() = delete;
  };


  template<TraversalType o>
  struct _choose_iterator {
    template<StrictPhylogenyType _Network, OptionalNodeSetType _SeenSet, class _Forbidden>
    using type = DFSNodeIterator<o, _Network, _SeenSet, _Forbidden>;
  };
  template<TraversalType o> requires (is_edge_traversal(o))
  struct _choose_iterator<o> {
    template<StrictPhylogenyType _Network, OptionalNodeSetType _SeenSet, class _Forbidden>
    using type = DFSEdgeIterator<o, _Network, _SeenSet, _Forbidden>;
  };
  template<TraversalType o> requires (is_all_edge_traversal(o))
  struct _choose_iterator<o> {
    template<StrictPhylogenyType _Network, OptionalNodeSetType _SeenSet, class _Forbidden>
    using type = DFSAllEdgesIterator<o, _Network, _SeenSet, _Forbidden>;
  };
  template<>
  struct _choose_iterator<all_edge_tail_postorder> {
    template<StrictPhylogenyType _Network, OptionalNodeSetType _SeenSet, class _Forbidden>
    using type = DFSAllEdgesTailPOIterator<_Network, _SeenSet, _Forbidden>;
  };
  template<TraversalType o, StrictPhylogenyType _Network, OptionalNodeSetType _SeenSet, class _Forbidden>
  using choose_iterator = typename _choose_iterator<o>::template type<_Network, _SeenSet, _Forbidden>;



  // helper class to construct DFS iterators, keeping track of the seen nodes
  template<TraversalType o,
           PhylogenyType Network,
           class Roots,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper:
    public mstd::optional_tuple<pred::AsContainmentPred<Forbidden>, SeenSet>
  {
    using ForbiddenPred = pred::AsContainmentPred<Forbidden>;
    using Parent = mstd::optional_tuple<ForbiddenPred, SeenSet>;
    // we can pass both an iterator or a container as Roots
    using RootIter = mstd::iterator_of_t<Roots>;
    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using SeenSetRef = std::add_lvalue_reference_t<SeenSet>;
    using ForbiddenPredRef = std::add_lvalue_reference_t<ForbiddenPred>;
    using SingleRootIter = choose_iterator<o, Network, SeenSetRef, ForbiddenPredRef>;
    using SingleRootIterFac = mstd::IterFactory<SingleRootIter>;
    static constexpr bool is_multi_rooted = true;
    static constexpr bool has_forbidden_pred = std::is_void_v<Forbidden>;
    static constexpr bool has_seen_set = std::is_void_v<SeenSet>;
    
    // transformation turning each root into an iterfactory of DFS-iterators from that node
    template<class TransForbidden, class TransSeen>
    struct MultiRootIterTrans: mstd::optional_tuple<TransForbidden, TransSeen> {
      using Parent = mstd::optional_tuple<TransForbidden, TransSeen>;
      using Parent::Parent;

      auto operator()(const NodeDesc r) const {
        if constexpr (has_seen_set) {
          if constexpr (has_forbidden_pred) {
            return SingleRootIterFac(r, this->template get<0>(), this->template get<1>());
          } else {
            return SingleRootIterFac(r, this->template get<1>());
          }
        } else {
          if constexpr (has_forbidden_pred) {
            return SingleRootIterFac(r, this->template get<0>());
          } else {
            return SingleRootIterFac(r);
          }
        }
      };
    };

    using Iter = mstd::concatenating_iterator<mstd::transforming_iterator<RootIter, MultiRootIterTrans<ForbiddenPredRef, SeenSetRef>>>;
    using OwningIter = mstd::concatenating_iterator<mstd::transforming_iterator<RootIter, MultiRootIterTrans<ForbiddenPred, SeenSet>>>;

    mstd::auto_iter<RootIter> roots;

    template<class RootInit, class... Args>
    TraversalHelper(RootInit&& _roots, Args&&... args):
      Parent(std::forward<Args>(args)...),
      roots(std::forward<RootInit>(_roots))
    {}

    template<class... Args>
    TraversalHelper(const Network& N, Args&&... args):
      Parent(std::forward<Args>(args)...),
      roots(N.roots())
    {}

    // if we are traversing nodes, then empty() means that roots is empty
    // if we are traversing edges, then empty() means that there are no edges; to determine this, we have to check if all roots are leaves
    bool empty() const {
      if constexpr (!is_node_traversal(o)) {
        for(auto it = roots; it.valid(); ++it)
          if(!Network::is_leaf(*it))
            return false;
        return true;
      } else return !roots.valid();
    }

    auto begin() & { return Iter(std::piecewise_construct, std::forward_as_tuple(roots), std::forward_as_tuple(static_cast<Parent&>(*this))); }
    auto begin() const & { return Iter(std::piecewise_construct, std::forward_as_tuple(roots), std::forward_as_tuple(static_cast<const Parent&>(*this))); }
    // if we are going out of scope (which will be most of the cases), then move our seen-set into the constructed iterator
    auto begin() && { return OwningIter(std::piecewise_construct, std::forward_as_tuple(roots), std::forward_as_tuple(static_cast<Parent&&>(*this))); }
    // if called with one or more arguments, begin() constructs a new iterator using these arguments
    template<class... Args> requires (sizeof...(Args) != 0)
    auto begin(Args&&... args) { return Iter(std::piecewise_construct, std::forward_as_tuple(roots), std::forward_as_tuple(std::forward<Args>(args)...)); }

    static constexpr auto end() { return mstd::GenericEndIterator(); }
  };

  // in the single-rooted case, things become much simpler
  template<TraversalType o,
           PhylogenyType Network,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper<o, Network, void, SeenSet, Forbidden>:
    public mstd::optional_tuple<pred::AsContainmentPred<Forbidden>, SeenSet>
  {
    using ForbiddenPred = pred::AsContainmentPred<Forbidden>;
    using Parent = mstd::optional_tuple<ForbiddenPred, SeenSet>;

    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using SeenSetRef = std::add_lvalue_reference_t<SeenSet>;
    using ForbiddenPredRef = std::add_lvalue_reference_t<ForbiddenPred>;
    using Iter = choose_iterator<o, Network, SeenSetRef, ForbiddenPredRef>;
    using OwningIter = choose_iterator<o, Network, SeenSet, ForbiddenPred>;
    static constexpr bool is_multi_rooted = false;
    static constexpr bool has_forbidden_pred = std::is_void_v<Forbidden>;
    static constexpr bool has_seen_set = std::is_void_v<SeenSet>;
 
    NodeDesc root;

    template<class... Args>
    TraversalHelper(const NodeDesc _root, Args&&... args):
      Parent(std::forward<Args>(args)...),
      root(_root)
    {}
    template<class... Args>
    TraversalHelper(const NodeSingleton& _root, Args&&... args):
      Parent(std::forward<Args>(args)...),
      root(_root.empty() ? NoNode : front(_root))
    {}
    template<class... Args>
    TraversalHelper(const Network& N, Args&&... args):
      Parent(std::forward<Args>(args)...),
      root(N.root())
    {}

    bool empty() const {
      if constexpr (!is_node_traversal(o))
        return (root == NoNode) || (Network::is_leaf(root));
      else return root == NoNode;
    }

    auto begin() & { return Iter(root, static_cast<Parent&>(*this)); }
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
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper<o, Network, NodeSingleton, SeenSet, Forbidden>:
    public TraversalHelper<o, Network, void, SeenSet, Forbidden> {
    using TraversalHelper<o, Network, void, SeenSet, Forbidden>::TraversalHelper;
  };
  template<TraversalType o,
           PhylogenyType Network,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper<o, Network, NodeDesc, SeenSet, Forbidden>:
    public TraversalHelper<o, Network, void, SeenSet, Forbidden> {
    using TraversalHelper<o, Network, void, SeenSet, Forbidden>::TraversalHelper;
  };


  template<class _Roots, PhylogenyType _Net>
  using RootsOr = std::remove_cvref_t<mstd::VoidOr<_Roots, typename _Net::RootContainer>>;

  // this guy is our factory; begin()/end() can be called on it
  //NOTE: Traversal has its own _SeenSet so that multiple calls to begin() can be given the same set of forbidden nodes
  //      Thus, you can either call begin() - which uses the Traversal's _SeenSet, or you call begin(bla, ...) to construct a new _SeenSet from bla, ...
  //NOTE: per default, the Traversal's SeenSet is used direcly by the DFS iterator, so don't call begin() twice with a non-empty SeenSet!
  //      if you want to reuse the same SeenSet for multiple DFS' you have the following options:
  //      (a) reset the SeenSet between calls to begin()
  //      (b) call begin(bla, ...) instead of begin()
  template<TraversalType o,
           PhylogenyType Network,
           class _Roots = void,
           OptionalNodeSetType SeenSet = typename Network::DefaultSeen,
           class Forbidden = void>
  struct Traversal:
    public TraversalHelper<o, Network, RootsOr<_Roots, Network>, SeenSet, Forbidden>,
    public mstd::iterator_traits<typename TraversalHelper<o, Network, RootsOr<_Roots, Network>, SeenSet, Forbidden>::Iter>
  {
    using Roots = RootsOr<_Roots, Network>;
    using Parent = TraversalHelper<o, Network, Roots, SeenSet, Forbidden>;
    using Traits = mstd::iterator_traits<typename TraversalHelper<o, Network, Roots, SeenSet, Forbidden>::Iter>;
    using iterator = typename Parent::Iter;
    using const_iterator = iterator;
    using Parent::Parent;
    using typename Traits::value_type;
    using typename Traits::difference_type;
    using typename Traits::reference;
    using typename Traits::const_reference;
    using typename Traits::pointer;

    static constexpr bool track_nodes = iterator::track_nodes;

    // allow the user to play with the SeenSet and ForbiddenPrediacte at all times
    //NOTE: this gives you the power to change the SeenSet while the DFS is running, and with great power comes great responsibility ;] so be careful!
    auto& seen_nodes() { return Parent::template get<1>(); }
    const auto& seen_nodes() const { return Parent::template get<1>(); }
    auto& forbidden_predicate() { return Parent::template get<0>(); }
    const auto& forbidden_predicate() const { return Parent::template get<0>(); }

    template<mstd::ContainerType Container>
    Container& append_to(Container& c) { append(c, *this); return c; }
    template<mstd::ContainerType Container>
    Container to_container() { Container c; append(c, *this); return c; }
  };


  // these convenience declarations allow saying EdgeTraversal<postorder, ...> while, normally, "postorder" means node-postorder
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           OptionalNodeSetType SeenSet = typename Network::DefaultSeen,
           class Forbidden = void>
  using NodeTraversal = Traversal<o, Network, Roots, SeenSet, Forbidden>;
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           OptionalNodeSetType SeenSet = typename Network::DefaultSeen,
           class Forbidden = void>
  using EdgeTraversal = Traversal<TraversalType(o | edge_traversal), Network, Roots, SeenSet, Forbidden>;
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = void, // void = root container of the network
           OptionalNodeSetType SeenSet = Network::DefaultSeen,
           class Forbidden = void>
  using AllEdgesTraversal = Traversal<TraversalType(o | all_edge_traversal), Network, Roots, SeenSet, Forbidden>;


}// namespace
