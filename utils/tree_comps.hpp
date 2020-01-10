
#pragma once

#include "network.hpp"

#define NO_ROOT UINT32_MAX

namespace PT{

  class ComponentRootInfo{
  public:
    typedef Network CompRootDAG;

  protected:
    const Network& N;

    // list of component roots in preorder
    IndexVec comp_roots;

    // the component DAG as mapping of nodes to predecessors and successors
    std::unordered_map<uint32_t, IndexSet> cr_pred;
    std::unordered_map<uint32_t, IndexSet> cr_succ;
    
    // map each vertex to its component root
    // if all parents of a reticulation r have the same component root, we say that also r has this root
    uint32_t* const my_root;
   
    // fill cr_pred and cr_succ by climbing up from v
    void compute_cr_pred(const uint32_t& v, const uint32_t parent)
    {
      const uint32_t& parent_root = my_root[parent];
      if(parent_root == NO_ROOT){
        // if the next vertex does not have a component root, then climb higher
        for(const uint32_t pp: N[parent].parents())
          compute_cr_pred(v, pp);
      } else {
        // if the next vertex has a component root, then set pred & succ
        cr_pred[v].insert(parent_root);
        cr_succ[parent_root].insert(v);
      }
    }

    void compute_comp_roots(const uint32_t& v, const uint32_t parent_root = NO_ROOT)
    {
      uint32_t v_root = N[v].is_reti() ? NO_ROOT : v;
      
      if(parent_root == NO_ROOT){
        my_root[v] = v_root;
        if(v_root != NO_ROOT) {
          comp_roots.push_back(v);
        }
      } else v_root = my_root[v] = my_root[parent_root];

      for(const uint32_t w: N[v].children())
        compute_comp_roots(w, v_root);
    }// function


  public:

    ComponentRootInfo(const Network& _N):
      N(_N),
      my_root((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t)))
    {
      // step 1: compute the vector of component roots
      compute_comp_roots(N.get_root());
      // step 2: compute predecessors and successors of component roots
      for(uint32_t i = comp_roots.size() - 1; i != 0; --i){
        assert(N[comp_roots[i]].out.size() == 1);
        compute_cr_pred(comp_roots[i], N[comp_roots[i]].out[0].tail());
      }
    }

    const IndexVec& get_comp_roots_preordered() const
    {
      return comp_roots;
    }

    // return the root of the component containing v, or NO_ROOT if v is a reticulation
    const uint32_t operator[](const uint32_t v) const
    {
      return my_root[v];
    }

    ~ComponentRootInfo()
    {
      free(my_root);
    }
  };
}
