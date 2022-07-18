
#pragma once

#include "stl_utils.hpp"

// an implementation of bipartite matching by Hopcroft-Karp (with some tweaks by me ;] )

namespace PT {

  template<class Adjacencies = NodeMap<NodeSet>,
           class Matching = NodeMap<NodeDesc>>
  class bipartite_matching {
    // unmatched nodes are only interesting on the left
    NodeSet left_unmatched;
    NodeSet left_seen;
    //NOTE: we need different matchings for left & right because the domains of the sides may overlap
    Matching left_match, right_match;
    
    const Adjacencies& adj;

    using NodeList = mstd::mapped_type_of_t<Adjacencies>;
    using adj_pair = mstd::value_type_of_t<Adjacencies>;

    // match a node u greedily in V (it's neighbors)
    void initial_greedy_one_node(const adj_pair& uV) {
      const NodeDesc u = uV.first;
      for(const NodeDesc v: uV.second){
        if(right_match.emplace(v, u).second) {
          left_match.emplace(u, v);
          return;
        }
      }
      mstd::set_val(left_unmatched, u);
    }

    void initial_greedy() { for(const auto& uV: adj) initial_greedy_one_node(uV); }

    // return whether the matching has been augmented, use 'seen' to indicate the visited nodes
    //NOTE: u is a node on the left
    bool augment_matching(const NodeDesc u) {
      if(!mstd::set_val(left_seen, u)) return false;
      // if u is matched, ignore u's matching partner when looking for augmenting paths
      const NodeDesc skip = mstd::map_lookup(left_match, u, NoNode);
      for(const NodeDesc v: adj.at(u)) if(v != skip) {
        // if v is not matched either, we can augment the matching with uv
        const auto [v_match_it, success] = right_match.emplace(v, u);
        if(success) {
          // if v is unmatched, then insert uv in the matching
          left_match.emplace(u, v);
          return true;
        } else if(augment_matching(v_match_it->second)){ // if v is matched, find an augmenting path from its matching partner
          // if we found an augmenting path from v's matching partner, then flip it
          left_match[u] = v;
          right_match[v] = u;
          return true;
        }
      }
      return false;
    }

    void compute_matching()
    {
_start_here:
      left_seen.clear();
      for(const NodeDesc u: left_unmatched)
        if(augment_matching(u)){
          mstd::erase(left_unmatched, u);
          goto _start_here;
        }
    }


  public:

    bipartite_matching(const Adjacencies& _adj): adj(_adj)
    { 
      initial_greedy(); 
      std::cout << "computing matching for "<<adj<<" (starting with greedy "<<left_match<<")\n";
    }

    // after initial_greedy, the matching is maximal (not maximum)
    const Matching& maximal_matching() const
    { return left_match; }

    const Matching& maximum_matching()
    {
      compute_matching();
      return left_match;
    }

  };
}
