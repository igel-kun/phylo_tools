
// classes for handling tree extensions of networks

#pragma once

#include "utils/union_find.hpp"
#include "utils/set_interface.hpp"
#include "utils/extension.hpp"
#include "utils/network.hpp"

namespace PT{

  // construct an edgelist of an extension tree from a network & an extension
  template<class _Network, class _Edgelist, class _Node = typename _Network::Node>
  void ext_to_tree(const _Network& N, const Extension<_Node>& ex, _Edgelist& el)
  {
    using NetNode = typename _Network::Node;
    // we can use a disjoint set forest with no-rank union to find the current highest node of the weakly-connected component of a node
    std::DisjointSetForest<NetNode> highest;

    DEBUG3(std::cout << "constructing extension tree from "<<ex<<std::endl);
    for(const auto& u: ex){
      // step 1: add a new set to the DSF with only u
      highest.add_new_set({u});
      DEBUG3(std::cout << "highest ancestors of node "<<u<<": "<<highest<<std::endl);
      // step 2: establish u as the parent in Gamma of all highest nodes in all weakly connected components (in G[ex[1..u]]) of its children in N
      std::unordered_set<NetNode> new_children;
      for(const NetNode v: N.children(u)){
        try{
          const NetNode x = highest.set_of(v).get_representative();
          append(new_children, x);
        } catch(std::out_of_range& e) {
          throw(std::logic_error("trying to compute extension tree on a non-extension"));
        }
      }
      // step 3: register u as the new hightest node in the weakly connected components of its new children 
      // step 4: add edges u->v to the edgelist
      for(const NetNode v: new_children){
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
    for(const auto u: ext.get_nodes_postorder()){
      uint32_t sw_u = N.in_degree(u);
      for(const auto v: ext.children(u))
        sw_u += out.at(v);
      sw_u -= N.out_degree(u);
      append(out, u, sw_u);
      DEBUG3(std::cout << "found sw("<<u<<") = "<<sw_u<<std::endl);
    }
  }
  template<class _Network, class _Tree, class _Container = std::unordered_map<typename _Network::Node, uint32_t>>
  _Container ext_tree_sw_map(const _Tree& ext, const _Network& N)
  {
    _Container result;
    ext_tree_sw_map(ext, N, result);
    return result;
  }



}// namespace
