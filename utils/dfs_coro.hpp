
#pragma once


#include "dfs_common.hpp"

namespace PT {
  // ========== DFS ==========
  // This is a standard DFS, augmented by tons of functionality like
  // -postorder, inorder, preorder or any mix thereof (nodes may be output multiple times)
  // -edge traversals
  // -DLS (depth-last search) to guarantee that all parents of a node have been visited before it
  // -reversed search, starting at the bottom and going up
  // -forbidden nodes/edges that may not be visited

  // ------- DFS: helpers ---------
  // see dfs_common.hpp

  // ------- DFS: main class ---------
	// NOTE: SeenSet_ may be a pointer (or even void)
  // Roots_ may be a pointer if we use someone else's roots (the network f.ex.)
  template<TraversalType tt,
           StrictPhylogenyType Network_,
           NodeOrIterableType<mstd::TR_PtrOK> Roots_ = typename Network_::RootContainer,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>> // SeenSet_ may be void (unzip all retis)  or a pointer (shared SeenSet)
    requires (std::is_pointer_v<Roots_> or mstd::is_poppable<DFSRootStorage<Roots_>>)
  struct DFSIterator:
    public DFSInfo<Roots_, Forbidden_, SeenSet_>
  {
    // ------- static stuff --------
    using Info = DFSInfo<Roots_, Forbidden_, SeenSet_>;
    using Info::get_current_root;
    using Info::pop_root;
    using Info::roots_spent;
    using Info::is_forbidden;
    using Info::get_forbidden;
    using Info::mark_seen;

    using Roots = Roots_;
    using Network = Network_;
    using Forbidden = Forbidden_;
    using SeenSet = SeenSet_;

    static constexpr bool reverse = is_reverse_traversal(tt);
    using AdjContainer = std::conditional_t<reverse, typename Network_::PredContainer, typename Network_::SuccContainer>;
    using AdjIter = mstd::auto_iter<AdjContainer>;
    using Edge = typename Network::Edge;
    // on the state-stack, we store nodes along with their 
    using state_stack = std::vector<AdjIter>;

    using difference_type = ptrdiff_t;
    using value_type = std::conditional_t<is_node_traversal(tt), NodeDesc, Edge>;
    using pointer = mstd::self_deref<value_type>;
    using reference = value_type;
    using iterator_category = std::forward_iterator_tag;

    static constexpr bool has_seen = Info::has_seen;
    static constexpr bool has_forbidden = Info::has_forbidden;
    static constexpr bool has_forbidden_edges = mstd::is_testable<std::remove_pointer_t<Forbidden>, NodePair>;
    static constexpr bool has_forbidden_nodes = mstd::is_testable<std::remove_pointer_t<Forbidden>, NodeDesc>;

    static auto& get_adjacencies(const NodeDesc u) {
      if constexpr (reverse)
        return Network::predecessors(u);
      else return Network::successors(u);
    }
   
    template<class Adj>
    static Edge make_edge(const NodeDesc u, Adj&& v) {
      if constexpr (reverse)
        return Edge(reverse_edge_tag{}, u, std::forward<Adj>(v));
      else return Edge(u, std::forward<Adj>(v));
    }



    // ------- members --------
  protected:
    state_stack children;
    resume_info_t<is_inorder_traversal(tt)> resume_info;

    // ------- construction & desctruction ---------
  public:
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

    template<PhylogenyType Phylo, class... Args>
    DFSIterator(Phylo&& N, Args&&... args):
      Info(std::forward<Phylo>(N), std::forward<Args>(args)...)
    {
      children.reserve(N.num_nodes());
      advance();
    }

    template<NodeOrIterableType RootsInit, class... Args>
    DFSIterator(RootsInit&& _roots, Args&&... args): 
      Info(std::forward<RootsInit>(_roots), std::forward<Args>(args)...)
    { advance(); }


    // ------- operators --------
  public:
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
    auto operator->() const { return mstd::self_deref<value_type>(operator*()); }

    DFSIterator& operator++() { advance(); return *this; }
    DFSIterator operator++(int) { DFSIterator tmp = *this; advance(); return tmp; }

    bool operator!=(const DFSIterator& other) const { return !(*this == other); }
    bool operator==(const DFSIterator& other) const {
      if(is_invalid()) return other.is_invalid();
      if(other.is_invalid()) return false;
      if(resume_info.current_pos != other.resume_info.current_pos) return false;
      if(children.size() != other.children.size()) return false;
      return static_cast<const Info&>(*this) == static_cast<const Info&>(other);
    }


    // ------- methods: initialization --------
    // ------- methods: query --------
  protected:
    state_stack&& get_children() && { return std::move(children); }

  public:
    const state_stack& get_children() const & { return children; }

    // return the k'th node on the child stack (k=0 for the current node)
    NodeDesc get_kth_node_on_top(const uint32_t k) const {
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
      NodeDesc u = get_current_root();
      os << "status: ";
      for(const auto& succ: children) {
        if(succ.is_valid()) {
          NodeDesc v = *succ;
          os << (reverse ? NodePair{v, u} : NodePair{u, v}) << ' ';
          u = v;
        } else os << "(inv) ";
      }
      return os;
    }

    // some getters are necessary in order to construct non-owning iterators from owning iterators
    auto get_resume_info() const & { return resume_info; }
    
    // Note: if we're running a DLS, we'll have to make sure to insert u into the seen-map before calling is_seen()
    bool is_seen(const NodeDesc u) const {
      if constexpr (is_depth_last_traversal(tt)) {
        assert(mstd::test(Info::get_seen(), u));
        return Info::get_seen().at(u) != 0;
      } else return Info::is_seen(u);
    }

    // return a reason why we cannot visit the next node:
    //    0 = no reason, 1 = node is seen (or is not seen often enough if in DLS mode), 2 = node is forbidden, 3 = edge is forbidden
    int may_not_visit_next() const {
      const NodeDesc x = node_on_top();
      if(is_seen(x)) return 1;
      if constexpr (has_forbidden_nodes)
        if(is_forbidden(x)) return 2;
      if constexpr (has_forbidden_edges)
        if(is_forbidden(edge_on_top())) return 3;
      return 0;
    }
    bool may_visit_next() const { return may_not_visit_next() == 0; }


    // return whether we can produce an in-order output
    // NOTE: in particular, we cannot produce an in-order output if we're doing an edge-traversal and the child-stack is size-1
    //    this is becuase an in-order edge is formed from the grandparent to the parent of the current node
    bool can_make_inorder_output() const {
      // we'll need at least 2 neighborhoods on the stack to form an inorder edge (grandparent->parent)
      // we'll need at least 1 neighborhoods on the stack to form an inorder node (parent)
      return (children.size() >= 2 - is_node_traversal(tt));
    }

    bool is_invalid() const { return roots_spent(); }
    bool is_valid() const { return not is_invalid(); }
    

    // ------- methods: modification --------
    // The following function is the heart of the iterator.
    // It emulates a stateful co-routine (C++20 coroutines must be stateless) by laveraging computed labels,
    //    which I attribute to Miro Kneipp
    //
    // The function returns the point where to pick back up on the next call
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

    // for use with DLS: discount unvisited parents using the seen-map, return the current number of unvisited parents after discounting (as reference!)
    void discount_parents(const NodeDesc u) {
      if constexpr (has_seen) {
        const auto [iter, success] = mstd::append(Info::get_seen(), u, NoDegree);
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

    void visit_next() {
      const NodeDesc x = node_on_top();
      children.emplace_back(get_adjacencies(x));
      DEBUG6(std::cout << "adding children of "<<x<<" to the stack: "<<get_adjacencies(x) << "\n");
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
  };


  // ------- DFS: factories ---------
  template<TraversalType tt,
           StrictPhylogenyType Network_,
           NodeOrIterableType Roots_ = typename Network_::RootContainer,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  struct Traversal:
    public mstd::IterFactory<DFSIterator<tt, Network_, Roots_, Forbidden_, SeenSet_>>
  {
    using Iter = DFSIterator<tt, Network_, Roots_, Forbidden_, SeenSet_>;
    using Info = typename Iter::Info;
    using Parent = mstd::IterFactory<Iter>;
    using typename Info::Roots;
    using typename Info::Forbidden;
    using typename Info::SeenSet;

    using IndirectRoots = std::conditional_t<Info::roots_indirect or std::is_same_v<Roots_, NodeDesc>, Roots_, std::add_pointer_t<const Roots_>>;
    using IndirectForbidden = std::conditional_t<Info::has_forbidden, std::add_pointer_t<std::remove_pointer_t<Forbidden>>, void>;
    using IndirectSeen = std::conditional_t<Info::has_seen, std::add_pointer_t<std::remove_pointer_t<SeenSet>>, void>;
    using NonOwningInfo = DFSInfo<IndirectRoots, IndirectForbidden, IndirectSeen>;
    using NonOwningIter = DFSIterator<tt, Network_, IndirectRoots, IndirectForbidden, IndirectSeen>;
    using CopiedOwningIter = DFSIterator<tt, Network_, IndirectRoots, IndirectForbidden, SeenSet_>;
    using OwningIter = Iter;

    Traversal() = default;
    INHERIT_ALL_CONSTRUCTORS(Traversal, Parent)
    INHERIT_ASSIGNMENT(Traversal, Parent)

    auto begin() const&  { return CopiedOwningIter(static_cast<const Iter&>(*this)); }
    auto begin() & { return NonOwningIter(static_cast<Iter&>(*this)); }
    auto begin() && { return OwningIter(static_cast<Iter&&>(*this)); }
  };

#warning "TODO: make a 'robust traversal' with shared ownership of the seen- and forbidden set between the iterators and the traversal. Will need shared_ptr for that..."

  template<TraversalType tt,
           StrictPhylogenyType Network_,
           NodeOrIterableType Roots_ = typename Network_::RootContainer,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  using NodeTraversal = Traversal<tt, Network_, Roots_, Forbidden_, SeenSet_>;

  template<TraversalType tt,
           StrictPhylogenyType Network_,
           NodeOrIterableType Roots_ = typename Network_::RootContainer,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  using EdgeTraversal = Traversal<tt | edge_traversal, Network_, Roots_, Forbidden_, SeenSet_>;

  template<TraversalType tt,
           StrictPhylogenyType Network_,
           NodeOrIterableType Roots_ = typename Network_::RootContainer,
           class Forbidden_ = void,
           DFSSeenType SeenSet_ = DefaultSeenSet<Network_, tt>>
  using AllEdgesTraversal = Traversal<tt | all_edge_traversal, Network_, Roots_, Forbidden_, SeenSet_>;

  template<TraversalType tt,
           StrictPhylogenyType Network_,
           NodeOrIterableType Roots_ = typename Network_::RootContainer,
           class Forbidden_ = void,
           NodeMapType<mstd::TR_PtrVoidOK> SeenSet_ = NodeMap<Degree>>
  using AllEdgesDLSTraversal = Traversal<tt | depth_last_traversal, Network_, Roots_, Forbidden_, SeenSet_>;

  // ------- DFS: concepts ---------
  // ------- DFS: deduction guides ---------
  // ------- DFS: defaults ---------


}
