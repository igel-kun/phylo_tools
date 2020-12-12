
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

    void force_both(const Node x) { comp_root = visible_leaf = x; }
    void clear() { force_both(NoNode); }
    friend std::ostream& operator<<(std::ostream& os, const ComponentData& cd)
    { return os<<"{rt: "<< (cd.comp_root == NoNode ? std::string(".") : std::to_string(cd.comp_root))
                <<" vl: "<<(cd.visible_leaf == NoNode ? std::string(".") : std::to_string(cd.visible_leaf))<<"}"; }
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

      N[N.root()].comp_root = N.root();
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
      std::cout << "updating 'reticulation' "<<r<<" with parents "<<N.parents(r)<<'\n';
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
      if(N.out_degree(r) <= 1) N[r].comp_root = rt;
      return rt;
    }

    // build the network of component roots by following the reticulations above each root
    void compute_edges(const NodeVec& roots, EdgeVec* edges = nullptr)
    {
      for(const Node u: roots){
        assert(!N.is_reti(u));
        typename Network::NodeSet components_above;
        const Node rt = update_reticulations(u, &components_above);
        if((N.out_degree(u) == 0) && (rt != NoNode)) {
          std::cout << "setting visible leaf of "<<rt<<" to the leaf "<<u<<"\n";
          N[rt].visible_leaf = u;
        }
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

    // we want to be able to give a new Network-reference when copy-constructing
    TreeComponentInfos(const TreeComponentInfos& tc, Network& _N):
      N(_N), comp_DAG(tc.comp_DAG)
    {}
    TreeComponentInfos(TreeComponentInfos&& tc, Network& _N):
      N(_N), comp_DAG(std::move(tc.comp_DAG))
    {}


    // get the highest component-root that is visible on u
    Node get_highest_visible_comp_root(const Node u){
      // get highest initial component root reachable without reticulation
      std::cout << "getting highest visible from "<<u<<" ("<<N[u]<<")\n";
      Node rt = N[u].comp_root;
      while(rt != NoNode) {
        std::cout << "rt = "<<rt<<" ("<<N[rt]<<")\n";
        assert(N.has_node(rt));
        if(N.in_degree(rt) == 1){
          const Node p_rt = N.parent(rt);
          switch(N.in_degree(p_rt)){
            case 0:
              return p_rt;
            case 1:
              rt = N[p_rt].comp_root;
              assert(rt != NoNode); // the parent is a tree-node, so it should have a component-root
              return rt;
            default:
              return rt;
          }
        } else {
          assert(N.in_degree(rt) == 0);
          return rt;
        }
      }
      return rt;
    }



    // recomputes the component root of a node y from the component roots of its parents
    // return whether the root of y changed
    bool inherit_root(const Node y, const bool spread_info = false)
    {
      Node& y_root = N[y].comp_root;
      if((y_root != y) || (N.in_degree(y) > 1)){ // update only for non-component roots!
        std::cout << "computing new component root of "<<y<<" (spread = "<<spread_info<<")\n";
        Node rt;
        switch(N.in_degree(y)){
          case 0: 
            rt = y;
            break;
          case 1:
            {
              const Node x = N.parent(y);
              std::cout << "inheriting comp-root from "<<x<<": "<<N[x]<<"\n";
              // if we have in-degree 1, then we take the component-root of our parent x if x is a tree node or we are a leaf
              rt = ((N.in_degree(x) > 1) && (N.out_degree(y) > 1)) ? y : N[x].comp_root;
            }
            break;
          default:
            rt = comp_root_consensus(N.parents(y));
        }
        std::cout << "updating comp-root of "<<y<<" with "<<rt<<'\n';
        if(y_root != rt) {
          if(y_root == y) {
            std::cout << "as "<<y<<" was a comp-root, we will suppress it in the component DAG\n";
            comp_DAG.suppress_node(y);
          }
          y_root = rt;
          std::cout << "updated comp-info of "<<y<<" to "<< N[y]<<"\n";
          if(N.out_degree(y) > 0) {
            if(spread_info) for(const Node z: N.children(y)) inherit_root(z);
          } else if(rt != NoNode) {
            std::cout << "setting visible leaf of "<<rt<<" to "<<y<<"\n";
            N[rt].visible_leaf = y; // further, if we are a leaf, then mark the component root(s) visible from us
          }
          return true;
        } else return false;
      } else return false;
    }

    // return the intersection of component roots of nodes in the container c (or NoNode if the intersection is empty)
    template<class Container>
    Node comp_root_consensus(const Container& c)
    {
      if(c.empty()) return NoNode;
      auto c_iter = c.begin();
      const Node result = get_highest_visible_comp_root(*c_iter);
      while(++c_iter != c.end())
        if(get_highest_visible_comp_root(*c_iter) != result) return NoNode;
      return result;
    }
  };
    



}
