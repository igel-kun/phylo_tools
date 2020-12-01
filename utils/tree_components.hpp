
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


  struct ComponentData
  {
    Node comp_root = NoNode;
    Node visible_leaf = NoNode;

    // conservative "set" functions that only set if the value was NoNode before and return whether the value was set
    bool set_comp_root(const Node x)
    {
      if(comp_root == NoNode) {
        comp_root = x; 
        return true;
      } else return false;
    }

    bool set_visible_leaf(const Node x)
    {
      if(visible_leaf == NoNode) {
        visible_leaf = x;
        return true;
      } else return false;
    }
    void force_both(const Node x) { comp_root = visible_leaf = x; }
    void clear() { force_both(NoNode); }
    friend std::ostream& operator<<(std::ostream& os, const ComponentData& cd) { return os<<"{rt: "<<cd.comp_root<<" vl: "<<cd.visible_leaf<<"}"; }
  };



  // use this with networks with node-data that derives from ComponentData
  template<class Network,
    class = std::enable_if_t<std::is_convertible_v<typename Network::NodeData, ComponentData>>>
  class TreeComponentInfos
  {
    Network& N;
  public:
    RWNetwork<> comp_DAG;

  protected:

    EdgeVec compute_comp_DAG()
    {
      std::cout << "constructing component-infos...\n";
      
      EdgeVec result;
      NodeVec non_trivial_roots;
      NodeVec trivial_roots;

      N[N.root()].force_both(N.root());
      compute_component_roots(trivial_roots, non_trivial_roots);
      // construct the component DAG from the non-trivial roots
      compute_edges(non_trivial_roots, &result);
      // construct visibility also from the trivial roots (the leaves), but don't add their edges to the component DAG
      compute_edges(trivial_roots);
      // the root has not yet stored in the list of non-trivial roots, so add it here
      //non_trivial_roots.emplace_back(N.root());
      return result;
    }

    // compute component roots and return list of leaves encountered
    void compute_component_roots(NodeVec& trivial_roots, NodeVec& non_trivial_roots)
    {
      auto dfs = N.dfs().preorder();
      auto u_it = dfs.begin();
      while(++u_it != dfs.end()){
        const Node u = *u_it;
        switch(N.type_of(u)){
          case NODE_TYPE_LEAF:
            trivial_roots.push_back(u);
            break;
          case NODE_TYPE_INTERNAL_TREE: {
              // note that u is not the root, so parent(u) is safe
              const Node pu = N.parent(u);
              if(N.is_reti(pu)){ // if the parent is a reticulation, register new component root
                N[u].comp_root = u;
                non_trivial_roots.push_back(u);
              } else N[u].comp_root = N[pu].comp_root; // if the parent is not a reticulation, copy their component root
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
          next_rt = N[pr].comp_root;
          if(components_above && (next_rt != NoNode)) append(*components_above, next_rt);
        } else next_rt = update_reticulations(pr, components_above);
        if(rt == r) rt = next_rt;
        if(rt != next_rt) rt = NoNode;
      }
      assert(rt != r); // rt == r only if r has no parents which should never be the case
      if(rt != NoNode) comp_root_visible(rt, r);
      return rt;
    }

    // build the network of component roots by following the reticulations above each root
    void compute_edges(const NodeVec& roots, EdgeVec* edges = nullptr)
    {
      for(const Node u: roots){
        assert(!N.is_reti(u));
        typename Network::NodeSet components_above;
        update_reticulations(u, &components_above);
        if(edges)
          for(const Node v: components_above)
            edges->emplace_back(v, u);
      }
    }



  public:

    // construction
    TreeComponentInfos(Network& _N):
      N(_N), comp_DAG(compute_comp_DAG())
    {}


    // recomputes the component root of a node y from the component roots of its parents
    void inherit_root(const Node y)
    {
      if(N.in_degree(y) > 0)
        comp_root_visible(N[N.parent(y)].comp_root, y);
      else N[y].comp_root = y;
      std::cout << "updated component root of "<<y<<" to "<< N[y].comp_root<<"\n";
    }

    // register that a component root r is visible from a node v
    void comp_root_visible(const Node r, const Node v)
    {
      std::cout << "marking "<<r<<" visible from "<<v<<'\n';

      Node& v_root = N[v].comp_root;
      if(v_root != r){     
        v_root = r;
        switch(N.out_degree(v)){
          case 0:
            N[r].visible_leaf = v;
            break;
          case 1:
            {
              const Node w = N.any_child(v);
              for(const Node pw: N.parents(w))
                if(N[pw].comp_root != r)
                    goto no_unique_root;
              comp_root_visible(r, w);
            }
  no_unique_root:
            break;
          default:
            for(const Node w: N.children(v))
              comp_root_visible(r, w);
        }
      }
    }

    // return the intersection of component roots of nodes in the container c (or NoNode if the intersection is empty)
    template<class Container>
    Node comp_root_consensus(const Container& c)
    {
      if(c.empty()) return NoNode;
      auto c_iter = c.begin();
      const Node result = *c_iter;
      while(++c_iter != c.end())
        if(N[*c_iter].component_root != result) return NoNode;
      return result;
    }
  };
    



}
