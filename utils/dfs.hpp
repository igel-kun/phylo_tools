
#pragma once

#include "edge.hpp"
#include "set_interface.hpp"
#include "auto_iter.hpp"
#include "storage_adj_common.hpp"

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

  template<class _Network,
           class _ItemContainerRef,
           class _SeenSet = typename _Network::NodeSet>
  class _TraversalTraits: public std::my_iterator_traits<std::iterator_of_t<_ItemContainerRef>>
  {
  protected:
    // with the fake wrapper, we can disable node-tracking by setting _SeenSet = void
    fake_wrapper<_SeenSet> seen;
  public:
    using Network = _Network;
    using ItemContainerRef  = _ItemContainerRef;
    using child_iterator    = std::iterator_of_t<ItemContainerRef>;
    using iterator_category = std::forward_iterator_tag;

    static constexpr bool track_seen = !std::is_void_v<_SeenSet>;

    template<class... Args>
    _TraversalTraits(Args&&... args): seen(std::forward<Args>(args)...) {}
    
    inline bool is_seen(const Node u) const { return seen.test(u); }
  };


  template<class _Network, class _SeenSet = typename _Network::NodeSet>
  class NodeTraversalTraits: public _TraversalTraits<_Network, SuccContainer_of<_Network>, _SeenSet>
  {
    using Parent = _TraversalTraits<_Network, SuccContainer_of<_Network>, _SeenSet>;
  public:
    using Parent::Parent;
    using Parent::seen;
    using typename Parent::Network;
    using typename Parent::ItemContainerRef;
    // node traversals return fabricated rvalue nodes, no reason to fiddle with adjacency-data here, we're doing a Node traversal!
    using value_type = Node;
    using reference = Node;
    using const_reference = Node;
    using pointer = std::self_deref<Node>;
    using const_pointer = std::self_deref<Node>;

    // if there is only one node on the stack (f.ex. if we tried putting a leaf on it), consider it empty
    static constexpr unsigned char min_stacksize = 1;

    static ItemContainerRef get_children(Network& N, const Node u) { return N.children(u); }
    static Node get_node(const Node u) { return u; }
    
    inline void mark_seen(const Node u) { seen.append(u); }
  };

  template<class _Network, class _SeenSet = typename _Network::NodeSet>
  class EdgeTraversalTraits: public _TraversalTraits<_Network, OutEdgeContainer_of<_Network>, _SeenSet>
  {
    using Parent = _TraversalTraits<_Network, OutEdgeContainer_of<_Network>, _SeenSet>;
  public:
    using Parent::Parent;
    using Parent::seen;
    using typename Parent::Network;
    using typename Parent::ItemContainerRef;
    using typename Parent::const_reference;

    // an empty stack represents the end-iterator
    static constexpr unsigned char min_stacksize = 2;

    static ItemContainerRef get_children(Network& N, const Node u) {
      ItemContainerRef result(N.out_edges(u));
      return result;
    }
    static Node get_node(const_reference uv) { return uv.head(); }
   
    // normally, we want to skip an edge if its head has been seen
    //NOTE: this will give us an edge-list of a DFS-tree
    inline bool is_seen(const_reference uv) const { return seen.test(uv.head()); }
    inline void mark_seen(const_reference uv) { seen.append(uv.head()); }    
  };

  //NOTE: EdgeTraversalTraits gives us the edges of a dfs-tree, but the infrastructure can be used to compute all edges below a node (except some)
  //      For this, however, we'll need to differentiate between forbidden nodes and nodes discovered during the DFS, since the former should not
  //      occur as head of any emitted edge, while the latter should not occur as tail of any emitted edge! Thus, we'll need a second storage
  template<class _Network, class _ForbiddenSet = typename _Network::NodeSet, class _SeenSet = typename _Network::NodeSet>
  class AllEdgesTraits: public EdgeTraversalTraits<_Network, _ForbiddenSet>
  {
    using Parent = EdgeTraversalTraits<_Network, _ForbiddenSet>;
  protected:
    fake_wrapper<_SeenSet> seen_during_DFS;
  public:
    using Parent::Parent;
    using Parent::seen;
    using typename Parent::const_reference;

    // so now, we want to skip an edge if its head is forbidden or its tail has been seen during the DFS
    //NOTE: this will give us all edges below some node, except for those with forbidden heads
    inline bool is_seen(const_reference uv) const { return seen.test(uv.head()) || seen_during_DFS.test(uv.tail()); }
    // istead of appending to seen, we now append to seen_during_DFS
    inline void mark_seen(const_reference uv) { seen_during_DFS.append(uv.head()); }
  };




  template<TraversalType o,
           class Traits>
  class DFSIterator: public Traits
  {
  public:
    using Network = typename Traits::Network;
    using Traits::track_seen;
    using Traits::Traits;
   
  protected:
    using ChildIter = std::auto_iter<typename Traits::child_iterator>;
    using Stack = std::deque<ChildIter>;
    using typename Traits::ItemContainerRef;
    using Traits::get_children;
    using Traits::get_node;
    using Traits::min_stacksize;
    using Traits::mark_seen;
    using Traits::is_seen;
  
    // NOTE: it may be tempting to replace the whole "seen"-mechanic by a filtering child iterator that filters-out all seen nodes,
    //       but in this case, all iterators need to carry their own reference to "seen" which may become a considerable overhead...
    Network& N;
    const Node root;
    
    // the history of descents in the network, for each descent, save the iterator of the current child and the end()-iterator
    Stack child_history;


    // dive deeper into the network, up to the next emittable node x, putting ranges on the stack (including that of x); return the current node
    // if we hit a node that is already seen, return that node
    void dive(const Node u)
    {
      ItemContainerRef&& children = get_children(N, u);

      std::cout << "stack-appending children of "<< u <<": "<<children<<"\n";

      child_history.emplace_back(children.begin(), children.end());

      // make sure we start with an unseen child
      skip_seen_children();
      
      // if a pre-order is requested, then u itself is emittable, so don't put more stuff on the stack; otherwise, keep diving to the first leaf
      if(!(o & preorder) && !current_node_finished())
        dive(get_node(*(child_history.back())));
    }

    // return whether all children of the current node have been treated
    bool current_node_finished() const
    {
      assert(!child_history.empty());
      return child_history.back().is_invalid();
    }

    // get the node whose child-iterators are on top of the stack
    Node node_on_top() const
    {
      // if there are at least 2 ranges on the stack, dereference the second to last to get the current node, otherwise, it's root
      return (child_history.size() > 1) ? get_node(*(child_history[child_history.size() - 2])) : root;
    }

    // when we're done treating all children, go back and continue with the parent
    void backtrack()
    {
      assert(current_node_finished());
      std::cout << "removing last stack-element\n";
      child_history.pop_back();
      // if we're not at the end, increment the parent iterator and proceed
      if(!child_history.empty()){
        assert(!current_node_finished());

        ChildIter& current_child = child_history.back();
        // mark the node that we just finished treating as "seen"
        if(track_seen) mark_seen(*current_child);
        ++current_child;

        // if now the child at current_iter has been seen, skip it (and all following)
        skip_seen_children();

        // if there are still unseen children then dive() into the next subtree unless inorder is requested
        if(current_child.is_valid()){
          if(!(o & inorder)) dive(get_node(*(child_history.back())));
        } else if(!(o & postorder)) backtrack(); // if the children are spent then keep popping end-iterators unless postorder is requested
      }
    }

    // skip over all seen children of the current node
    void skip_seen_children()
    {
      if(track_seen){
        ChildIter& current = child_history.back();
        // skip over all seen children
        while(current.is_valid() && is_seen(*current)) {
          std::cout << "skipping "<<*current<<"\n";
          ++current;
        }
      }
    }

  public:
    
    
    // construct with a given root node
    // NOTE: use this to construct an end-iterator
    DFSIterator(Network& _N):
      Traits(nullptr),
      N(_N), root(0) {}
    
    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSIterator(Network& _N, const Node _root, Args&&... args):
      Traits(std::forward<Args>(args)...),
      N(_N), root(_root)
    { 
      std::cout << "making new non-end DFS iterator starting at "<<_root<<" (tracking? "<<track_seen<<")\n";
      std::cout << "root is seen? "<<is_seen(root)<<"\n";
      if(!track_seen || !is_seen(root)) dive(root);
    }


    DFSIterator& operator++()
    {
      std::cout << "++DFS, stack-size: "<<child_history.size()<<" top node: "<<node_on_top()<<" finished? "<<current_node_finished()<<"\n";
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
    template<class __Traits>
    bool operator==(const DFSIterator<o, __Traits>& other) const
    {
      if(other.is_valid()){
        return is_valid() && (root == other.root) && (child_history.size() == other.child_history.size()) && (node_on_top() == other.node_on_top());
      } else return !is_valid();
    }
    template<class __Traits>
    bool operator!=(const DFSIterator<o, __Traits>& other) const { return !operator==(other); }


    inline bool is_valid() const { return child_history.size() >= min_stacksize; }

    template<TraversalType, class>
    friend class DFSIterator;

  };



  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet>
  class DFSNodeIterator: public DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet>>
  {
    using Parent = DFSIterator<o, NodeTraversalTraits<_Network, _SeenSet>>;
  public:
    using Parent::Parent;
    using Parent::node_on_top;
    using typename Parent::reference;
    using typename Parent::pointer;

    pointer operator->() const { std::cout << "\nemitting ptr to node "<<node_on_top()<<"\n\n"; return node_on_top(); }
    reference operator*() const { std::cout << "\nemitting node "<< node_on_top()<<"\n\n"; return node_on_top(); }
  };

  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet,
           class _Traits = EdgeTraversalTraits<_Network, _SeenSet>>
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
    
    pointer operator->() const {
      assert(child_history.size() > 1);
      std::cout << "\nemitting edge "<<*(child_history[child_history.size() - 2])<<"\n\n";
      return *(child_history[child_history.size() - 2]);
    }
    reference operator*() const { return *(operator->()); }

    // construct with a given set of seen nodes (which has to correspond to our declared SeenSet), may be movable
    template<class... Args>
    DFSEdgeIterator(const Network& _N, const Node _root, Args&&... args):
      Parent(_N, _root, std::forward<Args>(args)...)
    {
      if(o & preorder) Parent::operator++();
    }
  };

  template<TraversalType o,
           class _Network,
           class _ForbiddenSet = typename _Network::NodeSet,
           class _SeenSet = typename _Network::NodeSet>
  using DFSAllEdgesIterator = DFSEdgeIterator<o, _Network, _SeenSet, AllEdgesTraits<_Network, _ForbiddenSet, _SeenSet>>;



  // template specizalization to select the right DFS Iterator using a tag
  struct node_traversal_tag {};
  struct edge_traversal_tag {};
  struct all_edges_traversal_tag {};

  template<TraversalType o, class NodeEdge, class _SeenSet>
  struct _choose_iterator
  { template<class _Network> using type = DFSNodeIterator<o, _Network, _SeenSet>; };
  
  template<TraversalType o, class _SeenSet>
  struct _choose_iterator<o, edge_traversal_tag, _SeenSet>
  { template<class _Network> using type = DFSEdgeIterator<o, _Network, _SeenSet>; };
  
  template<TraversalType o, class _SeenSet>
  struct _choose_iterator<o, all_edges_traversal_tag, _SeenSet>
  { template<class _Network> using type = DFSAllEdgesIterator<o, _Network, _SeenSet>; };

  // this guy is our factory; begin()/end() can be called on it
  //NOTE: Traversal has its own _SeenSet so that multiple calls to begin() can be given the same set of forbidden nodes
  //      Thus, you can either call begin() - which uses the Traversal's _SeenSet, or you call begin(bla, ...) to construct a new _SeenSet from bla, ...
  //NOTE: per default, the Traversal's SeenSet is used direcly by the DFS iterator, so don't call begin() twice with a non-empty SeenSet!
  //      if you want to reuse the same SeenSet for multiple DFS' you have the following options:
  //      (a) reset the SeenSet between calls to begin()
  //      (b) call begin(bla, ...) instead of begin()
  //      (c) create a Traversal with seen_deep_copy = true, which will deep-copy the SeenSet each time begin() is called
  template<TraversalType o,
           class NodeEdge,
           class _Network,
           class _SeenSet = typename _Network::NodeSet,
           bool _seen_deep_copy = false>
  struct Traversal
  {
    static constexpr bool track_seen = !std::is_void_v<_SeenSet>;
    static constexpr bool seen_deep_copy = _seen_deep_copy;

    template<class __Network>
    using _Iterator = typename _choose_iterator<o, NodeEdge, _SeenSet>::template type<__Network>;

    using iterator = _Iterator<_Network>;
    using const_iterator = _Iterator<const _Network>;
    using value_type = typename std::my_iterator_traits<iterator>::value_type;
    using reference  = typename std::my_iterator_traits<iterator>::reference;
    using pointer    = typename std::my_iterator_traits<iterator>::pointer;
    using const_reference = typename std::my_iterator_traits<const_iterator>::reference;
    using const_pointer   = typename std::my_iterator_traits<const_iterator>::const_pointer;

    _Network& N;
    Node root;
    fake_wrapper<_SeenSet> seen;



    template<class... Args>
    Traversal(_Network& _N, const Node _root, Args&&... args):
      N(_N), root(_root), seen(std::forward<Args>(args)...)
    {
      std::cout << "creating ";
      if(std::is_same_v<NodeEdge, node_traversal_tag>) std::cout << "node traversal"; else std::cout << "edge traversal";
      std::cout << " from node "<<root<<'\n';
    }

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

    // allow the user to play with the _SeenSet at all times
    //NOTE: this gives you the power to change the _SeenSet while the DFS is running, and with great power comes great responsibility ;] so be careful!
    std::add_lvalue_reference_t<_SeenSet> seen_nodes() { return seen.get(); }
    std::add_lvalue_reference_t<const _SeenSet> seen_nodes() const { return seen.get(); }

    // so STL containers provide construction by a pair of iterators; unfortunately, it's not a "pair of iterators" but two separate iterators,
    // so we cannot have a function returning two seperate iterators :( thus, we'll just have to do it this way:
    template<class Container, class = std::enable_if_t<std::is_container_v<Container>>>
    operator Container() const { std::cout << "const-casting to container\n"; return {begin(), end()}; }
    template<class Container, class = std::enable_if_t<std::is_container_v<Container>>>
    operator Container() { std::cout << "casting to container\n"; return {begin(), end()}; }
    template<class Container>
    Container& append_to(Container& c) { std::cout << "appending to container\n"; for(auto uv: *this) append(c, uv); return c; }
  };

  template<TraversalType o, class NodeEdge, class _Network, class _SeenSet>
  struct Traversal<o, NodeEdge, _Network, _SeenSet, true>: public Traversal<o, NodeEdge, _Network, _SeenSet, false>
  {
    using Parent = Traversal<o, NodeEdge, _Network, _SeenSet, false>;
    using Parent::Parent;
    using Parent::N;
    using Parent::root;
    using Parent::seen;
    using typename Parent::iterator;
    using typename Parent::const_iterator;
    static constexpr bool seen_deep_copy = true;

    // overwrite begin() methods to deep-copy seen
    iterator begin() { return iterator(N, root, seen.get()); }
    const_iterator begin() const { return const_iterator(N, root, seen.get()); }
  };


  // node-container wrapper that allows being void, incurring 0 (runtime) cost
  template<class SeenSet>
  struct fake_wrapper
  {
    std::shared_ptr<SeenSet> c;

    // if given no argument, construct a default SeenSet
    fake_wrapper(): c(std::make_shared<SeenSet>()) { std::cout << "set-wrapper\n";}
    // if given a single argument, do copy/move construct
    fake_wrapper(fake_wrapper&&) = default;
    fake_wrapper(const fake_wrapper&) = default;
    // if given nullptr_t, do not construct a SeenSet
    fake_wrapper(const std::nullptr_t) {}
    // if given one argument that's not a fake_wrapper<SeenSet>, or any 2 or more arguments, construct a new SeenSet from those arguments
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
    template<class... Args> fake_wrapper(Args&&... args) { std::cout << "void-wrapper\n"; } // construct however, fake_wrapper<void> don't give a sh1t :)
    static bool test(const Node u) { return false; }
    static void append(const Node u) {}
    static void get() {};
  };

  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet,
           bool seen_deep_copy = false>
  using NodeTraversal = Traversal<o, node_traversal_tag, _Network, _SeenSet>;
  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet,
           bool seen_deep_copy = false>
  using EdgeTraversal = Traversal<o, edge_traversal_tag, _Network, _SeenSet>;
  template<TraversalType o,
           class _Network,
           class _SeenSet = typename _Network::NodeSet,
           bool seen_deep_copy = false>
  using AllEdgesTraversal = Traversal<o, all_edges_traversal_tag, _Network, _SeenSet>;


  // now this is the neat interface to the outer world - it can spawn any form of Traversal object and it's easy to interact with :)
  //NOTE: this is useless when const (but even const Networks can return a non-const MetaTraversal)
  template<class _Network,
           class _SeenSet,
           template<TraversalType, class, class, bool> class _Traversal,
           bool _seen_deep_copy = false>
  class MetaTraversal
  {
  public:
    using Network = _Network;
    using SeenSet = _SeenSet;
    static constexpr bool seen_deep_copy = _seen_deep_copy;
  protected:
    _Network& N;
    fake_wrapper<_SeenSet> fake_seen;
  public:

    template<TraversalType o>
    using Traversal = _Traversal<o, _Network, _SeenSet, _seen_deep_copy>;

    template<class... Args>
    MetaTraversal(_Network& _N, Args&&... args): N(_N), fake_seen(std::forward<Args>(args)...) {}

    // copy construct from other MetaTraversal (using the same SeenSet)
    //NOTE: this will not copy the SeenSet, just create a new shared_ptr to the same SeenSet (that's why we force the MetaTravels to have the same SeenSet)
    //      the _seen_deep_copy flag only affects how the seen-set is transferred from the Traversal to the DFSIterator
    template<TraversalType, template<TraversalType, class, class, bool> class __Traversal, bool __seen_deep_copy>
    MetaTraversal(const MetaTraversal<_Network, _SeenSet, __Traversal, __seen_deep_copy>& mt): N(mt.N), fake_seen(mt.fake_seen) {}

    // this one is the general form that can do any order, but is a little more difficult to use
    // NOTE: what's this weird order<o> at the end you ask? this allows you to let gcc infer the TraversalType if you so desire, so
    //    "traversal(u, order<inorder>())" and "template traversal<inorder>(u)" are the same thing, use whichever syntax you prefer :)
    // NOTE: we just pass fake_seen since it's basically a shared_ptr, which can be copied at almost no cost
    template<TraversalType o>
    Traversal<o> traversal(const Node u, const order<o>& = order<o>()) { return Traversal<o>(N, u, fake_seen); }
    template<TraversalType o>
    Traversal<o> traversal(const order<o>& = order<o>()) { return Traversal<o>(N, N.root(), fake_seen); }

    // then, we add 3 predefined orders: preorder, inorder, postorder, that are easier to use
    // preorder:
    Traversal<PT::preorder> preorder(const Node u) { return traversal<PT::preorder>(u); }
    Traversal<PT::preorder> preorder() { return preorder(N.root()); }

    // inorder:
    Traversal<PT::inorder> inorder(const Node u) { return traversal<PT::inorder>(u); }
    Traversal<PT::inorder> inorder() { return inorder(N.root()); }

    // postorder:
    Traversal<PT::postorder> postorder(const Node u) { return traversal<PT::postorder>(u); }
    Traversal<PT::postorder> postorder() { return postorder(N.root()); }
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
