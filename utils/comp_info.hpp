
#pragma once

#include "network.hpp"

namespace TC{

  class TreeComponentInfo{
    const Network& N;

    // list of component roots in preorder
    IndexVec comp_roots;  
    IndexVec reticulations;
    
    // map each vertex to its component root
    // if all parents of a reticulation r have the same component root, we say that also r has this root
    uint32_t* const my_root;
    
    // remember for each vertex on whom it is stable (NOTE: this will have false negatives)
    uint32_t* const stability;   

    void compute_comp_roots()
    {
      for(uint32_t u_idx = 0; u_idx < N.num_vertices; ++u_idx){
        const Vertex& u = N.vertices[u_idx];
        // if we are a tree vertex, we can 
        if(!u.is_reti()){
          const uint32_t parent_idx = u.pred[0];
          const Vertex& parent = N.vertices[parent_idx];
          // if the parent is a reticulation, we are a new component root, otherwise, we have the same component root as our parent
          if(parent.is_reti()){
            my_root[sub_root] = sub_root;
            comp_roots.push_back(sub_root);
          } else my_root[sub_root] = my_root[parent_idx];
        } else reticulations.push_back(u_idx);
      }
      // if all parents of a reticulation have the same component root, then the reticulation is considered to have this root too
      // otherwise, we set the root of reticulations to UINT32_MAX
      for(uint32_t r_idx : reticulations){
        const Vertex& r = N.vertices[r_idx];
        uint32_t root_accu = my_root[r.pred[0]];
        if(root_accu != UINT32_MAX){
          for(uint32_t i = 1; i < r.pred.count; ++i){
            if(my_root[r.pred[i]] != root_accu){
              root_accu = UINT32_MAX;
              break;
            }
          }
        }
        my_root[u_idx] = root_accu;
      }
    }

#warning TODO: find a good way to compute the LSA-tree instead of this approximation here
    // note: this will not do lots of effort, but it will be accurate for the lowest tree components
    void compute_stability()
    {
      for(uint32_t u_idx = N.num_vertices; u_idx-- > 0;){
        const Vertex& u = N.vertices[u_idx];
        uint32_t& u_stability;
        if(!u.is_leaf()){
          // if any non-reticulate child of u is stable, so is u
          for(uint32_t i = 0; i < u.succ.count; ++i){
            const uint32_t child_idx = succ[i];
            const Vertex& child = N.vertices[child_idx];
            if(!child.is_reti()){
              const uint32_t child_stability = stability[child_idx];
              if(child_stability != 0) {
                u_stability = child_stability;
                break;
              }
            }
          }
        } else u_stability = u_idx; // if u is a leaf, it is stable on itself

        // if u is stable, then so is its component root (note: this is implied by the previous loop for non-reticulations)
        if(u.is_reti() && (u_stability != UINT32_MAX) && (!my_root[u_idx] == UINT32_MAX))
          stability[my_root[u_idx]] = u_stability;
      }
    }

  public:

    TreeComponentInfo(const Network& _N):
      N(_N),
      my_root((uint32_t*)malloc(_N.num_vertices * sizeof(uint32_t)),
      stability((uint32_t*)calloc(_N.num_vertices, sizeof(uint32_t)),
    {
      assert(N.is_preordered());
      compute_comp_roots();
      compute_stability();

#warning TODO: continue here! We need to apply the reduction rule to stable tree components and update the information; then maybe find good branching vertices (bicombining reticulations whose both parents are tree vetices f.ex.) and branch on them before firing up the dynamic programming
      assert(false);
    }

    ~TreeComponentInfo()
    {
      free(v_to_root);
      free(stability);
    }
  };
}
