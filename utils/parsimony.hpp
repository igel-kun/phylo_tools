
// this file contains dynamic programming approaches for hardwired/softwired/parental parsimony on networks
// with respect to the node-scanwidth of a given layout
// NOTE: in the worst case, this is equal to the reticulation number of the network (for a trivial layout) plus one

#pragma once

#include <array>
#include "tight_int.hpp"
#include "static_capacity_vector.hpp"
#include "extension.hpp"

namespace PT {
  
  // ==================== HARDWIRED PARSIMONY ========================
  // the DP is indexed by a mapping of character-states to parents p of incomplete reticulations r
  //    s.t. r is 'to the right' of p in a given linear extension
  template<size_t _num_states, size_t _node_scanwidth>
  struct HW_DP_Entry {
    static constexpr size_t num_states = _num_states;
    static constexpr size_t node_scanwidth = _node_scanwidth;

    //using Character = mstd::uint_tight<num_states>;
    using Character = mstd::optional_by_invalid<mstd::uint_tight<num_states>, -1>;
    using Index = mstd::static_capacity_vector<Character, node_scanwidth>;
    
    // an entry maps character states for the cut-nodes to a cost
    std::unordered_map<Index, size_t> costs;
  };


  template<PhylogenyType Phylo, class SW_Extension, class _Entry>
  struct Parsimony_HW_DP {
    using Entry = _Entry;
    using Index = typename Entry::Index;
    using ExtIndex = size_t;

    const Phylo N;
    const SW_Extension ext;

    std::unordered_map<NodeDesc, Entry> dp_table;

    size_t score_for(const ExtIndex i, const Index& index) {
      Entry& e = dp_table[u];
      auto [iter, success] = mstd::append(e.costs, index);
      size_t& cost = iter->second;
      if(success) {
        // index was not found in the table, so compute its cost using the previous table entry

      }
      return cost;
    }

    size_t score_below(const ExtIndex i) {
    }

    size_t score() { return parsimony_below(ext.size()); }
  };
}
