
#pragma once

#include "network.hpp"

namespace PT{

  struct ComponentRootInfo{
    using CompRootDAG = Network;

  protected:
    // list of component roots in preorder
    IndexVec comp_roots;

    // the component DAG as mapping of nodes to predecessors and successors
    NodeMap<IndexSet> cr_pred;
    NodeMap<IndexSet> cr_succ;
    
    // map each vertex to its component root
    // if all parents of a reticulation r have the same component root, we say that also r has this root
    NodeVec my_root;
   
    // fill cr_pred and cr_succ by climbing up from v
    void compute_cr_pred(const uint32_t& v, const uint32_t parent) {
      const NodeDesc parent_root = my_root[parent];
      if(parent_root == NoNode){
        // if the next vertex does not have a component root, then climb higher
        for(const NodeDesc pp: Network::parents(parent))
          compute_cr_pred(v, pp);
      } else {
        // if the next vertex has a component root, then set pred & succ
        cr_pred[v].insert(parent_root);
        cr_succ[parent_root].insert(v);
      }
    }

    void compute_comp_roots(const NodeDesc v, const NodeDesc parent_root = NoNode) {
      NodeDesc v_root = Network::is_reti(v) ? NoNode : v;
      
      if(parent_root == NoNode){
        my_root[v] = v_root;
        if(v_root != NoNode) {
          comp_roots.push_back(v);
        }
      } else v_root = my_root[v] = my_root[parent_root];

      for(const NodeDesc w: Network::children(v))
        compute_comp_roots(w, v_root);
    }


  public:

    ComponentRootInfo(const Network& N_):
      my_root(N_.num_nodes(), 0)
    {
      // step 1: compute the vector of component roots
      compute_comp_roots(N.get_root());
      // step 2: compute predecessors and successors of component roots
      for(uint32_t i = comp_roots.size() - 1; i != 0; --i){
        assert(Network::out_edges(comp_roots[i]).size() == 1);
        compute_cr_pred(comp_roots[i], mstd::front(Network::out_edges(comp_roots[i])).tail());
      }
    }

    const IndexVec& get_comp_roots_preordered() const { return comp_roots; }

    // return the root of the component containing v, or NO_ROOT if v is a reticulation
    const NodeDesc operator[](const NodeDesc v) const noexcept { return my_root[v]; }
  };
}
