
#pragma once

#include "types.hpp"
#include "iter_factory.hpp"

namespace PT {

  template<StrictPhylogenyType Net>
  struct Switching {
    using ParentContainer = typename Net::ParentContainer;
    using ParentIter = mstd::iterator_of_t<ParentContainer>;

    std::vector<NodeWith<ParentIter>> active_parent;

    bool is_invalid() const { return active_parent.empty(); }
    bool is_valid() const { return not is_invalid(); }

    // NOTE: the active_edges are guaranteed in post-order of the switching (the tree containing all switched-on edges)
    template<class Out = NetEdgeVec<Net>, NodeIterableType Nodes, class RetiParentSelector>
    Out get_active_edges(Nodes&& leaves, RetiParentSelector parent_select) {
      Out out;
      NodeSet seen;
      if constexpr (mstd::is_iterable_with_size<Nodes>)
        seen.reserve(2*leaves.size());

      size_t retis_seen = 0;
      for(NodeDesc v: leaves) {
        DEBUG6(std::cout << "next node: "<< v << "\n");
        while(append(seen, v).second and LIKELY(Net::in_degree(v) > 0)) {          
          ParentIter vp;
          if(Net::in_degree(v) > 1) {
            assert(retis_seen <= active_parent.size());
            if(retis_seen == active_parent.size()) {
              // if it's the first time we encounter v on the path, then add a fresh parent-auto-iter to the stack and move along it
              // NOTE: this is important as the same procedure is used for initializing active_parent itself!
              vp = parent_select(v);
              append(active_parent, v, vp);
              DEBUG6(std::cout << "added "<<v<<" with it's first parent "<<*vp<<" to active_parent["<<retis_seen<<"]\n");
            } else vp = active_parent[retis_seen].second;
            ++retis_seen;
          } else vp = Net::parents(v).begin();
          // move along the current ParentIter, filling out
          if constexpr (mstd::AppendableR<Out, mstd::TR_RefOK, NodeDesc, decltype(*vp)>) {
            append(out, reverse_edge_tag{}, v, *vp);
          } else append(out, *vp);
          v = *vp;
        }
      }
      return out;
    }
    // by default, just select the first parent for each reticulation
    template<class Out = NetEdgeVec<Net>, NodeIterableType Nodes>
    Out get_active_edges(Nodes&& leaves) { return get_active_edges(std::forward<Nodes>(leaves), [](const NodeDesc r){ return Net::parents(r).begin(); }); }

    bool operator==(const Switching& other) { return active_parent == other.active_parent; }
  };

  // this is an iterator for switchings of a network
  template<StrictPhylogenyType Net, NodeContainerType<mstd::TR_PtrVoidOK> _Leaves = void, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  struct SwitchingIter:
    public mstd::iter_traits_from_reference<OutputVec>
  {
    using Traits = mstd::iter_traits_from_reference<OutputVec>;
    using EdgeVec = NetEdgeVec<Net>;
    using AdjVec = NetAdjVec<Net>;
    static constexpr bool has_leaves = not std::is_void_v<_Leaves>;
    using Leaves = std::conditional_t<has_leaves, _Leaves, mstd::monostate>;
    using NetPtr = std::conditional_t<has_leaves, mstd::monostate, const Net*>;
    using typename Traits::value_type;
    using typename Traits::pointer;

  protected:
    [[ no_unique_address ]] NetPtr N;
    [[ no_unique_address ]] Leaves leaves;

    // reticulations are on a stack of what is visible from the leaves in the current switching
    // NOTE: once the last adjacency in the stack is advanced over its last parent, the iter becomes invalid
    Switching<Net> sw;

    OutputVec cache;

    // NOTE: if we have Leaves stored in the iter, then we'll use those to base the active edges, otherwise, we use what's passed, or N->leaves()
    auto& get_leaves() { if constexpr (has_leaves) return mstd::access(leaves); else return N->leaves(); }
    const auto& get_leaves() const { if constexpr (has_leaves) return mstd::access(leaves); else return N->leaves(); }
  public:
    bool is_invalid() const { return sw.is_invalid(); }
    bool is_valid() const { return not is_invalid(); }

    const auto& get_active_parents() const { return sw.active_parent; }

    SwitchingIter() = default;

    template<class... Args> requires (has_leaves and (sizeof...(Args) != 0))
    SwitchingIter(const Net& _N, Args&&... args):
      leaves{std::forward<Args>(args)...}
    { cache = sw.get_active_edges(get_leaves()); }

    SwitchingIter(const Net& _N) {
      if constexpr (has_leaves) {
        _N->leaves().to_container(get_leaves());
      } else N = &_N;
      cache = sw.get_active_edges(get_leaves());
    }
    
    template<class... Args>
    auto active_adjacencies() const { return sw.template get_active_edges<AdjVec>(get_leaves());}
    template<class... Args>
    auto active_edges(Args&&... args) const { return sw.template get_active_edges<EdgeVec>(get_leaves());}

    auto& operator*() const { return cache; }
    auto operator->() const { return &cache; }

    SwitchingIter& operator++() {
      while(1) {
        if(sw.active_parent.empty()) {
          DEBUG5(std::cout << "all switchings considered, iter is now invalid...\n");
          assert(not is_valid());
          cache.clear();
          return *this;
        } else {
          auto& [r, parent] = sw.active_parent.back();
          if(++parent != Net::parents(r).end()) {
            cache = sw.get_active_edges(get_leaves());
            return *this;
          } else sw.active_parent.pop_back();
        }
      }
    }

    SwitchingIter operator++(int) { SwitchingIter old{*this}; ++(*this); return old; }

    bool operator==(const SwitchingIter& other) const {
      if(is_valid()) {
        if(other.is_valid()) {
          return sw == other.sw;
        } else return false;
      } else return not other.is_valid();
    }
  };

  template<StrictPhylogenyType Net, NodeContainerType<mstd::TR_ConstRefPtrOK> Leaves, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  using SwitchingFactory = mstd::IterFactory<SwitchingIter<Net, Leaves, OutputVec>>;

  //------------- deduction guides ------------------
  template<typename Net>
  SwitchingIter(Net) -> SwitchingIter<Net, NodeVec>;

}
