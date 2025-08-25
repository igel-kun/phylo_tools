
#pragma once

#include "runes.hpp"

#include "types.hpp"
#include "iter_factory.hpp"

namespace PT {

  template<StrictPhylogenyType Net_>
  struct Switching {
    using Net = Net_;
    using ParentContainer = typename Net::ParentContainer;
    using ParentIter = mstd::iterator_of_t<ParentContainer>;
    using Edge = Net::Edge;

    NodeMap<ParentIter> active_parent;

    bool is_invalid() const { return active_parent.empty(); }
    bool is_valid() const { return not is_invalid(); }

    bool is_switched_off(const NodeDesc x, const NodeDesc y) const { return Net::is_reti(y) and (*(active_parent.at(y)) != x); }
    bool is_switched_off(const auto& uv) const { return is_switched_off(uv.first, uv.second); }
    bool is_switched_on(const NodeDesc x, const NodeDesc y) const { return not is_switched_off(x, y); }
    bool is_switched_on(const auto& uv) const { return not is_switched_off(uv.first, uv.second); }

    // NOTE: the active_edges are guaranteed in post-order of the switching (the tree containing all switched-on edges)
    template<class Out = NetEdgeVec<Net>, class SWMaybeConst, NodeIterableType Nodes, class ParentSelector>
      requires mstd::is_same_v<SWMaybeConst, Switching>
    static Out _get_active_edges(SWMaybeConst& sw, Nodes&& leaves, ParentSelector&& parent_select) {
      Out out;
      NodeSet seen;
      if constexpr (mstd::is_iterable_with_size<Nodes>)
        seen.reserve(2*leaves.size());

      for(NodeDesc v: leaves) {
        DEBUG6(std::cout << "next node: "<< v << "\n");
        while(append(seen, v).second and LIKELY(Net::in_degree(v) > 0)) {          
          ParentIter vp;
          if(Net::in_degree(v) > 1) {
            if constexpr (not std::is_const_v<SWMaybeConst>) {
              const auto [iter, success] = sw.active_parent.try_emplace(v);
              if(success) {
                iter->second = vp = parent_select(v);
                DEBUG6(std::cout << "added "<<v<<" with it's first parent "<<*vp<<" to active_parent map\n");
              } else vp = iter->second;
            } else vp = sw.active_parent.at(v);
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
    template<class Out = NetEdgeVec<Net>, NodeIterableType Nodes, class ParentSelector>
    Out get_active_edges(Nodes&& leaves, ParentSelector&& parent_select) const {
      return _get_active_edges(*this, std::forward<Nodes>(leaves), std::forward<ParentSelector>(parent_select));
    }
    template<class Out = NetEdgeVec<Net>, NodeIterableType Nodes, class ParentSelector>
    Out get_active_edges(Nodes&& leaves, ParentSelector&& parent_select) {
      return _get_active_edges(*this, std::forward<Nodes>(leaves), std::forward<ParentSelector>(parent_select));
    }

    // by default, just select the first parent for each reticulation
    template<class Out = NetEdgeVec<Net>, NodeIterableType Nodes>
    Out get_active_edges(Nodes&& leaves) { return get_active_edges(std::forward<Nodes>(leaves), [](const NodeDesc r){ return Net::parents(r).begin(); }); }
    template<class Out = NetEdgeVec<Net>, NodeIterableType Nodes>
    Out get_active_edges(Nodes&& leaves) const { return get_active_edges(std::forward<Nodes>(leaves), [](const NodeDesc r){ return Net::parents(r).begin(); }); }

    bool operator==(const Switching& other) { return active_parent == other.active_parent; }
  };

  //------------- concepts ------------------
  template<class T>
  struct is_switching { static constexpr bool value = false; };
  template<StrictPhylogenyType Net>
  struct is_switching<Switching<Net>> { static constexpr bool value = true; };

  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  constexpr bool is_switching_v = mstd::apply_rune_v<T, rune> or is_switching<mstd::apply_rune_t<T, rune>>::value;

  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept SwitchingType = is_switching_v<T, rune>;

  // ---------------- helpers ----------------
  template<SwitchingType S>
  struct NetworkOf_<S> { using type = S::Net; };

  // --------------- iterators ---------------------
  template<StrictPhylogenyType Net, NodeContainerType<mstd::TR_PtrVoidOK> Leaves_ = void, mstd::VectorType OutputVec = NetEdgeVec<Net>>
  struct SwitchingIter:
    public mstd::iter_traits_from_reference<OutputVec>
  {
    using Traits = mstd::iter_traits_from_reference<OutputVec>;
    using EdgeVec = NetEdgeVec<Net>;
    using AdjVec = NetAdjVec<Net>;
    static constexpr bool has_leaves = not std::is_void_v<Leaves_>;
    using Leaves = std::conditional_t<has_leaves, Leaves_, mstd::monostate>;
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

    template<class First, class... Args>
      requires (has_leaves and not mstd::is_any_of<First, SwitchingIter, Net>)
    SwitchingIter(First&& first, Args&&... args):
      leaves{std::forward<First>(first), std::forward<Args>(args)...}
    { cache = sw.get_active_edges(get_leaves()); }

    SwitchingIter(const Net* N_) {
      if constexpr (has_leaves) {
        N_->leaves().to_container(get_leaves());
      } else N = N_;
      cache = sw.get_active_edges(get_leaves());
    }
    SwitchingIter(const Net& N_): SwitchingIter(&N_) {}
    
    auto active_adjacencies() const { return sw.template get_active_edges<AdjVec>(get_leaves());}
    auto active_edges() const { return sw.template get_active_edges<EdgeVec>(get_leaves());}
    const auto& get_switching() const { return sw; }

    auto& operator*() const { return cache; }
    auto operator->() const { return &cache; }

    SwitchingIter& operator++() {
      for(auto& [v, vp]: sw.active_parent) {
        if(++vp != Net::parents(v).end()) {
          cache = sw.get_active_edges(get_leaves());
          return *this;
        } else vp = Net::parents(v).begin();
      }
      DEBUG5(std::cout << "all switchings considered, rendering iter invalid...\n");
      sw.active_parent.clear();
      cache.clear();
      assert(not is_valid());
      return *this;
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

  template<StrictPhylogenyType Net,
           NodeContainerType<mstd::TR_ConstRefPtrOK> Leaves,
           mstd::VectorType OutputVec = NetEdgeVec<Net>>
  using SwitchingFactory = mstd::IterFactory<SwitchingIter<Net, Leaves, OutputVec>>;

  //------------- deduction guides ------------------
  template<typename Net>
  SwitchingIter(Net) -> SwitchingIter<Net, NodeVec>;
  
}
