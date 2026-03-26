
#pragma once

#include "runes.hpp"
#include "tags.hpp"
#include "types.hpp"

namespace PT {

  // ========== Switching ==========
  // describe what Switching does...

  // ------- Switching: helpers ---------
 
  // ------- Switching: main class ---------
  // NOTE: the switching can be used as a forbidden-predicate when traversing a network
  template<StrictPhylogenyType Net_>
  struct Switching {
    // ------- static stuff --------
    using Net = Net_;
    using ParentContainer = typename Net::ParentContainer;
    using ParentIter = mstd::iterator_of_t<ParentContainer>;
    using Edge = Net::Edge;

    // a parent selector that just selects the first parent; this is the default
    struct FirstParent {
      auto operator()(const NodeDesc r) const { return Net::parents(r).begin(); }
    };

    // ------- members --------
    NodeMap<ParentIter> active_parent;


    // ------- construction & desctruction ---------
    // NOTE: we allow construction by either roots or leaves, which is autodetected by checking whether the first one has children
    Switching() = default;

    template<NodeOrContainerType Nodes, class DefaultParent>
    Switching(const roots_tag, const Nodes& X, DefaultParent&& parent_select) {
      for(const NodeDesc r: Net::retis_below(X))
        append(active_parent, r, parent_select(r));
    }
    template<NodeOrContainerType Nodes, class DefaultParent>
    Switching(const leaves_tag, const Nodes& X, DefaultParent&& parent_select) {
      for(const NodeDesc r: Net::retis_above(X))
        append(active_parent, r, parent_select(r));
    }

    template<class DefaultParent>
    Switching(const Net& N, DefaultParent&& parent_select): Switching(roots_tag{}, N.roots(), std::forward<DefaultParent>(parent_select)) {}

    template<RootsOrLeavesTag Tag, NodeOrContainerType Nodes>
    Switching(const Tag t, const Nodes& X): Switching(t, X, FirstParent{}) {}

    Switching(const Net& N): Switching(N, FirstParent{}) {}


    // ------- operators --------
    bool operator==(const Switching& other) { return active_parent == other.active_parent; }

    // return true iff (u,v) is switched off (to be useed as 'forbidden' predicate)
    bool operator()(const NodeDesc x, const NodeDesc y) const { return is_switched_off(x, y); }
    bool operator()(const NodePair uv) const { return operator()(uv.first, uv.second); }
    bool operator()(const EdgeOf<Net>& uv) const { return operator()(uv.as_pair()); }
    
    // ------- methods: initialization --------
    // ------- methods: query --------
    bool is_switched_off(const NodeDesc x, const NodeDesc y) const {
      const auto iter = active_parent.find(y);
      if(iter != active_parent.end()) {
        const ParentIter vp = iter->second;
        assert(vp != Net::parents(y).end());
        return (*vp != x);
      } else return false;
    }
    bool is_switched_off(const auto& uv) const { return is_switched_off(uv.first, uv.second); }
    bool is_switched_on(const NodeDesc x, const NodeDesc y) const { return not is_switched_off(x, y); }
    bool is_switched_on(const auto& uv) const { return not is_switched_off(uv.first, uv.second); }

    // ------- methods: modification --------
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



