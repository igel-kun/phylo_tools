
#pragma once

#include "types.hpp"
#include "network.hpp"

namespace PT{

  //! get a list of component roots in preorder
  template<class Network, class NodeContainer = typename Network::NodeVec>
  NodeContainer get_tree_non_trivial_roots(const Network& N)
  {
    NodeContainer comp_roots;
    for(const Node u: N.nodes()){
      if(N.is_inner_tree_node(u)){
        const Node v = N.parent(u);
        if(N.is_reti(v)) append(comp_roots, v);
      }
    }
    return comp_roots;
  }

  template<class Network>
  struct TreeComponentInfos
  {
    using Translate = typename Network::NodeMap<Node>;

    const Network& N;
    // store for each node of N the root of its tree component
    Translate         component_root_of = { {N.root(), N.root()} };
    // store the list of all component roots
    NodeVec non_trivial_roots;
    NodeVec trivial_roots;
    // store the list of edges in the component-graph (each component represented by its root)
    EdgeVec           component_graph_edges;
    // for each component root x, if there is a tree-component directly below x whose root is visible on x, then store this root here
    HashMap<Node, Node> visible;


    // construction
    TreeComponentInfos(const Network& _N): N(_N)
    {
      compute_component_roots();
      // construct the component DAG from the non-trivial roots
      compute_edges(non_trivial_roots, true);
      // construct visibility also from the trivial roots (the leaves), but don't add their edges to the component DAG
      compute_edges(trivial_roots, false);
      // the root has not yet stored in the list of non-trivial roots, so add it here
      non_trivial_roots.emplace_back(N.root());
    };

    // compute component roots and return list of leaves encountered
    void compute_component_roots()
    {
      auto dfs = N.dfs().preorder();
      auto u_it = dfs.begin();
      while(++u_it != dfs.end()){
        const Node u = *u_it;
        switch(N.type_of(u)){
          case NODE_TYPE_LEAF:
            trivial_roots.push_back(u);
            break;
          case NODE_TYPE_TREE: {
              // note that u is not the root, so parent(u) is safe
              const Node pu = N.parent(u);
              if(N.is_reti(pu)){ // if the parent is a reticulation, register new component root
                component_root_of[u] = u;
                non_trivial_roots.push_back(u);
              } else component_root_of[u] = component_root_of[pu]; // if the parent is not a reticulation, copy their component root
              break;
            }
          default: break;
        }
      }
    }


    // update visibility of reticulation chains and return a list of components seen above
    //NOTE: also returns thee tree component root that sees r, or NoNode if there is no such root
    Node update_reticulations(const Node r, typename Network::NodeSet* components_above = nullptr)
    {
      // we use "r" to denote "uninitialized" and "NoNode" to denote "more than one component sees r"
      //NOTE: we cannot simply use "components_above.size() == 1" since previous branches might have already filled that
      Node rt = r;
      std::cout << "updating 'reticulation' "<<r<<'\n';
      for(const Node pr: N.parents(r)){
        Node next_rt;
        if(!N.is_reti(pr)){
          next_rt = map_lookup(component_root_of, pr, NoNode);
          if(components_above) append(*components_above, next_rt);
        } else next_rt = update_reticulations(pr, components_above);
        if(rt == r) rt = next_rt;
        if(rt != next_rt) rt = NoNode;
      }
      assert(rt != r); // rt == r only if r has no parents which should never be the case
      if(rt != NoNode) {
        std::cout << "marking "<<r<<" visible from "<<rt<<'\n';
        visible[rt] = r;
      }
      return rt;
    }

    // build the network of component roots by following the reticulations above each root
    void compute_edges(const NodeVec& roots, const bool place_edge)
    {
      for(const Node u: roots){
        assert(!N.is_reti(u));
        typename Network::NodeSet components_above;
        update_reticulations(u, &components_above);
        if(place_edge)
          for(const Node v: components_above)
            component_graph_edges.emplace_back(v, u);
      }
    }

  };


}
