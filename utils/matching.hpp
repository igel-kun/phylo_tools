
#pragma once

// an implementation of bipartite matching by Hopcroft-Karp (with some tweaks by me ;] )

namespace PT {

  template<class Node,
           class Adjacencies = std::unordered_map<Node, std::unordered_set<Node>>,
           class Matching = std::unordered_map<Node, Node>>
  class bipartite_matching
  {
    // unmatched nodes are only interesting on the left
    std::unordered_bitset left_unmatched;
    std::unordered_bitset left_seen;
    //NOTE: we need different matchings for left & right because the domains of the sides may overlap
    Matching left_match, right_match;
    
    const Adjacencies& adj;

    using NodeList = typename Adjacencies::mapped_type;
    using adj_pair = decltype(*adj.begin());

    // match a node u greedily in V (it's neighbors)
    void initial_greedy_one_node(const adj_pair& uV)
    {
      const Node u = uV.first;
      for(const Node v: uV.second){
        if(right_match.emplace(v, u).second) {
          left_match.emplace(u, v);
          return;
        }
      }
      left_unmatched.set(u);
    }

    void initial_greedy() { for(const auto& uV: adj) initial_greedy_one_node(uV); }

    // return whether the matching has been augmented, use 'seen' to indicate the visited nodes
    //NOTE: u is a node on the left
    bool augment_matching(const Node u)
    {
      if(!left_seen.set(u)) return false;
      // if u is matched, ignore u's matching partner when looking for augmenting paths
      const Node skip = map_lookup(left_match, u, NoNode);
      for(const Node v: adj.at(u)) if(v != skip) {
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
      for(const Node u: left_unmatched)
        if(augment_matching(u)){
          left_unmatched.unset(u);
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
