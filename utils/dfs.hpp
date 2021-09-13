
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
    using Traits::track_seen;
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
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet>
  struct Traversal;

  template<TraversalType o, TraversalTraitsType Traits>
  class DFSIterator: public Traits
  {
  public:
    using typename Traits::Network;
    using Traits::track_seen;
   
  protected:
    using ChildIter = std::auto_iter<typename Traits::child_iterator>;
    using Stack = std::vector<ChildIter>;
    using Traits::get_children;
    using Traits::get_node;
    using Traits::min_stacksize;
    using Traits::mark_seen;
    using Traits::is_seen;
    using ChildContainerRef = typename Traits::ItemContainerRef;
  
    Network& N;
    const NodeDesc root;
    
    // the history of descents in the network, for each descent, save the iterator of the current child and the end()-iterator
    Stack child_history;

    // dive deeper into the network, up to the next emittable node x, putting ranges on the stack (including that of x); return the current node
    // if we hit a node that is already seen, return that node
    void dive(const NodeDesc& u)
    {
      ChildContainerRef u_children = get_children(N, u);
      child_history.emplace_back(u_children);

      // make sure we start with an unseen child
      if constexpr(track_seen) {
        const size_t num_skipped = skip_seen_children();
        // ATTENTION: when track_seen is active, we may skip all but one child of u; when in in-order mode, this is problematic
        //    because we would want to output u after its last child in this case...
        //    slightly abusing our data structures, we can note this down in the seen_set...
        if constexpr(o & inorder)
          if(N.out_degree(u) == num_skipped + 1) mark_seen(u);
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
      const NodeDesc& result = (child_history.size() > 1) ? get_node(*(child_history[child_history.size() - 2])) : root;
      //std::cout << "node on top is " << result << "\n";
      return result;
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
        if constexpr(track_seen) mark_seen(*current_child);
        ++current_child;

        // if now the child at current_iter has been seen, skip it (and all following)
        size_t num_skipped = 0;
        if constexpr(track_seen) num_skipped = skip_seen_children();

        // if there are still unseen children then dive() into the next subtree unless inorder is requested
        if(current_child.is_valid()){
          if constexpr (!(o & inorder)) dive(get_node(*(child_history.back())));
        } else {
          if constexpr (o & postorder) return; // in post-order, output the node instead of backtracking further
          // in in-order, if all but at most one child have been skipped, then output the node instead of backtrack
          if((o & inorder) && (N.out_degree(node_on_top()) <= num_skipped + 1)) return;
          if((o & inorder) && is_seen(node_on_top())) return;
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
    DFSIterator(Network& _N): Traits(), N(_N), root(NoNode) {}

    // construct from a given traversal
    template<TraversalType _o, TraversalTag Tag, OptionalNodeSetType _SeenSet>
    DFSIterator(const Traversal<_o, Tag, Network, _SeenSet>& traversal): DFSIterator(traversal.begin()) {}

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSIterator(Network& _N, const NodeDesc& _root, Args&&... args):
      Traits(std::forward<Args>(args)...), N(_N), root(_root)
    { 
      DEBUG5(std::cout << "making new non-end DFS iterator (type "<< o <<") starting at "<<_root<<" (tracking? "<<track_seen<<")\n");
      if constexpr (track_seen) {
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
    constexpr DFSIterator get_end() const { return N; }

    // DFSIterators for other traversal types are our friends!
    template<TraversalType, TraversalTraitsType>
    friend class DFSIterator;
  };



  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet>
  class DFSNodeIterator: public DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet>>
  {
    using Parent = DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet>>;
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
    reference operator*() const {
      const auto& result = node_on_top();
      DEBUG4(std::cout << "\temitting node "<< result<<"\n");
      return result;
    }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet,
           TraversalTraitsType _Traits = EdgeTraversalTraits<_Network, _SeenSet>>
  class DFSEdgeIterator: public DFSIterator<o, _Traits>
  {
    using Parent = DFSIterator<o, _Traits>;
  public:
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
    DFSEdgeIterator(const Network& _N, const NodeDesc& _root, Args&&... args):
      Parent(_N, _root, std::forward<Args>(args)...)
    {
      if constexpr (o & preorder) Parent::operator++();
    }
  };

  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet>
  using DFSAllEdgesIterator = DFSEdgeIterator<o, _Network, _SeenSet, AllEdgesTraits<_Network, _SeenSet>>;

  template<TraversalType o, TraversalTag Tag, OptionalNodeSetType _SeenSet>
  struct _choose_iterator
  { template<PhylogenyType _Network> using type = DFSNodeIterator<o, _Network, _SeenSet>; };
  
  template<TraversalType o, OptionalNodeSetType _SeenSet>
  struct _choose_iterator<o, edge_traversal_t, _SeenSet>
  { template<PhylogenyType _Network> using type = DFSEdgeIterator<o, _Network, _SeenSet>; };
  
  template<TraversalType o, OptionalNodeSetType _SeenSet>
  struct _choose_iterator<o, all_edges_traversal_t, _SeenSet>
  { template<PhylogenyType _Network> using type = DFSAllEdgesIterator<o, _Network, _SeenSet>; };

  template<TraversalType o, TraversalTag Tag, OptionalNodeSetType _SeenSet, PhylogenyType Net>
  using choose_iterator = typename _choose_iterator<o, Tag, _SeenSet>::template type<Net>;



  // helper class to construct DFS iterators, keeping track of the seen nodes
  template<PhylogenyType Network, OptionalNodeSetType SeenSet>
  struct TraversalHelper {
    Network& N;
    const NodeDesc root;
    SeenSet seen;

    template<class Iterator> Iterator begin() & { return Iterator(N, root, seen); }
    template<class Iterator> Iterator begin() const & { return Iterator(N, root, seen); }
    // if we are going out of scope (which will be most of the cases), then move our seen-set into the constructed iterator
    template<class Iterator> Iterator begin() && { return Iterator(N, root, std::move(seen)); }
    // if called with one or more arguments, begin() constructs a new iterator using these arguments
    template<class Iterator, class... Args> requires (sizeof...(Args) != 0)
    Iterator begin(Args&&... args) { return Iterator(N, root, std::forward<Args>(args)...); }

    template<class EndIterator> EndIterator end() { return EndIterator(N); }
    template<class EndIterator> EndIterator end() const { return EndIterator(N); }
  };
  // specialization without seenset
  template<PhylogenyType Network>
  struct TraversalHelper<Network, void> {
    Network& N;
    const NodeDesc root;

    template<class Iterator> Iterator begin() & { return Iterator(N, root); }
    template<class Iterator> Iterator begin() const & { return Iterator(N, root); }
    // if we are going out of scope (which will be most of the cases), then move our seen-set into the constructed iterator
    template<class Iterator> Iterator begin() && { return Iterator(N, root); }
    // if called with one or more arguments, begin() constructs a new iterator using these arguments
    template<class Iterator, class... Args> requires (sizeof...(Args) != 0)
    Iterator begin(Args&&... args) { return Iterator(N, root, std::forward<Args>(args)...); }

    template<class EndIterator> EndIterator end() { return EndIterator(N); }
    template<class EndIterator> EndIterator end() const { return EndIterator(N); }
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
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet>
  struct Traversal
  {
    template<class _Net>
    using _Iterator = choose_iterator<o, Tag, _SeenSet, _Net>;
    template<class _Net>
    using _EndIterator = choose_iterator<o, Tag, void, _Net>;

    using iterator = _Iterator<_Network>;
    using const_iterator = _Iterator<const _Network>;
    using end_iterator = _EndIterator<_Network>;
    using end_const_iterator = _EndIterator<const _Network>;

    using value_type = typename std::my_iterator_traits<iterator>::value_type;
    using difference_type = ptrdiff_t;
    using size_type = size_t;
    using reference  = typename std::my_iterator_traits<iterator>::reference;
    using pointer    = typename std::my_iterator_traits<iterator>::pointer;
    using const_reference = typename std::my_iterator_traits<const_iterator>::reference;
    using const_pointer   = typename std::my_iterator_traits<const_iterator>::const_pointer;
    static constexpr bool track_seen = iterator::track_seen;

    TraversalHelper<_Network, _SeenSet> helper; // this contains a gloabl SeenSet with which each non-end iterator is instanciated

    template<class... Args>
    Traversal(_Network& _N, const NodeDesc& _root, Args&&... args):
      helper{_N, _root, std::forward<Args>(args)...}
    {
      DEBUG4(
      std::cout << "creating ";
      if constexpr (std::is_same_v<Tag, node_traversal_t>) std::cout << "node traversal"; else std::cout << "edge traversal";
      std::cout << " from node "<<helper.root<<'\n';
      );
    }

    // if called without arguments, begin() constructs an iterator using the root and _SeenSet created by our constructor
    iterator begin() & { return helper.template begin<iterator>(); }
    const_iterator begin() const & { return helper.template begin<const_iterator>(); }
    iterator begin() && { return std::move(helper).template begin<iterator>(); }

    // if called with one or more arguments, begin() constructs a new iterator using these arguments
    template<class... Args> requires (sizeof...(Args) != 0)
    iterator begin(Args&&... args) { return helper.template begin<iterator>(std::forward<Args>(args)...); }
    template<class... Args> requires (sizeof...(Args) != 0)
    const_iterator begin(Args&&... args) const { return helper.template begin<const_iterator>(std::forward<Args>(args)...); }

    end_iterator end() { return helper.template end<end_iterator>(); }
    end_const_iterator end() const { return helper.template end<end_const_iterator>(); }

    // allow the user to play with the _SeenSet at all times
    //NOTE: this gives you the power to change the _SeenSet while the DFS is running, and with great power comes great responsibility ;] so be careful!
    auto& seen_nodes() { return helper.seen; }
    const auto& seen_nodes() const { return helper.seen; }

    bool empty() const { if constexpr (std::is_same_v<Tag, node_traversal_t>) return helper.N.empty(); else return helper.N.edgeless(); }

    template<std::ContainerType Container>
    //Container& append_to(Container& c) { append(c, *this); return c; }
    Container& append_to(Container& c) {
      std::cout << "appending to container\n";
      auto iter = this->begin();
      while(iter != this->end()) { append(c, *iter); ++iter; }
      return c;
    }

  };


  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet>
  using NodeTraversal = Traversal<o, node_traversal_t, _Network, _SeenSet>;
  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet>
  using EdgeTraversal = Traversal<o, edge_traversal_t, _Network, _SeenSet>;
  template<TraversalType o,
           PhylogenyType _Network,
           OptionalNodeSetType _SeenSet = typename _Network::NodeSet>
  using AllEdgesTraversal = Traversal<o, all_edges_traversal_t, _Network, _SeenSet>;


  // now this is the neat interface to the outer world - it can spawn any form of Traversal object and it's easy to interact with :)
  //NOTE: this is useless when const (but even const Networks can return a non-const MetaTraversal)
  template<PhylogenyType _Network,
           template<TraversalType, class, class> class _Traversal,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen>
  class MetaTraversal {
  public:
    using Network = _Network;
    using SeenSet = _SeenSet;
  protected:
    Network& N;
    std::remove_reference_t<SeenSet> seen;
  public:

    template<TraversalType o>
    using Traversal = _Traversal<o, Network, SeenSet>;

    template<class... Args>
    MetaTraversal(Network& _N, Args&&... args): N(_N), seen(std::forward<Args>(args)...) {}

    // this one is the general form that can do any order, but is a little more difficult to use
    template<TraversalType o>
    Traversal<o> traversal(const NodeDesc& u, const order<o> = order<o>()) { return Traversal<o>(N, u, seen); }
    template<TraversalType o>
    Traversal<o> traversal(const order<o> = order<o>()) { return Traversal<o>(N, N.root(), seen); }

    // then, we add 3 predefined orders: preorder, inorder, postorder, that are easier to use
    // preorder:
    Traversal<PT::preorder> preorder(const NodeDesc& u) { return traversal<PT::preorder>(u); }
    Traversal<PT::preorder> preorder() { return preorder(N.root()); }

    // inorder:
    Traversal<PT::inorder> inorder(const NodeDesc& u) { return traversal<PT::inorder>(u); }
    Traversal<PT::inorder> inorder() { return inorder(N.root()); }

    // postorder:
    Traversal<PT::postorder> postorder(const NodeDesc& u) { return traversal<PT::postorder>(u); }
    Traversal<PT::postorder> postorder() { return postorder(N.root()); }
  };


  template<PhylogenyType _Network,
           template<TraversalType, class, class> class _Traversal>
  class MetaTraversal<_Network, _Traversal, void> {
  public:
    using Network = _Network;
    using SeenSet = void;
  protected:
    Network& N;
  public:

    template<TraversalType o>
    using Traversal = _Traversal<o, Network, void>;

    template<class... Args>
    MetaTraversal(Network& _N): N(_N) {}

    // this one is the general form that can do any order, but is a little more difficult to use
    template<TraversalType o>
    Traversal<o> traversal(const NodeDesc& u, const order<o> = order<o>()) { return Traversal<o>(N, u); }
    template<TraversalType o>
    Traversal<o> traversal(const order<o> = order<o>()) { return Traversal<o>(N, N.root()); }

    // then, we add 3 predefined orders: preorder, inorder, postorder, that are easier to use
    // preorder:
    Traversal<PT::preorder> preorder(const NodeDesc& u) { return traversal<PT::preorder>(u); }
    Traversal<PT::preorder> preorder() { return preorder(N.root()); }

    // inorder:
    Traversal<PT::inorder> inorder(const NodeDesc& u) { return traversal<PT::inorder>(u); }
    Traversal<PT::inorder> inorder() { return inorder(N.root()); }

    // postorder:
    Traversal<PT::postorder> postorder(const NodeDesc& u) { return traversal<PT::postorder>(u); }
    Traversal<PT::postorder> postorder() { return postorder(N.root()); }
  };


  // a meta-traversal acting on multiple roots
  template<PhylogenyType _Network,
           NodeIterableType RootContainer,
           template<TraversalType, class, class> class _Traversal,
           OptionalNodeSetType _SeenSet = typename _Network::DefaultSeen>
  class MultiRootMetaTraversal {
    // NOTE: Since the SeenSet has to be carried over between the concatenated traversals, we're using a reference to a global SeenSet.
    using MT = MetaTraversal<_Network, _Traversal, std::add_lvalue_reference<_SeenSet>>;
    using RootIter = std::auto_iter<RootContainer>;

    template<TraversalType o>
    using Traversal = typename MT::template Traversal<o>;

    // transformation turning each root into a traversal from that root
    template<TraversalType o>
    struct IterTrans {
      MT meta_traversal;
      Traversal<o> operator()(const NodeDesc& r) const { return meta_traversal.template traversal<o>(r); };
    };

    template<TraversalType o>
    using Iterator = std::concatenating_iterator<
                        std::transforming_iterator<RootIter, IterTrans<o>>
                     >;
    template<TraversalType o>
    using Factory = std::IterFactory<Iterator<o>>;

    _Network& N;
  public:
    MultiRootMetaTraversal(_Network& _N): N(_N) {}

    template<TraversalType o>
    Factory<o> traversal(const RootContainer& _roots, const order<o> = order<o>()) { return {_roots, N}; }
    template<TraversalType o>
    Factory<o> traversal(const RootIter& _roots, const order<o> = order<o>()) { return {_roots, N}; }


  };

}// namespace
