
#pragma once

#include "set_interface.hpp"
#include "auto_iter.hpp"
#include "concat_iter.hpp"
#include "trans_iter.hpp"
#include "traversal_traits.hpp"
#include "types.hpp"

namespace PT{

/*
  // a shot at using C++20 coroutines to implement the DFS node generator
  template<TraversalType o, TraversalTraitsType Traits>
  class DFSIteratorCoro: public Traits
  {
    using MyStack = stack<pair<Node*, typename vector<Node*>::iterator>>;
  public:
    using typename Traits::Network;
    using Traits::track_nodes;
    using Traits::Traits;

    void push_all(Node* x, MyStack& nodes){
      do {
        auto it = x->children.begin();
        nodes.push({x, it});
        if constexpr (o & preorder) std::cout << x->index << '\n';
        if(it != x->children.end()) x = *it; else return;
      } while(1);
    }

    void print_dfs(Node* x){
      MyStack nodes;
      // phase1: push all to the first leaf
      push_all(x, nodes, o);
      // phase 2:
      while(!nodes.empty()){
        auto& top_pair = nodes.top();
        Node* np = top_pair.first;
        auto& it = top_pair.second;
        if(it != np->children.end()) {
          ++it;
          if(it != np->children.end()){
            if constexpr (o & inorder) std::cout << np->index << '\n';
            push_all(*it, nodes, o);
          } else {
            if constexpr (o & inorder)
              if(np->children.size() == 1) std::cout << np->index << '\n';
            if constexpr (o & postorder) std::cout << np->index << '\n';
            nodes.pop();
          }
        } else {
          if constexpr (o & postorder) std::cout << np->index << '\n';
          if constexpr (o & inorder) std::cout << np->index << '\n';
          nodes.pop();
        }
      }
    }
  };

*/



  // template specizalization to select the right DFS Iterator using a tag
  struct traversal_t {};
  struct node_traversal_t: public traversal_t {};
  struct edge_traversal_t: public traversal_t {};
  struct all_edges_traversal_t: public edge_traversal_t {};

  template<class T>
  concept TraversalTag = std::derived_from<T, traversal_t>;

  // forward-declare Traversals
  template<TraversalType o,
           TraversalTag Tag,
           PhylogenyType Network,
           class Roots = typename Network::RootContainer,
           OptionalNodeSetType SeenSet = typename Network::DefaultSeen,
           class Forbidden = void>
  struct Traversal;


  template<TraversalType o, TraversalTraitsType Traits>
  class DFSIterator: public Traits
  {
  public:
    using typename Traits::Network;
    using Traits::track_nodes;
   
  protected:
    using ChildIter = std::auto_iter<typename Traits::child_iterator>;
    using Stack = std::vector<ChildIter>;
    using Traits::get_children;
    using Traits::get_node;
    using Traits::min_stacksize;
    using Traits::mark_seen;
    using Traits::is_seen;
    using ChildContainerRef = typename Traits::ItemContainerRef;
  
    const NodeDesc root;
    
    // the history of descents in the network, for each descent, save the iterator of the current child and the end()-iterator
    Stack child_history;

    // dive deeper into the network, up to the next emittable node x, putting ranges on the stack (including that of x); return the current node
    // if we hit a node that is already seen, return that node
    void dive(const NodeDesc& u)
    {
      ChildContainerRef u_children = get_children(u);
      child_history.emplace_back(u_children);

      // make sure we start with an unseen child
      if constexpr(track_nodes) {
        const size_t num_skipped = skip_seen_children();
        // ATTENTION: when track_nodes is active, we may skip all but one child of u; when in in-order mode, this is problematic
        //    because we would want to output u after its last child in this case...
        //    slightly abusing our data structures, we can note this down in the seen_set...
        if constexpr(o & inorder)
          if(node_of<Network>(u).out_degree() == num_skipped + 1) mark_seen(u);
      }

      // if a pre-order is requested, then u itself is emittable, so don't put more stuff on the stack; otherwise, keep diving to the first unseen child
      if constexpr (!(o & preorder))
        if(!current_node_finished())
          dive(get_node(*(child_history.back())));
    }

    // return whether all children of the current node have been treated
    bool current_node_finished() const
    {
      assert(!child_history.empty());
      return child_history.back().is_invalid();
    }

    // get the node whose child-iterators are on top of the stack
    const NodeDesc& node_on_top() const
    {
      // if there are at least 2 ranges on the stack, dereference the second to last to get the current node, otherwise, it's root
      //std::cout << "node on top is " << result << "\n";
      return (child_history.size() > 1) ? get_node(*(child_history[child_history.size() - 2])) : root;
    }

    // when we're done treating all children, go back and continue with the parent
    void backtrack()
    {
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
        if(current_child.is_valid()){
          if constexpr (!(o & inorder)) dive(get_node(*(child_history.back())));
        } else {
          if constexpr (o & postorder) return; // in post-order, output the node instead of backtracking further
          // in in-order, if all but at most one child have been skipped, then output the node instead of backtrack
          const NodeDesc& u = node_on_top();
          if((o & inorder) && (node_of<Network>(u).out_degree() <= num_skipped + 1)) return;
          if((o & inorder) && is_seen(u)) return;
          backtrack(); // if the children are spent then keep popping end-iterators unless postorder is requested
        }
      }
    }

    // skip over all seen children of the current node
    size_t skip_seen_children()
    {
      size_t result = 0;
      ChildIter& current = child_history.back();
      // skip over all seen children
      while(current.is_valid() && is_seen(*current)) {++current; ++result;}
      return result;
    }

  public:
    // NOTE: use this to construct an end-iterator
    DFSIterator(): Traits(), root(NoNode) {}

    // construct from a given traversal
    //template<TraversalType _o, TraversalTag Tag, OptionalNodeSetType _SeenSet>
    //DFSIterator(const Traversal<_o, Tag, Network, _SeenSet>& traversal): DFSIterator(traversal.begin()) {}

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSIterator(const NodeDesc& _root, Args&&... args):
      Traits(std::forward<Args>(args)...), root(_root)
    { 
      DEBUG5(std::cout << "making new non-end DFS iterator (type "<< o <<") starting at "<<_root<<" (tracking? "<<track_nodes<<")\n");
      if constexpr (track_nodes) {
        if(!is_seen(root)) dive(root);
      } else dive(root);
    }

    DFSIterator& operator++()
    {
      if(current_node_finished()){
        // since we're done with the node_on_top now, go backtrack()
        backtrack();
      } else {
        // the current node is not finished, so
        //  (b) either we're doing pre-order and we're currently diving
        //  (a) or we're doing inorder and we just returned from a subtree with backtrack()
        // in both cases, we continue with a dive()
        dive(get_node(*(child_history.back())));
      }
      return *this;
    }
    DFSIterator operator++(int) { DFSIterator tmp(*this); ++(*this); return tmp; }

    // NOTE: we roughly estimate that two DFSIterators are equal if they are both the end iterator or they have the same root, stack size and top node
    template<TraversalType __o, TraversalTraitsType __Traits>
    bool operator==(const DFSIterator<__o, __Traits>& other) const
    {
      if(other.is_valid()){
        return is_valid() && (root == other.root) && (child_history.size() == other.child_history.size()) && (node_on_top() == other.node_on_top());
      } else return !is_valid();
    }
    template<TraversalType __o, TraversalTraitsType __Traits>
    bool operator!=(const DFSIterator<__o, __Traits>& other) const { return !operator==(other); }


    bool is_valid() const { return child_history.size() >= min_stacksize; }
    constexpr DFSIterator get_end() const { return {}; }

    // DFSIterators for other traversal types are our friends!
    template<TraversalType, TraversalTraitsType>
    friend class DFSIterator;
  };



  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void>
  class DFSNodeIterator: public DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet, _Forbidden>> {
    using Parent = DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet, _Forbidden>>;
  public:
    using Parent::Parent;
    using Parent::node_on_top;
    using typename Parent::reference;
    using typename Parent::pointer;

    pointer operator->() const {
      const auto& result = node_on_top();
      DEBUG4(std::cout << "\temitting ptr to node " << result << "\n");
      return &result;
    }
    const NodeDesc& operator*() const {
      const NodeDesc& result = node_on_top();
      DEBUG4(std::cout << "\temitting node "<< result<<"\n");
      return result;
    }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void,
           TraversalTraitsType _Traits = EdgeTraversalTraits<_Network, _SeenSet, _Forbidden>>
  class DFSEdgeIterator: public DFSIterator<o, _Traits> {
  public:
    using Parent = DFSIterator<o, _Traits>;
    using Parent::Parent;
    using Parent::node_on_top;
    using typename Parent::Network;
    using typename Parent::reference;
    using typename Parent::pointer;
    using Parent::child_history;
    
    pointer operator->() const
    {
      assert(child_history.size() > 1);
      DEBUG4(std::cout << "\temitting edge "<<*(child_history[child_history.size() - 2])<<"\n";)
      return *(child_history[child_history.size() - 2]);
    }
    reference operator*() const { return *(operator->()); }

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSEdgeIterator(const NodeDesc& _root, Args&&... args):
      Parent(_root, std::forward<Args>(args)...)
    {
      if constexpr (o & preorder) Parent::operator++();
    }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void>
  using DFSAllEdgesIterator = DFSEdgeIterator<o, _Network, _SeenSet, _Forbidden, AllEdgesTraits<_Network, _SeenSet, _Forbidden>>;


  template<TraversalType o, TraversalTag Tag, OptionalNodeSetType _SeenSet, class _Forbidden>
  struct _choose_iterator
  { template<PhylogenyType _Network> using type = DFSNodeIterator<o, _Network, _SeenSet, _Forbidden>; };
  
  template<TraversalType o, OptionalNodeSetType _SeenSet, class _Forbidden>
  struct _choose_iterator<o, edge_traversal_t, _SeenSet, _Forbidden>
  { template<PhylogenyType _Network> using type = DFSEdgeIterator<o, _Network, _SeenSet, _Forbidden>; };

  template<TraversalType o, OptionalNodeSetType _SeenSet, class _Forbidden>
  struct _choose_iterator<o, all_edges_traversal_t, _SeenSet, _Forbidden>
  { template<PhylogenyType _Network> using type = DFSAllEdgesIterator<o, _Network, _SeenSet, _Forbidden>; };


  template<TraversalType o, TraversalTag Tag, OptionalNodeSetType _SeenSet, class _Forbidden, PhylogenyType Net>
  using choose_iterator = typename _choose_iterator<o, Tag, _SeenSet, _Forbidden>::template type<Net>;



  // helper class to construct DFS iterators, keeping track of the seen nodes
  template<TraversalType o,
           TraversalTag Tag,
           PhylogenyType Network,
           class Roots,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper: public std::optional_tuple<pred::AsContainmentPred<Forbidden>, SeenSet> {
    using ForbiddenPred = pred::AsContainmentPred<Forbidden>;
    using Parent = std::optional_tuple<ForbiddenPred, SeenSet>;
    // we can pass both an iterator or a container as Roots
    using RootIter = std::iterator_of_t<Roots>;
    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using SeenSetRef = std::add_lvalue_reference_t<SeenSet>;
    using ForbiddenPredRef = std::add_lvalue_reference_t<ForbiddenPred>;
    using SingleRootIter = choose_iterator<o, Tag, SeenSetRef, ForbiddenPredRef, Network>;
    using SingleRootIterFac = std::IterFactory<SingleRootIter>;
    static constexpr bool is_multi_rooted = true;
    static constexpr bool has_forbidden_pred = std::is_void_v<Forbidden>;
    static constexpr bool has_seen_set = std::is_void_v<SeenSet>;
    
    // transformation turning each root into an iterfactory of DFS-iterators from that node
    template<class TransForbidden, class TransSeen>
    struct MultiRootIterTrans: std::optional_tuple<TransForbidden, TransSeen> {
      using Parent = std::optional_tuple<TransForbidden, TransSeen>;
      using Parent::Parent;

      auto operator()(const NodeDesc& r) const {
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

    using Iter = std::concatenating_iterator<std::transforming_iterator<RootIter, MultiRootIterTrans<ForbiddenPredRef, SeenSetRef>>>;
    using OwningIter = std::concatenating_iterator<std::transforming_iterator<RootIter, MultiRootIterTrans<ForbiddenPred, SeenSet>>>;


    std::auto_iter<RootIter> roots;

    template<class RootInit, class... Args>
    TraversalHelper(RootInit&& _roots, Args&&... args):
      Parent(std::forward<Args>(args)...),
      roots(std::forward<RootInit>(_roots))
    {}

    // if we are traversing nodes, then empty() means that roots is empty
    // if we are traversing edges, then empty() means that there are no edges; to determine this, we have to check if all roots are leaves
    bool empty() const {
      if constexpr (!std::is_same_v<Tag, node_traversal_t>) {
        for(auto it = roots; it.valid(); ++it)
          if(!node_of<Network>(*it).is_leaf())
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

    static auto end() { return std::GenericEndIterator(); }
  };

  // in the single-rooted case, things become much simpler
  template<TraversalType o,
           TraversalTag Tag,
           PhylogenyType Network,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper<o, Tag, Network, void, SeenSet, Forbidden>: public std::optional_tuple<pred::AsContainmentPred<Forbidden>, SeenSet> {
    using ForbiddenPred = pred::AsContainmentPred<Forbidden>;
    using Parent = std::optional_tuple<ForbiddenPred, SeenSet>;

    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using SeenSetRef = std::add_lvalue_reference_t<SeenSet>;
    using ForbiddenPredRef = std::add_lvalue_reference_t<ForbiddenPred>;
    using Iter = choose_iterator<o, Tag, SeenSetRef, ForbiddenPredRef, Network>;
    using OwningIter = choose_iterator<o, Tag, SeenSet, ForbiddenPred, Network>;
    static constexpr bool is_multi_rooted = false;
    static constexpr bool has_forbidden_pred = std::is_void_v<Forbidden>;
    static constexpr bool has_seen_set = std::is_void_v<SeenSet>;
 
    NodeDesc root;

    template<class... Args>
    TraversalHelper(const NodeDesc& _root, Args&&... args):
      Parent(std::forward<Args>(args)...),
      root(_root)
    {}
    template<class... Args>
    TraversalHelper(const NodeSingleton& _root, Args&&... args):
      Parent(std::forward<Args>(args)...),
      root(front(_root))
    {}

    bool empty() const {
      if constexpr (!std::is_same_v<Tag, node_traversal_t>) 
        return (root == NoNode) || (node_of<Network>(root).is_leaf());
      else return root == NoNode;
    }

    auto begin() & { return Iter(root, static_cast<Parent&>(*this)); }
    // if we are const, then our seen-set is const and so, the iterator we create cannot reference our seen-set; thus, we'll have to copy it :/
    auto begin() const & { return OwningIter(root, static_cast<const Parent&>(*this)); }
    // if we are going out of scope (which will be most of the cases), then move our seen-set into the constructed iterator
    auto begin() && { return OwningIter(root, static_cast<Parent&&>(*this)); }
    // if called with one or more arguments, begin() constructs a new iterator using these arguments
    template<class... Args> requires (sizeof...(Args) != 0)
    auto begin(Args&&... args) { return Iter(root, std::forward<Args>(args)...); }

    static auto end() { return std::GenericEndIterator(); }
  };
  template<TraversalType o,
           TraversalTag Tag,
           PhylogenyType Network,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct TraversalHelper<o, Tag, Network, NodeSingleton, SeenSet, Forbidden>:
    public TraversalHelper<o, Tag, Network, void, SeenSet, Forbidden> {
    using TraversalHelper<o, Tag, Network, void, SeenSet, Forbidden>::TraversalHelper;
  };
 


  // this guy is our factory; begin()/end() can be called on it
  //NOTE: Traversal has its own _SeenSet so that multiple calls to begin() can be given the same set of forbidden nodes
  //      Thus, you can either call begin() - which uses the Traversal's _SeenSet, or you call begin(bla, ...) to construct a new _SeenSet from bla, ...
  //NOTE: per default, the Traversal's SeenSet is used direcly by the DFS iterator, so don't call begin() twice with a non-empty SeenSet!
  //      if you want to reuse the same SeenSet for multiple DFS' you have the following options:
  //      (a) reset the SeenSet between calls to begin()
  //      (b) call begin(bla, ...) instead of begin()
  template<TraversalType o,
           TraversalTag Tag,
           PhylogenyType Network,
           class Roots,
           OptionalNodeSetType SeenSet,
           class Forbidden>
  struct Traversal:
    public TraversalHelper<o, Tag, Network, Roots, SeenSet, Forbidden>,
    public std::my_iterator_traits<typename TraversalHelper<o, Tag, Network, Roots, SeenSet, Forbidden>::Iter>
  {
    using Parent = TraversalHelper<o, Tag, Network, Roots, SeenSet, Forbidden>;
    using iterator = typename Parent::Iter;
    using const_iterator = iterator;
    using Parent::Parent;

    static constexpr bool track_nodes = iterator::track_nodes;

    // allow the user to play with the SeenSet and ForbiddenPrediacte at all times
    //NOTE: this gives you the power to change the SeenSet while the DFS is running, and with great power comes great responsibility ;] so be careful!
    auto& seen_nodes() { return Parent::template get<1>(); }
    const auto& seen_nodes() const { return Parent::template get<1>(); }
    auto& forbidden_predicate() { return Parent::template get<0>(); }
    const auto& forbidden_predicate() const { return Parent::template get<0>(); }

    template<std::ContainerType Container>
    Container& append_to(Container& c) { append(c, *this); return c; }
    /*Container& append_to(Container& c) {
      std::cout << "appending to container\n";
      for(auto iter = this->begin(); iter != this->end(); ++iter) append(c, *iter);
      return c;
    }*/

  };


  template<TraversalType o,
           PhylogenyType Network,
           class Roots = typename Network::RootContainer,
           OptionalNodeSetType SeenSet = typename Network::DefaultSeen,
           class Forbidden = void>
  using NodeTraversal = Traversal<o, node_traversal_t, Network, Roots, SeenSet, Forbidden>;
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = typename Network::RootContainer,
           OptionalNodeSetType SeenSet = typename Network::DefaultSeen,
           class Forbidden = void>
  using EdgeTraversal = Traversal<o, edge_traversal_t, Network, Roots, SeenSet, Forbidden>;
  template<TraversalType o,
           PhylogenyType Network,
           class Roots = typename Network::RootContainer,
           OptionalNodeSetType SeenSet = Network::DefaultSeen,
           class Forbidden = void>
  using AllEdgesTraversal = Traversal<o, all_edges_traversal_t, Network, Roots, SeenSet, Forbidden>;

/*
  // now this is the neat interface to the outer world - it can spawn any form of Traversal object and it's easy to interact with :)
  template<PhylogenyType _Network,
           template<TraversalType, class, class, class> class _Traversal,
           OptionalNodeSetType _IteratorSeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void,
           OptionalNodeSetType _GlobalSeenSet = std::conditional_t<std::is_void_v<_IteratorSeenSet>, void, std::remove_reference_t<_IteratorSeenSet>>>
  class MetaTraversal: public std::optional_tuple<AsContainmentPred<_Forbidden>, _GlobalSeenSet> {
    using _ForbiddenPredicate = AsContainmentPred<_Forbidden>;
    using Parent = std::optional_tuple<_ForbiddenPredicate, _GlobalSeenSet>;
  public:
    using Network = _Network;
    using GlobalSeenSet = _GlobalSeenSet;
    using IteratorSeenSet = _IteratorSeenSet;
    using ForbiddenPredicate = _ForbiddenPredicate;
  protected:
    NodeDesc root;
  public:
    template<TraversalType o> using Traversal = _Traversal<o, Network, IteratorSeenSet, ForbiddenPredicate>;

    template<class... Args>
    MetaTraversal(const NodeDesc& _root, Args&&... args): Parent(std::forward<First>(first), std::forward<Args>(args)...), root(_root) {}

    template<class First, class... Args> requires !std::is_same_v<std::remove_cvref_t<First>, NodeDesc>
    MetaTraversal(First&& first, Args&&... args): Parent(std::forward<First>(first), std::forward<Args>(args)...), root(NoNode) {}

    // this one is the general form that can do any order, but is a little more difficult to use
    template<TraversalType o>
    static Traversal<o> traversal(const NodeDesc& u, const order<o> = order<o>()) { return Traversal<o>(u, static_cast<Parent&>(*this)); }
    template<TraversalType o>
    Traversal<o> traversal(const order<o> = order<o>()) { return Traversal<o>(root, static_cast<Parent&>(*this)); }

    // then, we add 3 predefined orders: preorder, inorder, postorder, that are easier to use
    // preorder:
    static Traversal<PT::preorder> preorder(const NodeDesc& u) { return traversal<PT::preorder>(u); }
    Traversal<PT::preorder> preorder() { return preorder(root); }

    // inorder:
    static Traversal<PT::inorder> inorder(const NodeDesc& u) { return traversal<PT::inorder>(u); }
    Traversal<PT::inorder> inorder() { return inorder(root); }

    // postorder:
    static Traversal<PT::postorder> postorder(const NodeDesc& u) { return traversal<PT::postorder>(u); }
    Traversal<PT::postorder> postorder() { return postorder(root); }
  };




  // a meta-traversal acting on multiple roots
  template<PhylogenyType _Network,
           NodeIterableType RootContainer,
           template<TraversalType, class, class, class> class _Traversal,
           OptionalNodeSetType _IteratorSeenSet = typename _Network::DefaultSeen,
           class _Forbidden = void,
           OptionalNodeSetType _GlobalSeenSet = std::conditional_t<std::is_void_v<_IteratorSeenSet>, void, std::remove_reference_t<_IteratorSeenSet>>>
  class MultiRootMetaTraversal: public std::optional_tuple<AsContainmentPred<_Forbidden>, _GlobalSeenSet> {
    using _ForbiddenPredicate = AsContainmentPred<_Forbidden>;
    using Parent = std::optional_tuple<_ForbiddenPredicate, _GlobalSeenSet>;
    using MT = MetaTraversal<_Network, _Traversal,
      std::add_lvalue_reference_t<_IteratorSeenSet>, std::add_lvalue_reference_t<_ForbiddenPredicate>, std::add_lvalue_reference_t<_GlobalSeenSet>>;
    using RootIter = std::auto_iter<RootContainer>;
    
    template<TraversalType o> using Traversal = typename MT::template Traversal<o>;

    // transformation turning each root into a traversal from that root
    template<TraversalType o>
    struct MultiRootIterTrans {
      Traversal<o> operator()(const NodeDesc& r) const { return MT(*this).template traversal<o>(r); };
    };

    template<TraversalType o>
    using Iterator = std::concatenating_iterator<std::transforming_iterator<RootIter, MultiRootIterTrans<o>>>;
    template<TraversalType o>
    using Factory = std::IterFactory<Iterator<o>>;


    template<TraversalType o>
    static Factory<o> traversal(const RootContainer& _roots, const order<o> = order<o>()) { return {_roots, *this}; }
    template<TraversalType o>
    static Factory<o> traversal(const RootIter& _roots, const order<o> = order<o>()) { return {_roots, *this}; }
  };
*/
}// namespace
