
#pragma once

#include "edge.hpp"
#include "set_interface.hpp"

namespace PT{
  // preorder: emit a node before all nodes below it
  // inorder: emit a node between each two consecutive subtrees below it (f.ex. node 0 with children 1, 2, 3 --> 1 0 2 0 3)
  // postorder: emit a node after all nodes below it
  // NOTE: these can be combined freely!
  enum TraversalType {preorder = 1, inorder = 2, postorder = 4, pre_and_in_order = 3, pre_and_post_order = 5, in_and_post_order = 6, all_order = 7};

  // this template can be used as sentinel to avoid the ugly "tree.template node_traversal<preoder>()" notation
  template<TraversalType> struct order{};
  // this fake wrapper can take "void" as template argument, in which case it acts as a shared_ptr<NodeContainer> with 0 runtime cost
  template<class NodeContainer> struct fake_wrapper;


  template<TraversalType o,
           class Traits,
           class _SeenSet = typename Traits::Network::NodeSet>
  class DFSIterator: public Traits
  {
  public:
    using Network = typename Traits::Network;
    using SeenSet = _SeenSet;
   
  protected:
    using ChildIter = typename Traits::child_iterator;
    using ChildIterPair = std::pair<ChildIter, ChildIter>;
    using Stack = std::deque<ChildIterPair>;
    using typename Traits::ItemContainerRef;
  
    static constexpr bool track_seen = !std::is_void_v<_SeenSet>;

    // NOTE: it may be tempting to replace the whole "seen"-mechanic by a filtering child iterator that filters-out all seen nodes,
    //       but in this case, all iterators need to carry their own reference to "seen" which may become a considerable overhead...
    Network& N;
    const Node root;
    
    // with the fake wrapper, we can disable node-tracking by setting _SeenSet = void
    fake_wrapper<_SeenSet> seen;

    // the history of descents in the network, for each descent, save the iterator of the current child and the end()-iterator
    Stack child_history;


    // dive deeper into the network, up to the next emittable node x, putting ranges on the stack (including that of x); return the current node
    // if we hit a node that is already seen, return that node
    void dive(const Node u)
    {
      assert(!track_seen || !seen.test(u)); // don't dive on seen nodes!

      ItemContainerRef&& children = Traits::get_children(N, u);
      child_history.emplace_back(children.begin(), children.end());

      // make sure we start with an unseen child
      skip_seen_children();
      
      // if a pre-order is requested, then u itself is emittable, so don't put more stuff on the stack; otherwise, keep diving to the first leaf
      if(!(o & preorder) && !current_node_finished())
        dive(Traits::get_node(*(child_history.back().first)));
    }

    // return whether all children of the current node have been treated
    bool current_node_finished() const
    {
      assert(!child_history.empty());
      return child_history.back().first == child_history.back().second;
    }

    // get the node whose child-iterators are on top of the stack
    Node node_on_top() const
    {
      // if there are at least 2 ranges on the stack, dereference the second to last to get the current node, otherwise, it's root
      return (child_history.size() > 1) ? Traits::get_node(*(child_history[child_history.size() - 2].first)) : root;
    }

    // when we're done treating all children, go back and continue with the parent
    void backtrack()
    {
      assert(current_node_finished());
      child_history.pop_back();
      // if we're not at the end, increment the parent iterator and proceed
      if(!child_history.empty()){
        assert(!current_node_finished());

        ChildIter& current_iter = child_history.back().first;
        // mark the node that we just finished treating as "seen"
        if(track_seen) seen.append(Traits::get_node(*current_iter));
        ++current_iter;

        // if now the child at current_iter has been seen, skip it (and all following)
        skip_seen_children();

        // if there are still unseen children then dive() into the next subtree unless inorder is requested
        if(!current_node_finished()){
          if(!(o & inorder)) dive(node_on_top());
        } else if(!(o & postorder)) backtrack(); // if the children are spent then keep popping end-iterators unless postorder is requested
      }
    }

    // skip over all seen children of the current node
    void skip_seen_children(){
      if(track_seen){
        ChildIterPair& current = child_history.back();
        // skip over all seen children
        while((current.first != current.second) && seen.test(Traits::get_node(*(current.first)))) ++(current.first);
      }
    }

  public:
    
    
    // construct with a given root node
    // NOTE: use this to construct an end-iterator
    DFSIterator(Network& _N): N(_N), root(0), seen() {}
    
    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSIterator(Network& _N, const Node _root, Args&&... args):
      N(_N), root(_root), seen(std::forward<Args>(args)...)
    { if(track_seen && !seen.test(root)) dive(root); }


    DFSIterator& operator++()
    {
      if(current_node_finished()){
        // there are no more children; this cannot happen unless postorder is requested
        assert(o & postorder);
        // since we're done with the node_on_top now, go backtrack()
        backtrack();
      } else {
        // the current node is not finished, so
        //  (b) either we're doing pre-order and we're currently diving
        //  (a) or we're doing inorder and we just returned from a subtree with backtrack()
        // in both cases, we continue with a dive()
        dive(node_on_top());
      }
      return *this;
    }
    DFSIterator operator++(int) { DFSIterator tmp(*this); ++(*this); return tmp; }

    // we restrict comparability to DFSIterators whose traversal type, network type and Stack type equal our own
    // NOTE: we roughly estimate that two DFSIterators are equal if they are both the end iterator or they have the same root, stack size and top node
    template<class __SeenSet>
    bool operator==(const DFSIterator<o, Traits, __SeenSet>& other) const
    {
      if(other.is_valid()){
        return is_valid() && (root == other.root) && (child_history.size() == other.child_history.size()) && (node_on_top() == other.node_on_top());
      } else return !is_valid();
    }
    template<class __SeenSet>
    bool operator!=(const DFSIterator<o, Traits, __SeenSet>& other) const { return !operator==(other); }


    inline bool is_valid() const { return !child_history.empty(); }

    template<TraversalType, class, class>
    friend class DFSIterator;

  };


  template<class _Network>
  struct NodeTraversalTraits
  {
    using Network = _Network;
    using difference_type = ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
 
    using ItemContainerRef = std::conditional_t<std::is_const_v<_Network>, typename Network::ConstSuccContainerRef, typename Network::SuccContainerRef>;
    using child_iterator   = std::iterator_of_t<ItemContainerRef>;
    using value_type = Node;
    using reference  = value_type;
    using pointer    = std::self_deref<value_type>;
   
    static ItemContainerRef get_children(Network& N, const Node u) { return N.children(u); }
    static Node get_node(const Node u) { return u; }
  };

  template<class _Network>
  struct EdgeTraversalTraits: public NodeTraversalTraits<_Network>
  {
    using Network = _Network;
    using ItemContainerRef = std::conditional_t<std::is_const_v<_Network>, typename Network::ConstOutEdgeContainerRef, typename Network::OutEdgeContainerRef>;
    using child_iterator   = std::iterator_of_t<ItemContainerRef>;
    using value_type = typename child_iterator::value_type;
    using reference  = value_type;
    using pointer    = std::self_deref<value_type>;
 
    static ItemContainerRef get_children(Network& N, const Node u) {
      ItemContainerRef result(N.out_edges(u));
      return result;
    }
    static Node get_node(const typename child_iterator::reference uv) { return uv.head(); }
  };


  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet>
  class DFSNodeIterator: public DFSIterator<o, NodeTraversalTraits<_Network>, _SeenSet>
  {
    using Parent = DFSIterator<o, NodeTraversalTraits<_Network>, _SeenSet>;
  public:
    using Parent::Parent;
    using Parent::node_on_top;
    using typename Parent::reference;
    using typename Parent::pointer;
    
    pointer operator->() const { return node_on_top(); }
    reference operator*() const { return node_on_top(); }
  };

  template<TraversalType o, class _Network, class _SeenSet = typename _Network::NodeSet>
  class DFSEdgeIterator: public DFSIterator<o, EdgeTraversalTraits<_Network>, _SeenSet>
  {
    using Parent = DFSIterator<o, EdgeTraversalTraits<_Network>, _SeenSet>;
  public:
    using Parent::Parent;
    using Parent::node_on_top;
    using typename Parent::Network;
    using typename Parent::reference;
    using typename Parent::pointer;
    using Parent::child_history;
    
    pointer operator->() const {
      assert(child_history.size() > 1);
      return *(child_history[child_history.size() - 2].first);
    }
    reference operator*() const { return *(operator->()); }

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSEdgeIterator(const Network& _N, const Node _root, Args&&... args):
      Parent(_N, _root, std::forward<Args>(args)...)
    { if(o & preorder) Parent::operator++(); }
  };


  struct node_traversal_tag {};
  struct edge_traversal_tag {};

  // this guy is our factory; begin()/end() can be called on it
  template<TraversalType o,
           class NodeEdge,
           class _Network,
           class _SeenSet = typename _Network::NodeSet>
  struct Traversal
  {
    _Network& N;
    Node root;

    fake_wrapper<_SeenSet> seen;

    static constexpr bool track_seen = !std::is_void_v<_SeenSet>;

    template<class __Network>
    using _Iterator = std::conditional_t<std::is_same_v<NodeEdge, node_traversal_tag>,
          DFSNodeIterator<o, __Network, _SeenSet>,
          DFSEdgeIterator<o, __Network, _SeenSet>>;

    using iterator = _Iterator<_Network>;
    using const_iterator = _Iterator<const _Network>;
    using value_type = typename std::my_iterator_traits<iterator>::value_type;
    using reference  = typename std::my_iterator_traits<iterator>::reference;
    using pointer    = typename std::my_iterator_traits<iterator>::pointer;
    using const_reference = typename std::my_iterator_traits<const_iterator>::reference;
    using const_pointer   = typename std::my_iterator_traits<const_iterator>::const_pointer;

    template<class... Args>
    Traversal(_Network& _N, const Node _root, Args&&... args):
      N(_N), root(_root), seen(std::forward<Args>(args)...) {}

    // if called without arguments, begin() constructs an iterator using the root and _SeenSet created by our constructor
    iterator begin() { return iterator(N, root, seen); }
    const_iterator begin() const { return const_iterator(N, root, seen); }
    // if called with one or more arguments, begin() constructs a new iterator whose _SeenSet is constructed using these arguments
    template<class First, class... Args>
    iterator begin(First&& first, Args&&... args) { return iterator(N, root, std::forward<First>(first), std::forward<Args>(args)...); };
    template<class First, class... Args>
    const_iterator begin(First&& first, Args&&... args) const { return iterator(N, root, std::forward<First>(first), std::forward<Args>(args)...); };

    iterator end() { return iterator(N); };
    const_iterator end() const { return const_iterator(N); };

    std::add_lvalue_reference<_SeenSet> seen_nodes() { return seen.get(); }
    std::add_lvalue_reference<const _SeenSet> seen_nodes() const { return seen.get(); }

    // so STL containers provide construction by a pair of iterators; unfortunately, it's not a "pair of iterators" but two separate iterators,
    // so we cannot have a function returning two seperate iterators :( thus, we'll just have to do it this way:
    template<class Container, class = std::enable_if<std::is_container_v<Container>>>
    operator Container() const { return {begin(), end()}; }
    template<class Container, class = std::enable_if<std::is_container_v<Container>>>
    operator Container() { return {begin(), end()}; }
  };

  // node-container wrapper that allows being void, incurring 0 (runtime) cost
  template<class SeenSet>
  struct fake_wrapper
  {
    std::shared_ptr<SeenSet> c;

    // if given a single argument, do copy/move construct, otherwise, instanciate a new _SeenSet with the arguments
    fake_wrapper() {}
    fake_wrapper(fake_wrapper&&) = default;
    fake_wrapper(const fake_wrapper&) = default;
    template<class First, class... Args, class = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<First>, fake_wrapper<SeenSet>>>>
    fake_wrapper(First&& first, Args&&... args): c(std::make_shared<SeenSet>(std::forward<First>(first), std::forward<Args>(args)...)) {}

    bool test(const Node u) const { return std::test(*c, u); }
    void append(const Node u) { std::append(*c, u); }
    SeenSet& get() { return c.get(); }
    const SeenSet& get() const { return c.get(); }
  };
  // fake version of the wrapper
  template<>
  struct fake_wrapper<void>
  {
    template<class... Args> fake_wrapper(Args&&... args) {} // construct however, fake_wrapper don't give a sh1t :)
    static bool test(const Node u) { return false; }
    static void append(const Node u) {}
    static void get() {};
  };

  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet>
  using NodeTraversal = Traversal<o, node_traversal_tag, _Network, _SeenSet>;
  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet>
  using EdgeTraversal = Traversal<o, edge_traversal_tag, _Network, _SeenSet>;


  // now this is the neat interface to the outer world - it can spawn any form of Traversal object and it's easy to interact with :)
  //NOTE: this is useless when const (but even const Networks can return a non-const MetaTraversal)
  template<class _Network,
           class _SeenSet,
           template<TraversalType, class, class> class _Traversal>
  class MetaTraversal
  {
  protected:
    _Network& N;
    fake_wrapper<_SeenSet> fake_seen;
  public:

    template<class... Args>
    MetaTraversal(_Network& _N, Args&&... args): N(_N), fake_seen(std::forward<Args>(args)...) {}

    // this one is the general form that can do any order, but is a little more difficult to use
    // NOTE: what's this weird order<o> at the end you ask? this allows you to let gcc infer the TraversalType if you so desire, so
    //    "traversal(u, order<inorder>())" and "template traversal<inorder>(u)" are the same thing, use whichever syntax you prefer :)
    // NOTE: we just pass fake_seen since it's basically a shared_ptr, which can be copied at almost no cost
    template<TraversalType o>
    _Traversal<o, _Network, _SeenSet> traversal(const Node u, const order<o>& = order<o>()) // Node u = N.root() saves 9 lines, but gcc whines about it...
    { return _Traversal<o, _Network, _SeenSet>(N, u, fake_seen); }
    template<TraversalType o>
    _Traversal<o, _Network, _SeenSet> traversal(const order<o>& = order<o>())
    { return _Traversal<o, _Network, _SeenSet>(N, N.root(), fake_seen); }

    // then, we add 3 predefined orders: preorder, inorder, postorder, that are easier to use
    // preorder:
    _Traversal<PT::preorder, _Network, _SeenSet> preorder(const Node u)
    { return _Traversal<PT::preorder, _Network, _SeenSet>(N, u, fake_seen); }
    _Traversal<PT::preorder, _Network, _SeenSet> preorder()
    { return _Traversal<PT::preorder, _Network, _SeenSet>(N, N.root(), fake_seen); }

    // inorder:
    _Traversal<PT::inorder, _Network, _SeenSet> inorder(const Node u)
    { return _Traversal<PT::inorder, _Network, _SeenSet>(N, u, fake_seen); }
    _Traversal<PT::inorder, _Network, _SeenSet> inorder()
    { return _Traversal<PT::inorder, _Network, _SeenSet>(N, N.root(), fake_seen); }

    // postorder:
    _Traversal<PT::postorder, _Network, _SeenSet> postorder(const Node u)
    { return _Traversal<PT::postorder, _Network, _SeenSet>(N, u, fake_seen); }
    _Traversal<PT::postorder, _Network, _SeenSet> postorder()
    { return _Traversal<PT::postorder, _Network, _SeenSet>(N, N.root(), fake_seen); }
  };





/*



  // a traversal is a kind of factory for the DFSIterator
  template<TraversalType o,
           class _Network,
           class SeenSet = typename _Network::NodeSet,
           template <class> Stack = std::vector>
  class Traversal: public std::iterator_traits<DFSIterator<o, _Network, SeenSet, Stack>>
  {
  public:
    using Edge = typename _Network::Edge;
  protected:
    const _Network& N;
    
  public:

    Traversal(_Network& _N): N(_N)
    {}

    void do_preorder() {do_preorder(N.root());}
    bool do_preorder(const Node u)
    {
      if(!track_seen || !test(seen, u)){
        if(track_seen) append(seen, u);
        emit_node(u);
        for(const auto& v: N.children(u)){
          emit_edge(u, v);
          do_preorder(v);
        }
        return true;
      } return false;
    }

    void do_postorder() {do_postorder(N.root());}
    bool do_postorder(const Node u)
    {
      if(!track_seen || !test(seen, u)){
        if(track_seen) append(seen, u);
        for(const auto& v: N.children(u)){
          do_postorder(v);
          emit_edge(u, v);
        }
        emit_node(u);
        return true;
      } return false;
    }

    void do_inorder() {do_inorder(N.root());}
    bool do_inorder(const Node u)
    {
      if(!track_seen || !test(seen, u)){
        if(track_seen) append(seen, u);
        
        bool emitted = false;
        for(const auto& v: N.children(u)){
          do_inorder(v);
          if(!emitted) {
            emit_node(u);
            emit_edge(u, v);
            emitted = true;
          }
        }
        if(!emitted) emit_node(u);
        return true;
      } return false;
    }


    _Container& do_traversal() {return do_traversal(N.root());}
    _Container& do_traversal(const Node u)
    {
      switch(o){
        case preorder: do_preorder(u); break;
        case inorder: do_inorder(u); break;
        case postorder: do_postorder(u); break;
        default: std::cerr<<"invalid traversal method: "<<o<<std::endl; exit(1);
      };
      return out;
    }
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  class NodeTraversal: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;
  protected:
    using Parent::out;

    void emit_node(const Node u) const { append(out, u); }
    void emit_edge(const Node u, const Node v) const {}
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  class EdgeTraversal: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;

  protected:
    using Parent::out;

    void emit_node(const Node u) const {}
    void emit_edge(const Node u, const Node v) const { append(out, u, v); }
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _ExceptContainer,
           class _Network>
  class NodeTraversalExcept: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;
  protected:
    using Parent::out;
    using Parent::seen;

    void emit_node(const Node u) const { append(out, u); }
    void emit_edge(const Node u, const Node v) const {}
  public:
    NodeTraversalExcept(_Network& _N, _Container& _out, const _ExceptContainer& _except):
      Parent(_N, _out)
    {
      seen.insert(_except.begin(), _except.end());
    }
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _ExceptContainer,
           class _Network>
  class EdgeTraversalExcept: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;

  protected:
    using Parent::out;
    using Parent::seen;
    const _ExceptContainer& except;

    void emit_node(const Node u) const {}
    void emit_edge(const Node u, const Node v) const { if(!test(except, v)) append(out, u, v); }
  public:
    EdgeTraversalExcept(_Network& _N, _Container& _out, const _ExceptContainer& _except):
      Parent(_N, _out),
      except(_except)
    {
      seen.insert(_except.begin(), _except.end());
    }
  };


  // convenience functions
  template<TraversalType o,
           class _Container,
           class _ExceptContainer,
           class _Network>
  _Container node_traversal(_Network& N, const _ExceptContainer& except, const Node u)
  {
    _Container out;
    return NodeTraversalExcept<o, true, _Container, _ExceptContainer, _Network>(N, out, except).do_traversal(u);
  }

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  _Container node_traversal(_Network& N, const Node u)
  {
    _Container out;
    return NodeTraversal<o, track_seen, _Container, _Network>(N, out).do_traversal(u);
  }

  template<TraversalType o,
           class _Container,
           class _ExceptContainer,
           class _Network>
  _Container edge_traversal(_Network& N, const _ExceptContainer& except, const Node u)
  {
    _Container out;
    return EdgeTraversalExcept<o, true, _Container, _ExceptContainer, _Network>(N, out, except).do_traversal(u);
  }

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  _Container edge_traversal(_Network& N, const Node u)
  {
    _Container out;
    return EdgeTraversal<o, track_seen, _Container, _Network>(N, out).do_traversal(u);
  }

*/


}// namespace
