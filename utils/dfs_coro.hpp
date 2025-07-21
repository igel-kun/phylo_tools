
#pragma once

#include "stl_utils.hpp"
#include "optional_tuple.hpp"

#include "iter_factory.hpp"

#ifdef DFSCORO
namespace PT
#else
namespace PTx
#endif
{

  using TraversalType = uint8_t;

  constexpr TraversalType preorder = 0x01; // preorder traversal will output a node before any of its descendants
  constexpr TraversalType postorder = 0x02; // postorder traversal will output a node after all its descendants
  constexpr TraversalType inorder = 0x04; // inorder traversal will output a node before *EVERY* child except the first and if there is no child
  constexpr TraversalType pre_and_post_order = preorder + postorder;
  constexpr TraversalType reverse_traversal = 0x10;
  constexpr TraversalType edge_traversal = 0x20; // an edge traversal produces the edges of a DFS-tree
  constexpr TraversalType all_edge_traversal = 0x40; // an all-edge traversal produces all edges of the network
  constexpr TraversalType depth_last_traversal = 0x80; // a depth-last traversal is only allowed to process a node when all parents have been processed

  constexpr bool is_preorder_traversal(const TraversalType tt) { return tt & preorder; }
  constexpr bool is_postorder_traversal(const TraversalType tt) { return tt & postorder; }
  constexpr bool is_inorder_traversal(const TraversalType tt) { return tt & inorder; }

  constexpr bool is_edge_traversal(const TraversalType tt) { return tt & edge_traversal; }
  constexpr bool is_all_edge_traversal(const TraversalType tt) { return tt & all_edge_traversal; }
  constexpr bool is_depth_last_traversal(const TraversalType tt) { return tt & depth_last_traversal; }
  constexpr bool is_reverse_traversal(const TraversalType tt) { return tt & reverse_traversal; }
  constexpr bool is_node_traversal(const TraversalType tt) { return not (is_edge_traversal(tt)) and (not is_all_edge_traversal(tt)); }

  constexpr auto& spell_out_traversal(const TraversalType tt, auto& os) {
    if(tt & reverse_traversal) os << "reverse ";
    if(tt & edge_traversal) os << "edge ";
    if(tt & all_edge_traversal) os << "all-edge ";
    if(is_node_traversal(tt)) os << "node ";
    if(tt & depth_last_traversal) os << "depth-last ";
    if(tt & preorder) os << "preorder ";
    if(tt & postorder) os << "postorder ";
    if(tt & inorder) os << "inorder ";
    return os;
  }

  // the seen set may be a set of nodes or a map indexed by nodes
  template<class S, mstd::TypeRune rune = mstd::TR_PtrVoidOK>
  concept DFSSeenType = PT::NodeSetType<S, rune> or PT::NodeMapType<S, rune>;

	// the default set of nodes to track is void for trees
	template<class T> struct _DefaultSeenSet {};
	template<class T> struct _DefaultSeenMap {};
  
	template<PT::NodeType Node>
	struct _DefaultSeenSet<Node> { using type = std::conditional_t<PT::TreeNodeType<Node>, void, PT::NodeSet>; };
	template<PT::StrictPhylogenyType Network>
	struct _DefaultSeenSet<Network>: public _DefaultSeenSet<typename Network::Node> {};

  template<PT::NodeType Node>
	struct _DefaultSeenMap<Node> { using type = std::conditional_t<PT::TreeNodeType<Node>, void, PT::NodeMap<PT::Degree>>; };
	template<PT::StrictPhylogenyType Network>
	struct _DefaultSeenMap<Network>: public _DefaultSeenMap<typename Network::Node> {};

	template<class T, bool is_map = false> requires (PT::PhylogenyType<T> || PT::NodeType<T>)
	using DefaultSeenSet = std::conditional_t<is_map, typename _DefaultSeenMap<T>::type, typename _DefaultSeenSet<T>::type>;

  // the root storage is either a non-owning reverse auto_iter if we don't own the roots, or a poppable root container
  template<class Roots>  struct ProtoDFSRootStorage {};
  template<mstd::IterableType<mstd::TR_Strict> Roots>  struct ProtoDFSRootStorage<Roots*> { using type = mstd::IterFactory<mstd::RBeginType<Roots>>; };
  template<mstd::IterableType<mstd::TR_Strict> Roots>  struct ProtoDFSRootStorage<Roots> { using type = Roots; };
  template<> struct ProtoDFSRootStorage<PT::NodeDesc> { using type = PT::NodeSingleton; };
  template<> struct ProtoDFSRootStorage<PT::NodeDesc*> { using type = PT::NodeSingleton; };
  
  template<PT::NodeOrIterableType<mstd::TR_PtrOK> Roots>
  using DFSRootStorage = typename ProtoDFSRootStorage<Roots>::type;

  // store roots, forbidden and seen
  template<PT::NodeOrIterableType<mstd::TR_PtrOK> _Roots,
           class _Forbidden,
           DFSSeenType _SeenSet>
    requires (std::is_pointer_v<_Roots> || mstd::is_poppable<DFSRootStorage<_Roots>>)
  struct DFSInfo: 
    public mstd::optional_tuple<_Forbidden, _SeenSet>
  {
    using Roots = DFSRootStorage<_Roots>;
    static_assert(mstd::IterableType<Roots>);
    static_assert(not std::is_pointer_v<_Roots> || mstd::is_derived_from_template_v<Roots, mstd::_auto_iter>);
    using Forbidden = _Forbidden;
    using SeenSet = _SeenSet;
    using Parent = mstd::optional_tuple<Forbidden, SeenSet>;

    static constexpr bool has_forbidden = not std::is_void_v<Forbidden>;
    static constexpr bool has_seen = not std::is_void_v<_SeenSet>;

    static constexpr bool roots_indirect = std::is_pointer_v<_Roots>;
    static constexpr bool forbidden_indirect = std::is_pointer_v<Forbidden>;
    static constexpr bool seen_indirect = std::is_pointer_v<SeenSet>;

    DFSRootStorage<_Roots> roots;

    template<PT::StrictPhylogenyType Phylo, class... Args>
    DFSInfo(const Phylo& N, Args&&... args):
      Parent{std::forward<Args>(args)...},
      roots(N.roots())
    {}

    template<PT::NodeOrIterableType RootsInit, class... Args>
    DFSInfo(RootsInit&& _roots, Args&&... args):
      Parent{std::forward<Args>(args)...},
      roots(std::forward<RootsInit>(_roots))
    {}

    DFSInfo() = default;

    // initialization of indirections from containers is already implemented in mstd::optional_tuple,
    // and we'll forbid initializing containers from indirections (for now)
    template<PT::NodeOrIterableType<mstd::TR_PtrOK> Other_Roots,
            class Other_Forbidden,
            DFSSeenType Other_SeenSet>
      requires (not std::is_same_v<DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<Other_Roots>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) or (forbidden_indirect >= std::is_pointer_v<Other_Forbidden>)) and
          ((not has_seen) or (seen_indirect >= std::is_pointer_v<Other_SeenSet>)))
    DFSInfo(const DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>& other):
      Parent(static_cast<const typename DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>::Parent&>(other)),
      roots(other.roots)
    {}
    template<PT::NodeOrIterableType<mstd::TR_PtrOK> Other_Roots,
            class Other_Forbidden,
            DFSSeenType Other_SeenSet>
      requires (not std::is_same_v<DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<Other_Roots>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) or (forbidden_indirect >= std::is_pointer_v<Other_Forbidden>)) and
          ((not has_seen) or (seen_indirect >= std::is_pointer_v<Other_SeenSet>)))
    DFSInfo(DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>& other):
      Parent(static_cast<typename DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>::Parent&>(other)),
      roots(other.roots)
    {}

    template<PT::NodeOrIterableType<mstd::TR_PtrOK> Other_Roots,
            class Other_Forbidden,
            DFSSeenType Other_SeenSet>
      requires (not std::is_same_v<DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>, DFSInfo> and
          (roots_indirect >= std::is_pointer_v<Other_Roots>) and // do not allow initializing the root container from a root-indirection!
          ((not has_forbidden) || (forbidden_indirect >= std::is_pointer_v<Other_Forbidden>)) and
          ((not has_seen) || (seen_indirect >= std::is_pointer_v<Other_SeenSet>)))
    DFSInfo(DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>&& other):
      Parent(static_cast<typename DFSInfo<Other_Roots, Other_Forbidden, Other_SeenSet>::Parent&&>(other)),
      roots(std::move(other.roots))
    {}

    const auto& get_roots() const { return roots; }
    const auto& get_forbidden() const requires (has_forbidden) { return mstd::access(this->template get<0>()); }
    auto& get_forbidden() requires (has_forbidden) { return mstd::access(this->template get<0>()); }
    const auto& get_seen() const requires (has_seen) { return mstd::access(this->template get<1>()); }

  protected:
    auto& get_seen() requires (has_seen) { return mstd::access(this->template get<1>()); }
    auto& get_roots() { return roots; }

    template<class... Args>
    void mark_seen(const PT::NodeDesc u, Args&&... args) { 
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

  // helper structure for resuming the iteration; it has to be the same for all template-instanciations, so it needs to be outside the class
  template<bool doing_inorder_traversal>
  struct resume_info_t {
    uint8_t current_pos = 0; // this contains the point at which we want to resume iteration
    bool operator!=(const resume_info_t& other) const = default; 
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


	// NOTE: _SeenSet may be a reference (or even void)
  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer, // _Roots may be a pointer if we use someone else's roots (the network f.ex.)
           class _Forbidden = void,
           DFSSeenType _SeenSet = DefaultSeenSet<_Network>> // _SeenSet may be void (unzip all retis)  or a pointer (shared SeenSet)
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
    static constexpr bool has_forbidden_edges = mstd::is_testable<std::remove_pointer_t<Forbidden>, PT::NodePair>;
    static constexpr bool has_forbidden_nodes = mstd::is_testable<std::remove_pointer_t<Forbidden>, PT::NodeDesc>;

    // on the state-stack, we store nodes along with their 
    using state_stack = std::vector<AdjIter>;

  protected:
    state_stack children;
    resume_info_t<is_inorder_traversal(tt)> resume_info;

  public:
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

    // return the k'th node on the child stack (k=0 for the current node)
    PT::NodeDesc get_kth_node_on_top(const uint32_t k) const {
      assert(not roots_spent());
      assert(children.size() >= k);
      if(children.size() > k) {
        const auto& iter = children[children.size() - k - 1];
        assert(iter.is_valid());
        return iter->get_desc();
      } else return get_current_root();
    }
    // return the k'th edge on the child stack (k=0 for the edge to the current node)
    auto get_kth_edge_on_top(const uint32_t k) const {
      assert(not roots_spent());
      assert(children.size() >= k + 1);
      const NodeDesc x = get_kth_node_on_top(k + 1);
      const auto y_iter = children[children.size() - k - 1];
      assert(y_iter.is_valid());
      return make_edge(x, *y_iter);
    }

    auto node_on_top() const { return get_kth_node_on_top(0); }
    auto tail_on_top() const { return get_kth_node_on_top(1); }

    auto edge_on_top() const { return get_kth_edge_on_top(0); }
    auto second_edge_on_top() const { return get_kth_edge_on_top(1); }

    bool top_is_invalid() const { return children.back().is_invalid(); }

    auto& status(auto& os) const {
      PT::NodeDesc u = get_current_root();
      os << "status: ";
      for(const auto& succ: children) {
        if(succ.is_valid()) {
          PT::NodeDesc v = *succ;
          os << (reverse ? PT::NodePair{v, u} : PT::NodePair{u, v}) << ' ';
          u = v;
        } else os << "(inv) ";
      }
      return os;
    }

    // some getters are necessary in order to construct non-owning iterators from owning iterators
    auto get_resume_info() const & { return resume_info; }
    
    const state_stack& get_children() const & { return children; }

  protected:
    state_stack&& get_children() && { return std::move(children); }

    // for use with DLS: discount unvisited parents using the seen-map, return the current number of unvisited parents after discounting (as reference!)
    void discount_parents(const PT::NodeDesc u) {
      if constexpr (has_seen) {
        const auto [iter, success] = mstd::append(Info::get_seen(), u, PT::NoDegree);
        auto& num_parents = iter->second;
        if(success) {
          num_parents = Network::in_degree(u);
          DEBUG6(std::cout << "newly inserted parent-map entry "<<*iter<<'\n');
        }
        --num_parents;
        DEBUG6(std::cout << "discounting parents of "<<u<<" to " << num_parents << '\n');
        DEBUG6(std::cout << "parent-map now: " << Info::get_seen() << '\n');
      }
    }

    // Note: if we're running a DLS, we'll have to make sure to insert u into the seen-map before calling is_seen()
    bool is_seen(const PT::NodeDesc u) const {
      if constexpr (is_depth_last_traversal(tt)) {
        assert(mstd::test(Info::get_seen(), u));
        return Info::get_seen().at(u) != 0;
      } else return Info::is_seen(u);
    }

    // return a reason why we cannot visit the next node:
    //    0 = no reason, 1 = node is seen (or is not seen often enough if in DLS mode), 2 = node is forbidden, 3 = edge is forbidden
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
      DEBUG6(std::cout << "adding children of "<<x<<" to the stack: "<<get_adjacencies(x)<<'\n');
    }

    // return whether we can produce an in-order output
    // NOTE: in particular, we cannot produce an in-order output if we're doing an edge-traversal and the child-stack is size-1
    //    this is becuase an in-order edge is formed from the grandparent to the parent of the current node
    bool can_make_inorder_output() const {
      // we'll need at least 2 neighborhoods on the stack to form an inorder edge (grandparent->parent)
      // we'll need at least 1 neighborhoods on the stack to form an inorder node (parent)
      return (children.size() >= 2 - is_node_traversal(tt));
    }

    // prepare to descent into a child node and return whether the parent should be output before
    bool prepare_inorder_descent() {
      auto& num_sc = resume_info.num_successful_children;
      assert(not num_sc.empty());
      uint8_t& current_num_sc = mstd::back(num_sc);
      const bool result = (current_num_sc != 0); // if we have not dived successfully into a child node, then don't output the parent
      ++current_num_sc;
      append(num_sc, 0); // for the child node that we are about to dive into, prepare an entry in the table
      return result;
    }

    // return the point where to pick back up on the next call
    uint8_t iterate(const uint8_t start_jump) {
      static constexpr void* jump_table[] = {&&outer_loop, &&resume_roots, // 0, 1
        &&resume_descending_all_edge, &&resume_descending, &&resume_ascending, // 2, 3, 4
        &&resume_ascending_inorder, &&resume_descending_inorder, &&resume_outer}; // 5, 6, 7
      DEBUG6(std::cout << "resuming at index "<<static_cast<int>(start_jump)<<'\n');
      goto* jump_table[start_jump];
outer_loop: // while(1) {
        assert(children.empty());
        
        // step 1: get the next root and put its adjacency on the stack
root_loop:
        if(roots_spent()) return 0;
        // when doing DLS, remember to insert the root so 'may_visit_next' can find it (but roots have no indegree)
        if constexpr (is_depth_last_traversal(tt) and has_seen) mstd::append(Info::get_seen(), node_on_top(), 0);
        if(may_not_visit_next()) {
          pop_root();
          goto root_loop;
        } else {
          // only the pre-order node-traversal must output the root first
          if constexpr (is_node_traversal(tt) && is_preorder_traversal(tt)) return 1; // resume at &&resume_roots
resume_roots:
          if constexpr (is_inorder_traversal(tt)) append(resume_info.num_successful_children, 0);
          visit_next();
        }
        // step 2: go as deep as possible, yielding nodes/edges if in postorder
descending_loop: // while(1) {
          DEBUG6(status(std::cout) << '\n');
          // if the node_on_top has no more children, then take its adjacency iterator off the stack and go up to the parent
          if(top_is_invalid()) {
            // if we're in in-order mode and we are about to ascend, then output the parent iff we've had <2 successful children
            if constexpr (is_inorder_traversal(tt)) {
              if(mstd::value_pop_back(resume_info.num_successful_children) < 2)
                if(can_make_inorder_output()) return 5; // resume at &&resume_ascending_inorder
            }
resume_ascending_inorder:
            children.pop_back();
            // step 3: go up 1 step and descend from the next child-node
            if(not children.empty()) {
              assert(may_visit_next());
              if constexpr (is_postorder_traversal(tt)) return 4; // resume at &&resume_ascending
resume_ascending:
              // we're done with the current child node, so mark this one and advance to the next
              if constexpr (not is_depth_last_traversal(tt)) mark_seen(*(children.back()));
            } else goto descending_loop_end; // if there are no more nodes on the stack, then we're done with this root
          } else {
            if constexpr (is_depth_last_traversal(tt)) discount_parents(node_on_top());
            switch(may_not_visit_next()) {
              case 0: // we actually may visit node_on_top
                if constexpr (is_preorder_traversal(tt)) return 3; // resume at &&resume_descending
                goto resume_descending;
              case 1: // node_on_top is seen
                if constexpr (is_all_edge_traversal(tt)) return 2; // resume at &&resume_descending_all_edge
              default: break;
            }
          }
resume_descending_all_edge:
          // if we are done with the current child node (either we treated it or we aren't allowed to visit it), then advance the adjacency
          ++(children.back());
          goto descending_loop;
resume_descending:
          // before visiting the next node: if we are in in-order mode, then output the parent, unless this is the first child
          if constexpr (is_inorder_traversal(tt)) {
            if(prepare_inorder_descent())
              if(can_make_inorder_output()) return 6; // resume at &&dresume_descending_inorder
          }
resume_descending_inorder:
          visit_next(); // if we may visit the next node on the stack, then go ahead
          goto descending_loop;
//      } // end descending loop
descending_loop_end:
        // finally, output the root if we're in node-postorder
        if constexpr (is_node_traversal(tt) and is_postorder_traversal(tt)) return 7; // resume at &&resume_outer
resume_outer:
        mark_seen(get_current_root());
        pop_root();
//    } // while(not roots.empty())
      goto outer_loop;
    } // iterate function

  public:
    void advance() {
      DEBUG6(std::cout << "advancing a "; spell_out_traversal(tt, std::cout)<<'\n');
      resume_info.current_pos = iterate(resume_info.current_pos);
    }

    DFSIterator() = default;

    template<TraversalType other_tt,
           class OtherNetwork,
           class OtherRoots,
           class OtherForbidden,
           class OtherSeenSet>
      requires (not mstd::is_same_v<DFSIterator<other_tt, OtherNetwork, OtherRoots, OtherForbidden, OtherSeenSet>, DFSIterator>)
    DFSIterator(const DFSIterator<other_tt, OtherNetwork, OtherRoots, OtherForbidden, OtherSeenSet>& other): 
      Info(static_cast<const DFSInfo<OtherRoots, OtherForbidden, OtherSeenSet>&>(other)),
      children(other.get_children()),
      resume_info(other.get_resume_info())
    {}

    template<TraversalType other_tt,
           class OtherNetwork,
           class OtherRoots,
           class OtherForbidden,
           class OtherSeenSet>
      requires (not mstd::is_same_v<DFSIterator<other_tt, OtherNetwork, OtherRoots, OtherForbidden, OtherSeenSet>, DFSIterator>)
    DFSIterator(DFSIterator<other_tt, OtherNetwork, OtherRoots, OtherForbidden, OtherSeenSet>& other): 
      Info(static_cast<DFSInfo<OtherRoots, OtherForbidden, OtherSeenSet>&>(other)),
      children(other.get_children()),
      resume_info(other.get_resume_info())
    {}

    template<TraversalType other_tt,
           class OtherNetwork,
           class OtherRoots,
           class OtherForbidden,
           class OtherSeenSet>
      requires (not mstd::is_same_v<DFSIterator<other_tt, OtherNetwork, OtherRoots, OtherForbidden, OtherSeenSet>, DFSIterator>)
    DFSIterator(DFSIterator<other_tt, OtherNetwork, OtherRoots, OtherForbidden, OtherSeenSet>&& other): 
      Info(static_cast<DFSInfo<OtherRoots, OtherForbidden, OtherSeenSet>&&>(other)),
      children(std::move(other.get_children())),
      resume_info(other.get_resume_info())
    {}

    template<PT::PhylogenyType Phylo, class... Args>
    DFSIterator(Phylo&& N, Args&&... args):
      Info(std::forward<Phylo>(N), std::forward<Args>(args)...)
    {
      children.reserve(N.num_nodes());
      advance();
    }

    template<PT::NodeOrIterableType RootsInit, class... Args>
    DFSIterator(RootsInit&& _roots, Args&&... args): 
      Info(std::forward<RootsInit>(_roots), std::forward<Args>(args)...)
    { advance(); }


    bool is_invalid() const { return roots_spent(); }
    bool is_valid() const { return not is_invalid(); }
    
    value_type operator*() const {
      if constexpr (is_node_traversal(tt))
        if constexpr (is_inorder_traversal(tt)) {
          return tail_on_top();
        } else return node_on_top();
      else {
        if constexpr (is_inorder_traversal(tt)) {
          return second_edge_on_top();
        } else return edge_on_top();
      }
    }

    DFSIterator& operator++() { advance(); return *this; }
    DFSIterator operator++(int) { DFSIterator tmp = *this; advance(); return tmp; }

    bool operator!=(const DFSIterator& other) const { return !(*this == other); }
    bool operator==(const DFSIterator& other) const {
      if(is_invalid()) return other.invalid();
      if(other.invalid()) return false;
      if(resume_info != other.resume_info) return false;
      if(children.size() != other.children.size()) return false;
      return static_cast<const Info&>(*this) == static_cast<const Info&>(other);
    }
  };


  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer,
           class _Forbidden = void,
           DFSSeenType _SeenSet = DefaultSeenSet<_Network, is_depth_last_traversal(tt)>>
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
    using OwningIter = Iter;

    Traversal() = default;
    INHERIT_ALL_CONSTRUCTORS(Traversal, Parent)
    INHERIT_ASSIGNMENT(Traversal, Parent)

    // note: can only draw an iterator from a const traversal if we have no SeenSet
    auto begin() const& requires (std::is_void_v<_SeenSet>) { return NonOwningIter(static_cast<const Iter&>(*this)); }
    auto begin() & { return NonOwningIter(static_cast<Iter&>(*this)); }
    auto begin() && { return OwningIter(static_cast<Iter&&>(*this)); }
  };

#warning "TODO: make a 'robust traversal' with shared ownership of the seen- and forbidden set between the iterators and the traversal. Will need shared_ptr for that..."


  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer,
           class _Forbidden = void,
           DFSSeenType _SeenSet = DefaultSeenSet<_Network>>
  using NodeTraversal = Traversal<tt, _Network, _Roots, _Forbidden, _SeenSet>;

  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer,
           class _Forbidden = void,
           DFSSeenType _SeenSet = DefaultSeenSet<_Network>>
  using EdgeTraversal = Traversal<tt | edge_traversal, _Network, _Roots, _Forbidden, _SeenSet>;

  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer,
           class _Forbidden = void,
           DFSSeenType _SeenSet = DefaultSeenSet<_Network>>
  using AllEdgesTraversal = Traversal<tt | all_edge_traversal, _Network, _Roots, _Forbidden, _SeenSet>;

  template<TraversalType tt,
           PT::StrictPhylogenyType _Network,
           PT::NodeOrIterableType _Roots = typename _Network::RootContainer,
           class _Forbidden = void,
           PT::NodeMapType<mstd::TR_PtrVoidOK> _SeenSet = PT::NodeMap<PT::Degree>>
  using AllEdgesDLSTraversal = Traversal<tt | depth_last_traversal, _Network, _Roots, _Forbidden, _SeenSet>;

}
