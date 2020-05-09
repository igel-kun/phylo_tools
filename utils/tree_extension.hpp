
// classes for handling tree extensions of networks

#pragma once

#include "utils/union_find.hpp"
#include "utils/set_interface.hpp"
#include "utils/extension.hpp"
#include "utils/network.hpp"

namespace PT{

  // construct an edgelist of an extension tree from a network & an extension
  template<class _Network, class _Edgelist>
  void ext_to_tree(const _Network& N, const Extension& ex, _Edgelist& el)
  {
    // we can use a disjoint set forest with no-rank union to find the current highest node of the weakly-connected component of a node
    std::DisjointSetForest<Node> highest;

    DEBUG3(std::cout << "constructing extension tree from "<<ex<<std::endl);
    for(const auto& u: ex){
      // step 1: add a new set to the DSF with only u
      highest.add_new_set({u});
      DEBUG3(std::cout << "highest ancestors of node "<<u<<": "<<highest<<std::endl);
      // step 2: establish u as the parent in Gamma of all highest nodes in all weakly connected components (in G[ex[1..u]]) of its children in N
      NodeSet new_children;
      for(const Node v: N.children(u)){
        try{
          const Node x = highest.set_of(v).get_representative();
          append(new_children, x);
        } catch(std::out_of_range& e) {
          throw(std::logic_error("trying to compute extension tree on a non-extension"));
        }
      }
      // step 3: register u as the new hightest node in the weakly connected components of its new children 
      // step 4: add edges u->v to the edgelist
      for(const Node v: new_children){
        // NOTE: make sure the merge is not done by size but v is always plugged below u!
        highest.merge_sets_of(u, v, false);
        append(el, u, v);
        DEBUG3(std::cout << "appended " << u << " -> "<< v<<" edges are now: "<<el<<std::endl);
      }
    }
  }

  // compute the scanwidth of all nodes in a given extension tree
  template<class _Network, class _Tree, class _Container>
  void ext_tree_sw_map(const _Tree& ext, const _Network& N, _Container& out)
  {
    for(const Node u: ext.dfs().postorder()){
      Degree sw_u = N.in_degree(u);
      for(const Node v: ext.children(u))
        sw_u += out.at(v);
      sw_u -= N.out_degree(u);
      append(out, u, sw_u);
      DEBUG3(std::cout << "found sw("<<u<<") = "<<sw_u<<std::endl);
    }
  }

  template<class _Network, class _Tree, class _Container = typename _Network::DegreeMap>
  _Container ext_tree_sw_map(const _Tree& ext, const _Network& N)
  {
    _Container result;
    ext_tree_sw_map(ext, N, result);
    return result; // hopefully we'll get return-value optimization here ^^
  }



}// namespace
