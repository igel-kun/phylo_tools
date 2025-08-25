
#pragma once

#include "optional_tuple.hpp"
#include "set_interface.hpp"
#include "predicates.hpp"

#include "types.hpp"
#include "tags.hpp"
#include "dfs_common.hpp"

namespace PT {


	// NOTE: SeenSet_ may be a reference (or even void)
  template<class Forbidden_, DFSSeenType SeenSet_>
  struct DFSSupportSets:
    public mstd::optional_tuple<mstd::NoRef<Forbidden_>, mstd::NoRef<SeenSet_>>
  {
#warning "make it possible to forbid edges instead of vertices"
    static constexpr bool has_forbidden = !std::is_void_v<Forbidden_>;
    static constexpr bool has_seen = !std::is_void_v<SeenSet_>;
    static constexpr bool track_nodes = has_seen || has_forbidden;

    // NOTE: if SeenSet_ is a reference, we replace it with a pointer in order to not lose assignment
    using Forbidden = mstd::NoRef<Forbidden_>; //mstd::FirstNonVoid<mstd::NoRef<Forbidden_>, mstd::monostate>;
    using SeenSet = mstd::NoRef<SeenSet_>; //mstd::FirstNonVoid<mstd::NoRef<SeenSet_>, mstd::monostate>;
    using Parent = mstd::optional_tuple<Forbidden, SeenSet>;
    
    DFSSupportSets() = default;
    INHERIT_ALL_CONSTRUCTORS(DFSSupportSets, Parent);
    INHERIT_ASSIGNMENT(DFSSupportSets, Parent);

    // we'll give references to our seen set and the forbidden predicate to each sub-iterator
    using SeenSetRef = std::conditional_t<has_seen, std::add_pointer_t<mstd::remove_pointer_t<SeenSet>>, void>;
    // NOTE: we're adding a const here, since the forbidden set is not supposed to be changed by any sub-iterator
    using ForbiddenRef = std::conditional_t<has_forbidden, std::add_pointer_t<std::add_const_t<mstd::remove_pointer_t<Forbidden>>>, void>;

    /*
    // NOTE: I tried leaving this as an aggregate, but GCC wouldn't let me:
    //  "error: initializer for mstd::monostate must be brace-enclosed"... great

    [[ no_unique_address ]] Forbidden forbidden;
    [[ no_unique_address ]] SeenSet seen;

    template<class First, class... Args> requires (not mstd::is_same_v<First, DFSSupportSets> and not has_forbidden and has_seen)
    DFSSupportSets(First&& first, Args&&... args):
      seen(std::forward<First>(first), std::forward<Args>(args)...)
    {}

    template<class First, class... Args> requires (not mstd::is_same_v<First, DFSSupportSets> and has_forbidden and not has_seen)
    DFSSupportSets(First&& first, Args&&... args):
      forbidden(std::forward<First>(first), std::forward<Args>(args)...)
    {}

    template<class First, class... Args> requires (not mstd::is_same_v<First, DFSSupportSets> and has_forbidden and has_seen)
    DFSSupportSets(First&& first, Args&&... args):
      forbidden(std::forward<First>(first)),
      seen(std::forward<Args>(args)...)
    {}
    */


    bool is_forbidden(const NodeDesc u) const {
      if constexpr (has_forbidden) {
        return mstd::test(mstd::access(this->template get<0>()), u);
      } else return false;
    }
/*    bool is_forbidden(const NodePair uv) const {
      if constexpr (has_forbidden) {
        if constexpr (mstd::is_testable<Forbidden_, NodePair>)
          return mstd::test(mstd::access(this->template get<0>()), uv);
        else if constexpr (reverse) {
          return is_forbidden(uv.first);
        } else return is_forbidden(uv.second);
      } else return false;
    }
*/
    // we consider a node 'seen' if it's either seen or forbidden
    bool is_seen(const NodeDesc u) const {
      bool result = is_forbidden(u);
      if constexpr (has_seen) {
        //std::cout << "checking seen("<<u<<") from seenset "<<this->template get<1>()<<'\n';
        result |= mstd::test(mstd::access(this->template get<1>()), u);
        //std::cout << "result: "<<result<<'\n';
      }
      return result;
    }
    void mark_seen(const NodeDesc u) {
      if constexpr (has_seen)
        mstd::append(mstd::access(this->template get<1>()), u);
    }
  };

  template<StrictPhylogenyType Network_, bool reverse>
  using NextNodeContainer = std::conditional_t<reverse, typename Network_::ParentContainer, typename Network_::ChildContainer>;

  template<TraversalType tt,
           StrictPhylogenyType Network_,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  struct TraversalTraits:
    public DFSSupportSets<Forbidden_, SeenSet_>,
    public mstd::iterator_traits<mstd::iterator_of_t<NextNodeContainer<Network_, is_reverse_traversal(tt)>>>
  {
    using Parent = DFSSupportSets<Forbidden_, SeenSet_>;
    using IterTraits = mstd::iterator_traits<mstd::iterator_of_t<NextNodeContainer<Network_, is_reverse_traversal(tt)>>>;
    using ItemContainer  = NextNodeContainer<Network_, is_reverse_traversal(tt)>;
    using Network = Network_;
    using child_iterator  = mstd::auto_iter<mstd::iterator_of_t<ItemContainer>>;
    using iterator_category = std::forward_iterator_tag;

    TraversalTraits() = default;
    INHERIT_ALL_CONSTRUCTORS(TraversalTraits, Parent);
    INHERIT_ASSIGNMENT(TraversalTraits, Parent);
  };


  template<TraversalType tt,
           StrictPhylogenyType Network_,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  struct NodeTraversalTraits:
    public TraversalTraits<tt, Network_, Forbidden_, SeenSet_>
  {
    static constexpr bool reverse = is_reverse_traversal(tt);
    using Parent = TraversalTraits<tt, Network_, Forbidden_, SeenSet_>;
    using typename Parent::IterTraits;
    using typename Parent::Network;
    using typename Parent::child_iterator;
    using value_type      = const NodeDesc;
    using reference       = value_type;
    using const_reference = reference;
    using pointer         = mstd::pointer_from_reference<reference>;
    using const_pointer   = mstd::pointer_from_reference<const_reference>;

    NodeTraversalTraits() = default;
    INHERIT_ALL_CONSTRUCTORS(NodeTraversalTraits, Parent);
    INHERIT_ASSIGNMENT(NodeTraversalTraits, Parent);

    // if there is only one node on the stack (f.ex. if we tried putting a leaf on it), consider it empty
    static constexpr unsigned char min_stacksize = 1;

    static constexpr auto get_next_items(const NodeDesc u) {
      if constexpr (reverse)
        return child_iterator(Network::parents(u));
      else return child_iterator(Network::children(u));
    }
    static constexpr size_t num_next_items(const NodeDesc u) {
      return reverse ? Network::in_degree(u) : Network::out_degree(u);
    }
    static constexpr NodeDesc get_node(const NodeDesc u) { return u; }
  };

  template<StrictPhylogenyType Network_, bool reverse>
  using NextEdgeContainer = std::conditional_t<reverse, typename Network_::InEdgeContainer, typename Network_::OutEdgeContainer>;


  template<TraversalType tt,
           StrictPhylogenyType Network_,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  struct EdgeTraversalTraits:
    public TraversalTraits<tt, Network_, Forbidden_, SeenSet_>
  {
    static constexpr bool reverse = is_reverse_traversal(tt);
    using Parent = TraversalTraits<tt, Network_, Forbidden_, SeenSet_>;
    using EdgeContainer = NextEdgeContainer<Network_, reverse>;
    using EdgeIter = mstd::iterator_of_t<EdgeContainer>;
    using EdgeIterTraits = mstd::iterator_traits<EdgeIter>;
    // NOTE: the DFS traversal stack will hold auto-iters for iterators into Network_::(Out)EdgeContainer (which is an IterFactory)
    //       such iterators construct edges from the child/parent-adjacencies on the fly when they are de-referenced (rvalues instead of lvalue references).
    //       Note that these edges **DO NOT EXIST IN MEMORY** (only on the return-stack), so we also return rvalues here.
    using typename Parent::Network;
    using typename Parent::child_iterator;
    using value_type      = typename EdgeIterTraits::value_type;
    using reference       = typename EdgeIterTraits::reference;
    using const_reference = typename EdgeIterTraits::const_reference;
    using pointer         = typename EdgeIterTraits::pointer;
    using const_pointer   = typename EdgeIterTraits::const_pointer;
    using Adjacency = typename Network_::Adjacency;
    using Parent::mark_seen;
    using Parent::is_seen;

    EdgeTraversalTraits() = default;
    INHERIT_ALL_CONSTRUCTORS(EdgeTraversalTraits, Parent);
    INHERIT_ASSIGNMENT(EdgeTraversalTraits, Parent);


    // an empty stack represents the end-iterator
    static constexpr unsigned char min_stacksize = 2;

    // NOTE: out_edges returns a temporary iterator factory, so we cannot return a reference to it!
    static constexpr auto get_next_items(const NodeDesc u) {
      if constexpr (reverse)
        return child_iterator(Network::in_edges(u));
      else return child_iterator(Network::out_edges(u));
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
  template<TraversalType tt,
           StrictPhylogenyType Network_,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  struct AllEdgesTraits:
    public EdgeTraversalTraits<tt, Network_, Forbidden_, SeenSet_>
  {
    static constexpr bool reverse = is_reverse_traversal(tt);
    using Parent = EdgeTraversalTraits<tt, Network_, Forbidden_, SeenSet_>;
    using typename Parent::Network;
    using typename Parent::value_type;
    using typename Parent::child_iterator;
    using Parent::mark_seen;
    using Parent::is_seen;

    AllEdgesTraits() = default;
    INHERIT_ALL_CONSTRUCTORS(AllEdgesTraits, Parent);
    INHERIT_ASSIGNMENT(AllEdgesTraits, Parent);

    // if u has been seen, just return an empty (Out)EdgeContainer (because all of u's out-edges will be skipped anyways)
    child_iterator get_next_items(const NodeDesc u) const {
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
  concept TraversalTraitsType = requires (T t) { typename T::child_iterator; typename T::IterTraits; typename T::ItemContainer; };


}
