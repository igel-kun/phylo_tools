
#pragma once

#include "runes.hpp"
#include "tags.hpp"
#include "types.hpp"

namespace PT {

  // ========== Switching ==========
  // A switching is a decision of a single incoming arc of each reticulation in a network.
  // As such, each switching corresponds to a displayed tree, but multiple switchings might correspond to the same tree.
  // The corresponding tree can be computed from the switching by exhaustive deletion of unlabelled leaves and suppression of deg-2 nodes.

  // ------- Switching: helpers ---------
 
  // ------- Switching: main class ---------
  // Internally, a Switching maps each reticulation to an iterator of the parent-set, representing the current parent.
  // NOTE: operator(e) returns whether the edge e is switched OFF (that is, NOT in the switching),
  //       so the switching can be used as a forbidden-predicate when traversing a network.
  //       Thus, the switching can be traversed by traversing the network with the switching as forbidden-predicate.
  template<StrictPhylogenyType Net_>
  struct Switching {
    // ------- static stuff --------
    using Net = Net_;
    using ParentContainer = typename Net::ParentContainer;
    using ParentIter = mstd::iterator_of_t<ParentContainer>;
    using NetEdge = Net::Edge;

    // a parent selector that just selects the first parent; this is the default
    struct FirstParent {
      auto operator()(const NodeDesc r) const { return Net::parents(r).begin(); }
    };

    // ------- members --------
    NodeMap<ParentIter> active_parent;


    // ------- construction & desctruction ---------
    Switching() = default;

    // NOTE: we allow construction by either roots or leaves, indicated by the passed tag
    template<NodeOrIterableType Nodes, class DefaultParent>
    Switching(const roots_tag, const Nodes& X, DefaultParent&& parent_select) {
      for(const NodeDesc r: Net::retis_below(&X))
        append(active_parent, r, parent_select(r));
    }

    template<NodeOrIterableType Nodes, class DefaultParent>
    Switching(const leaves_tag, const Nodes& X, DefaultParent&& parent_select) {
      DEBUG4(std::cout << "constructing switching above "<<X<<"\n");
      for(const NodeDesc r: Net::nodes_above(&X)) {
        if(Net::is_reti(r))
          append(active_parent, r, parent_select(r));
      }
    }

    template<class DefaultParent>
    Switching(const Net& N, DefaultParent&& parent_select):
      Switching(roots_tag{}, N.roots(), std::forward<DefaultParent>(parent_select)) {}

    template<RootsOrLeavesTag Tag, NodeOrIterableType Nodes>
    Switching(const Tag t, const Nodes& X):
      Switching(t, X, FirstParent{}) {}

    Switching(const Net& N):
      Switching(N, FirstParent{}) {}


    // ------- operators --------
    bool operator==(const Switching& other) { return active_parent == other.active_parent; }
   
    // ------- methods: initialization --------
    // ------- methods: query --------
    template<class First> requires mstd::is_any_of<First, NodeDesc, ParentIter>
    bool is_switched_off(const First x, const NodeDesc y) const {
      const auto iter = active_parent.find(y);
      if(iter != active_parent.end()) {
        const ParentIter vp = iter->second;
        assert(vp != Net::parents(y).end());
        if constexpr (std::is_same_v<First, NodeDesc>)
          return (*vp != x);
        else return vp != x;
      } else return false;
    }
    bool is_switched_off(const auto& uv) const { return is_switched_off(uv.first, uv.second); }
    bool is_switched_on(const NodeDesc x, const NodeDesc y) const { return not is_switched_off(x, y); }
    bool is_switched_on(const auto& uv) const { return not is_switched_off(uv.first, uv.second); }

    template<EdgeContainerType Edges = std::vector<NetEdge>>
    Edges get_active_edges() const {
      Edges result;
      if constexpr (mstd::HasReserve<Edges>)
        result.reserve(active_parent.size());
      for(auto uv: active_parent | std::ranges::views::transform([](const auto& vu_pair) { return NetEdge{reverse_edge_tag{}, vu_pair}; }))
        append(result, std::move(uv));
      return result;
    }
    template<EdgeContainerType Edges = std::vector<NetEdge>, NodeOrIterableType Nodes>
    Edges get_active_edges_above(const Nodes& X) const {
      Edges result;
      for(const auto& v: Net::nodes_above(&X)) {
        const auto iter = active_parent.find(v);
        if(iter != active_parent.end())
          append(result, iter->second, v);
      }
      return result;
    }

    // ------- methods: modification --------
  };


  // This is a predicate that returns true iff an edge/node-pair/parent-iter is switched off in the given switching.
  // It can be useed as 'forbidden' predicate in DFS-traversals.
  // NOTE: If a network N may contain double-edges, then N may have xy twice, only one of which is forbidden (switched off).
  //       To do this right, we have to be able to call operator() with an iterator into node_of(y).parents()
  // NOTE: If one wants to use this with a DFSIterator, the user needs to make sure it's a reverse traversal
  //       (otherwise the DFS will store iterators into node_of(u).children() which will not match our ParentIters here).
  using ExposalTag = uint8_t;
  constexpr ExposalTag expose_twonodes = 0x01;
  constexpr ExposalTag expose_nodepair = 0x02;
  constexpr ExposalTag expose_edge = 0x04;
  constexpr ExposalTag expose_parent_iter = 0x08;

  template<StrictPhylogenyType Net, ExposalTag expose = expose_nodepair>
  struct SwitchedOffPredicate {
  protected:
    const Switching<Net>* switching;

  public:
    using ParentIter = typename Switching<Net>::ParentIter;

    SwitchedOffPredicate(const Switching<Net>& s):
      switching(&s) {}
    SwitchedOffPredicate(const Switching<Net>* s):
      switching(s)
    { assert(s != nullptr); }

    bool operator()(const NodeDesc x, const NodeDesc y) const requires ((expose & expose_twonodes) != 0) { return switching->is_switched_off(x, y); }
    bool operator()(const NodePair xy) const requires ((expose & expose_nodepair) != 0) { return switching->is_switched_off(xy.first, xy.second); }
    bool operator()(const EdgeType auto& xy) const requires ((expose & expose_edge) != 0) { return switching->is_switched_off(xy.tail(), xy.head()); }
    bool operator()(const ParentIter& x, const NodeDesc y) const requires ((expose & expose_parent_iter) != 0) { return switching->is_switched_off(x, y); }
  };
 
  // ------- Switching: factories ---------
  
  // ------- Switching: concepts ---------
  template<class T>
  struct is_switching { static constexpr bool value = false; };
  template<StrictPhylogenyType Net>
  struct is_switching<Switching<Net>> { static constexpr bool value = true; };

  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  constexpr bool is_switching_v = mstd::apply_rune_v<T, rune> or is_switching<mstd::apply_rune_t<T, rune>>::value;

  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept SwitchingType = is_switching_v<T, rune>;
 
  // ------- Switching: deduction guides ---------
  
  // ------- Switching: defaults ---------

}



