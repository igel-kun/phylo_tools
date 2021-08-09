
// classes for handling tree extensions of networks

#pragma once

#include "utils/union_find.hpp"
#include "utils/set_interface.hpp"
#include "utils/extension.hpp"
#include "utils/network.hpp"

namespace PT{


  // construct an extension tree from a network & an extension
  // NOTE: make_node_data() will receive the description of the node in the network
  //       make_edge_data() will receive the two node-descriptions of the edges endpoints
  template<PhylogenyType _Network, TreeType _ExtTree>
  _ExtTree ext_to_tree(const _Network& N, const Extension& ex, _ExtTree&& T = _ExtTree(),
                            auto&& make_node_data = std::IgnoreFunction<NodeDataOr<_ExtTree>>(),
                            auto&& make_edge_data = std::IgnoreFunction<EdgeDataOr<_ExtTree>>()){
    // we can use a disjoint set forest with no-rank union to find the current highest node of the weakly-connected component of a node
    std::DisjointSetForest<NodeDesc> highest;
    NodeTranslation net_to_tree;

    DEBUG3(std::cout << "constructing extension tree from "<<ex<<std::endl);
    for(const auto& u: ex){
      const NodeDesc tree_u = test(net_to_tree, u) ? net_to_tree[u] : append(net_to_tree, u, make_node(make_node_data(u))).first->second;
      // step 1: add a new set to the DSF with only u
      highest.add_new_set({u});
      DEBUG3(std::cout << "highest ancestors of node "<<u<<": "<<highest<<std::endl);
      // step 2: establish u as the parent in Gamma of all highest nodes in all weakly connected components (in G[ex[1..u]]) of its children in N
      NodeVec new_children;
      for(const NodeDesc v: N.children(u)){
        try{
          const NodeDesc x = highest.set_of(v).get_representative();
          append(new_children, x);
        } catch(std::out_of_range& e) {
          throw(std::logic_error("trying to compute extension tree on a non-extension"));
        }
      }
      // step 3: register u as the new hightest node in the weakly connected components of its new children 
      // step 4: add edges u->v to the edgelist
      for(const Node v: new_children){
        const NodeDesc tree_v = test(net_to_tree, v) ? net_to_tree[v] : append(net_to_tree, v, make_node(make_node_data(v))).first->second;
        // NOTE: make sure the merge is not done by size but v is always plugged below u!
        highest.merge_sets_of(u, v, false);
        T.add_child(tree_u, tree_v, make_edge_data(tree_u, tree_v));
      }
    }
  }

  // compute the scanwidth of all nodes in a given extension tree
  template<PhylogenyType _Network, TreeType _Tree, std::ContainerType _Container>
  void ext_tree_sw_map(const _Tree& ext, const _Network& N, _Container& out) {
    for(const Node u: ext.dfs().postorder()){
      Degree sw_u = N.in_degree(u);
      for(const Node v: ext.children(u))
        sw_u += out.at(v);
      sw_u -= N.out_degree(u);
      append(out, u, sw_u);
      DEBUG3(std::cout << "found sw("<<u<<") = "<<sw_u<<std::endl);
    }
  }

  template<PhylogenyType _Network, TreeType _Tree, std::ContainerType _Container = typename _Network::DegreeMap>
  _Container ext_tree_sw_map(const _Tree& ext, const _Network& N)
  {
    _Container result;
    ext_tree_sw_map(ext, N, result);
    return result; // hopefully we'll get return-value optimization here ^^
  }



}// namespace
