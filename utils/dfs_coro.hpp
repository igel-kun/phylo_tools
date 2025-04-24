
#include "iter_factory.hpp"

namespace PTx {

  using TraversalType = uint8_t;

  constexpr TraversalType preorder = 0x01;
  constexpr TraversalType postorder = 0x02;
  constexpr TraversalType pre_and_post_order = preorder + postorder;
  constexpr TraversalType reverse_traversal = 0x10;
  constexpr TraversalType edge_traversal = 0x20; // an edge traversal produces the edges of a DFS-tree
  constexpr TraversalType all_edge_traversal = 0x40; // an all-edge traversal produces all edges of the network

  constexpr bool is_preorder_traversal(const TraversalType tt) { return tt & preorder; }
  constexpr bool is_postorder_traversal(const TraversalType tt) { return tt & postorder; }

  constexpr bool is_edge_traversal(const TraversalType tt) { return tt & edge_traversal; }
  constexpr bool is_all_edge_traversal(const TraversalType tt) { return tt & all_edge_traversal; }
  constexpr bool is_reverse_traversal(const TraversalType tt) { return tt & reverse_traversal; }
  constexpr bool is_node_traversal(const TraversalType tt) { return !is_edge_traversal(tt) && !is_all_edge_traversal(tt); }

	// the default set of nodes to track is void for trees
	template<class T> struct _DefaultSeenSet {};
  
	template<PT::NodeType Node>
	struct _DefaultSeenSet<Node> { using type = std::conditional_t<PT::TreeNodeType<Node>, void, PT::NodeSet>; };
	template<PT::StrictPhylogenyType Network>
	struct _DefaultSeenSet<Network> { using type = std::conditional_t<PT::TreeNodeType<typename Network::Node>, void, PT::NodeSet>; };
	template<class T> requires (PT::PhylogenyType<T> || PT::NodeType<T>)
	using DefaultSeenSet = typename _DefaultSeenSet<T>::type;

  // the root storage is either a non-owning reverse auto_iter if we don't own the roots, or a poppable root container
  template<class Roots>  struct ProtoDFSRootStorage {};
  template<mstd::IterableType<mstd::TR_Strict> Roots>  struct ProtoDFSRootStorage<Roots*> { using type = mstd::IterFactory<mstd::RBeginType<Roots>>; };
  template<mstd::IterableType<mstd::TR_Strict> Roots>  struct ProtoDFSRootStorage<Roots> { using type = Roots; };
  template<class Roots> requires mstd::is_same_v<std::remove_pointer_t<Roots>, PT::NodeDesc> 
  struct ProtoDFSRootStorage<Roots> { using type = PT::NodeSingleton; };
  
  template<PT::NodeOrIterableType<mstd::TR_PtrOK> Roots>
  using DFSRootStorage = typename ProtoDFSRootStorage<Roots>::type;

  // store roots, forbidden and seen
  template<PT::NodeOrIterableType<mstd::TR_PtrOK> _Roots,
           class _Forbidden,
           PT::NodeSetType<mstd::TR_PtrVoidOK> _SeenSet>
    requires (std::is_pointer_v<_Roots> || mstd::is_poppable<DFSRootStorage<_Roots>>)
  struct DFSInfo: 
    public mstd::optional_tuple<DFSRootStorage<_Roots>, _Forbidden, _SeenSet>
  {
    using Roots = DFSRootStorage<_Roots>;
    static_assert(mstd::IterableType<Roots>);
    static_assert(not std::is_pointer_v<_Roots> || mstd::is_derived_from_template_v<Roots, mstd::_auto_iter>);
    using Forbidden = _Forbidden;
    using SeenSet = _SeenSet;
    using Parent = mstd::optional_tuple<Roots, Forbidden, SeenSet>;

    static constexpr bool has_forbidden = not std::is_void_v<Forbidden>;
    static constexpr bool has_seen = not std::is_void_v<_SeenSet>;

    static constexpr bool roots_indirect = std::is_pointer_v<_Roots>;
    static constexpr bool forbidden_indirect = std::is_pointer_v<Forbidden>;
    static constexpr bool seen_indirect = std::is_pointer_v<SeenSet>;

    template<PT::StrictPhylogenyType Phylo>
    DFSInfo(const Phylo& N):
      Parent{N.roots()}
    {}

    DFSInfo() = default;
    INHERIT_ALL_CONSTRUCTORS(DFSInfo, Parent);

    // initialization of indirections from containers is already implemented in mstd::optional_tuple,
    // and we'll forbid initializing containers from indirections (for now)
    template<PT::NodeOrIterableType<mstd::TR_PtrOK> Other_Roots,
            class Other_Forbidden,
            PT::NodeSetType<mstd::TR_PtrVoidOK> Other_SeenSet>
      requires (not std::is_same_v<DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<Other_Roots>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) || (forbidden_indirect >= std::is_pointer_v<Other_Forbidden>)) and
          ((not has_seen) || (seen_indirect >= std::is_pointer_v<Other_SeenSet>)))
    DFSInfo(const DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>& other):
      Parent(other)
    {}
    template<PT::NodeOrIterableType<mstd::TR_PtrOK> Other_Roots,
            class Other_Forbidden,
            PT::NodeSetType<mstd::TR_PtrVoidOK> Other_SeenSet>
      requires (not std::is_same_v<DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<Other_Roots>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) || (forbidden_indirect >= std::is_pointer_v<Other_Forbidden>)) and
          ((not has_seen) || (seen_indirect >= std::is_pointer_v<Other_SeenSet>)))
    DFSInfo(DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>&& other):
      Parent(std::move(other))
    {}

    const auto& get_roots() const { return this->template get<0>(); }
    const auto& get_forbidden() const requires (has_forbidden) { return mstd::access(this->template get<1>()); }
    auto& get_forbidden() requires (has_forbidden) { return mstd::access(this->template get<1>()); }
    const auto& get_seen() const requires (has_seen) { return mstd::access(this->template get<2>()); }

  protected:
    auto& get_seen() requires (has_seen) { return mstd::access(this->template get<2>()); }
    auto& get_roots() { return this->template get<0>(); }

    void mark_seen(const PT::NodeDesc u) { 
      if constexpr (has_seen) {
        mstd::append(get_seen(), u);
        DEBUG6(std::cout << "seen: "<< get_seen() << '\n');
      }
    }

    void pop_root() {
      if constexpr (roots_indirect) // if our roots are indirect, then we have an auto_iter onto the container, so advance that one
        ++get_roots();
      else mstd::pop_back(get_roots()); // if our roots are direct, then pop the last root
    }

  public:

    bool is_forbidden(const auto x) const {
      if constexpr (has_forbidden) {
        return mstd::test(get_forbidden(), x);
      } else return false;
    }
    
    bool is_seen(const PT::NodeDesc u) const {
      if constexpr (has_seen) {
        return mstd::test(get_seen(), u);
      } else return false;
    }

    auto get_current_root() const {
      if constexpr (roots_indirect)
        return *get_roots();
      else return mstd::back(get_roots());
    }
    bool roots_spent() const {
      if constexpr (roots_indirect)
        return get_roots().is_invalid();
      else return get_roots().empty();
    }
  };

	// NOTE: _SeenSet may be a reference (or even void)
  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer, // _Roots may be a pointer if we use someone else's roots (the network f.ex.)
           class _Forbidden = void,
           PT::NodeSetType<mstd::TR_PtrVoidOK> _SeenSet = DefaultSeenSet<_Network>> // _SeenSet may be void (unzip all retis)  or a pointer (shared SeenSet)
    requires (std::is_pointer_v<_Roots> || mstd::is_poppable<DFSRootStorage<_Roots>>)
  struct DFSIterator:
    public DFSInfo<_Roots, _Forbidden, _SeenSet>
  {
    using Info = DFSInfo<_Roots, _Forbidden, _SeenSet>;
    using Info::get_current_root;
    using Info::pop_root;
    using Info::roots_spent;
    using Info::is_forbidden;
    using Info::get_forbidden;
    using Info::is_seen;
    using Info::mark_seen;

    using Roots = _Roots;
    using Network = _Network;
    using Forbidden = _Forbidden;
    using SeenSet = _SeenSet;

    static constexpr bool reverse = is_reverse_traversal(tt);
    using AdjContainer = std::conditional_t<reverse, typename _Network::PredContainer, typename _Network::SuccContainer>;
    using AdjIter = mstd::auto_iter<AdjContainer>;
    using Edge = typename Network::Edge;

    using difference_type = ptrdiff_t;
    using value_type = std::conditional_t<is_node_traversal(tt), PT::NodeDesc, Edge>;
    using pointer = mstd::self_deref<value_type>;
    using reference = value_type;
    using iterator_category = std::forward_iterator_tag;

    static constexpr bool has_seen = Info::has_seen;
    static constexpr bool has_forbidden = Info::has_forbidden;
    static constexpr bool has_forbidden_edges = mstd::is_testable<std::remove_pointer_t<_Forbidden>, PT::NodePair>;
    static constexpr bool has_forbidden_nodes = mstd::is_testable<std::remove_pointer_t<_Forbidden>, PT::NodeDesc>;

    // on the state-stack, we store nodes along with their 
    using state_stack = std::vector<AdjIter>;

  protected:
    state_stack children;
    uint8_t current_resume_pos = 0;

  public:

    PT::NodeDesc node_on_top() const {
      assert(not roots_spent());
      if(not children.empty()) {
        assert(children.back().is_valid());
        return *(children.back());
      } else return get_current_root();
    }

    PT::NodeDesc tail_on_top() const {
      assert(not children.empty());
      return (children.size() == 1) ? get_current_root() : children[children.size() - 2]->get_desc();
    }

    PT::NodePair edge_on_top() const {
      return make_edge(tail_on_top(), *(children.back()));
    }

    bool top_is_inv() const { return children.back().is_invalid(); }

    auto& status(auto& os) const {
      PT::NodeDesc u = get_current_root();
      for(const auto& succ: children) {
        if(succ.is_valid()) {
          PT::NodeDesc v = *succ;
          os << (reverse ? PT::NodePair{v, u} : PT::NodePair{u, v}) << ' ';
          u = v;
        } else os << "(inv) ";
      }
      return os;
    }
  
  protected:
    static auto& get_adjacencies(const PT::NodeDesc u) {
      if constexpr (reverse)
        return Network::predecessors(u);
      else return Network::successors(u);
    }
   
    template<class Adj>
    static Edge make_edge(const PT::NodeDesc u, Adj&& v) {
      if constexpr (reverse)
        return Edge(PT::reverse_edge_tag{}, u, std::forward<Adj>(v));
      else return Edge(u, std::forward<Adj>(v));
    }

    // return a reason why we cannot visit the next node, 0 = no reason, 1 = node is seen, 2 = node is forbidden, 3 = edge is forbidden
    int may_not_visit_next() const {
      const PT::NodeDesc x = node_on_top();
      if(is_seen(x)) return 1;
      if constexpr (has_forbidden_nodes)
        if(is_forbidden(x)) return 2;
      if constexpr (has_forbidden_edges)
        if(is_forbidden(edge_on_top())) return 3;
      return 0;
    }
    bool may_visit_next() const { return may_not_visit_next() == 0; }

    void visit_next() {
      const PT::NodeDesc x = node_on_top();
      children.emplace_back(get_adjacencies(x));
      DEBUG6(std::cout << "adding neighbors of "<<x<<" to the stack: "<<get_adjacencies(x)<<'\n');
    }

    // return the point where to pick back up on the next call
    uint8_t iterate(const uint8_t start_jump) {
      static constexpr void* jump_table[] = {&&outer_loop, &&resume_roots,
        &&resume_descending_all_edge, &&resume_descending, &&resume_ascending, &&resume_outer};
      DEBUG6(std::cout << "resuming at index "<<static_cast<int>(start_jump)<<'\n');
      goto* jump_table[start_jump];
outer_loop: // while(1) {
        assert(children.empty());
        
        // step 1: get the next root and put its adjacency on the stack
root_loop: // while(1) {
          if(roots_spent()) return 0;
          if(may_visit_next()) {
            // only the pre-order node-traversal must output the root first
            if constexpr (is_node_traversal(tt) && is_preorder_traversal(tt)) return 1; // resume at &&resume_roots
resume_roots:
            visit_next();
            goto inner_loop;
          } else pop_root();
        goto root_loop; //}
inner_loop: // while(1) {
          // step 2: go as deep as possible, yielding nodes/edges if in postorder
descending_loop: // while(1) {
            DEBUG6(status(std::cout) << '\n');
            // if we encounter a leaf, then take its (empty) adjacencies off the stack
            if(top_is_inv()) {
              children.pop_back();
              goto ascending_loop;
            } else {
              switch(may_not_visit_next()) {
                case 0:
                  if constexpr (is_preorder_traversal(tt)) return 3;
                  goto resume_descending;
                case 1:
                  if constexpr (is_all_edge_traversal(tt)) return 2;
                default: break;
              }
resume_descending_all_edge:
              // if we aren't allowed to go to the next node, then advance the adjacency until we are
              ++(children.back());
              goto descending_loop;
            }
            if constexpr (is_preorder_traversal(tt)) return 3;
resume_descending:
            visit_next(); // if we may visit the next node on the stack, then go ahead
//          }
          goto descending_loop;

          // step 3: go up again until we find an adjacency that we can advance in order to go deep again
ascending_loop: //  while(1) {
            if(children.empty()) goto inner_loop_end;
            assert(may_visit_next());
            if constexpr (is_postorder_traversal(tt)) return 4;
resume_ascending:
            mark_seen(*(children.back()));
            ++(children.back());
            if(top_is_inv()) goto ascending_end;
            switch(may_not_visit_next()) {
              case 0: goto ascending_end;
              case 1:
                  if constexpr (is_all_edge_traversal(tt)) return 4;
              default: break;
            }
            children.pop_back();
//          }
          goto ascending_loop;
ascending_end:
          if(children.empty()) goto inner_loop_end;
        goto inner_loop;
inner_loop_end:
//        }
        if constexpr (is_node_traversal(tt) && is_postorder_traversal(tt)) return 5;
resume_outer:
        mark_seen(get_current_root());
        pop_root();
      goto outer_loop;
//      } // while(not roots.empty())
    } // iterate function

  public:
    void advance() { current_resume_pos = iterate(current_resume_pos); }

    DFSIterator() = default;

    template<class... Args>
    DFSIterator(Args&&... args): 
      Info(std::forward<Args>(args)...)
    { advance(); }

    bool is_invalid() const { return roots_spent(); }
    bool is_valid() const { return not is_invalid(); }
    
    value_type operator*() const {
      if constexpr (is_node_traversal(tt))
        return node_on_top();
      else return edge_on_top();
    }

    DFSIterator& operator++() { advance(); return *this; }
    DFSIterator operator++(int) { DFSIterator tmp = *this; advance(); return tmp; }

    bool operator!=(const DFSIterator& other) const { return !(*this == other); }
    bool operator==(const DFSIterator& other) const {
      if(is_invalid()) return other.invalid();
      if(other.invalid()) return false;
      if(current_resume_pos != other.current_resume_pos) return false;
      if(children.size() != other.children.size()) return false;
      return static_cast<const Info&>(*this) == static_cast<const Info&>(other);
    }
  };


  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer,
           class _Forbidden = void,
           PT::NodeSetType<mstd::TR_PtrVoidOK> _SeenSet = DefaultSeenSet<_Network>>
  struct Traversal:
    public mstd::IterFactory<DFSIterator<tt, _Network, _Roots, _Forbidden, _SeenSet>>
  {
    using Iter = DFSIterator<tt, _Network, _Roots, _Forbidden, _SeenSet>;
    using Info = typename Iter::Info;
    using Parent = mstd::IterFactory<Iter>;
    using typename Info::Roots;
    using typename Info::Forbidden;
    using typename Info::SeenSet;

    using IndirectRoots = std::conditional_t<Info::roots_indirect, Roots, std::add_pointer_t<Roots>>;
    using IndirectForbidden = std::conditional_t<Info::has_forbidden, std::add_pointer_t<std::remove_pointer_t<Forbidden>>, void>;
    using IndirectSeen = std::conditional_t<Info::has_seen, std::add_pointer_t<std::remove_pointer_t<SeenSet>>, void>;
    using NonOwningInfo = DFSInfo<IndirectRoots, IndirectForbidden, IndirectSeen>;
    using NonOwningIter = DFSIterator<tt, _Network, IndirectRoots, IndirectForbidden, IndirectSeen>;

    Traversal() = default;
    INHERIT_ALL_CONSTRUCTORS(Traversal, Parent)
    INHERIT_ASSIGNMENT(Traversal, Parent)

    auto begin() const& { return NonOwningIter(static_cast<const Iter&>(*this)); }
    auto begin() & { return NonOwningIter(static_cast<Iter&>(*this)); }
    auto begin() && { return static_cast<Iter&&>(*this); }
  };

#warning "TODO: make a 'robust traversal' whose DFS-iterators survive the destruction of the traversal. Will need shared_ptr for that..."

}
