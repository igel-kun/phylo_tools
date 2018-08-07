
/*
 * this is an implementation of the code of Lengauer & Tarjan, 1979
 * to compute dominators in a DAG
 */

#pragma once

#include "ro_network.hpp"

namespace PT{

  class LSATree{
    /* computation of the LSA tree follows [Lengauer & Tarjan, 1979]
     * it runs in O((n+m)*alpha(n,m)) which is as good as linear time for all intends an purposes
     */
    const Network& N;

    uint32_t* DFS_parent;
    uint32_t* DFS_vertex;
    uint32_t* semi_dominator;
    uint32_t* dominator;
    std::unordered_map<uint32_t, std::vector<uint32_t>> bucket;

    // arrays used in LINK & EVAL
    uint32_t* best_ancestor; //<-- this is called "label" in LT'79
    uint32_t* ancestor;
    uint32_t* size;
    uint32_t* child;

    void initial_DFS(const uint32_t v, uint32_t& dfs_num)
    {
      semi_dominator[v] = dfs_num;
      DFS_vertex[dfs_num++] = v;
      for(uint32_t succ: N[v].children())
        if(semi_dominator[succ] != 0){
          DFS_parent[succ] = v;
          initial_DFS(succ, dfs_num);
        }
    }

    void link(const uint32_t v, const uint32_t w)
    {
      uint32_t s = w;
      while(semi_dominator[best_ancestor[w]] < semi_dominator[best_ancestor[child[s]]]){
        if(size[s] + size[child[child[s]]] >= 2 * size[child[s]]){
          DFS_parent[child[s]] = s;
          child[s] = child[child[s]];
        } else {
          size[child[s]] = size[s];
          s = DFS_parent[s] = child[s];
        }
        best_ancestor[s] = best_ancestor[w];
        size[v] = size[v] + size[w];
        
        if(size[v] < 2 * size[w]) std::swap(s, child[v]);

        while(s != 0){
          DFS_parent[s] = v;
          s = child[s];
        }
      }
    }

    void compress(const uint32_t v)
    {
      if(ancestor[ancestor[v]] != 0){
        compress(ancestor[v]);
        uint32_t& v_anc = ancestor[v];
        if(semi_dominator[best_ancestor[v_anc]] < semi_dominator[best_ancestor[v]])
          best_ancestor[v] = best_ancestor[v_anc];
        v_anc = ancestor[v_anc];
      }
    }

    // this is called "EVAL" in LT'79
    uint32_t ancestor_with_min_semi_dominator(const uint32_t v)
    {
      if(ancestor[v] != 0){
        compress(v);
        if(semi_dominator[best_ancestor[ancestor[v]]] >= semi_dominator[best_ancestor[v]])
          return best_ancestor[v];
        else
          return best_ancestor[ancestor[v]];
      } else return best_ancestor[v];
    }

    void compute_semi_dominator()
    {
      for(uint32_t i = N.num_nodes(); i != 0; --i){
        // step 2
        const uint32_t w = DFS_vertex[i];
        uint32_t& semi_dom_w = semi_dominator[w];
        for(uint32_t parent: N[w].parents()){
          const uint32_t u = semi_dominator[ancestor_with_min_semi_dominator(parent)];
          semi_dom_w = std::min(semi_dom_w, u);
        }
        bucket[DFS_vertex[semi_dom_w]].push_back(w);
        link(DFS_parent[w], w);
        
        // step 3
        std::vector<uint32_t>& parent_bucket = bucket[DFS_parent[w]];
        for(uint32_t v: parent_bucket){
          const uint32_t u = ancestor_with_min_semi_dominator(v);
          dominator[v] = (semi_dominator[u] < semi_dominator[v]) ? u : DFS_parent[w];
        }
        parent_bucket.clear();        
      }
    }

    void compute_dominators()
    {
      for(uint32_t i = 1; i < N.num_nodes(); ++i){
        const uint32_t w = DFS_vertex[i];
        if(dominator[w] != DFS_vertex[semi_dominator[w]])
          dominator[w] = dominator[dominator[w]];
      }
    }

  public:

    LSATree(const Network& _N):
      N(_N),
      DFS_parent((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      DFS_vertex((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      semi_dominator((uint32_t*)calloc(_N.num_nodes(), sizeof(uint32_t))),
      dominator((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      best_ancestor((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      ancestor((uint32_t*)calloc(_N.num_nodes(), sizeof(uint32_t))),
      size((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      child((uint32_t*)calloc(_N.num_nodes(), sizeof(uint32_t)))
    {
      // step 0: initialization
      for(uint32_t v = 0; v < N.num_nodes(); ++v) size[v] = 1;
      for(uint32_t v = 0; v < N.num_nodes(); ++v) best_ancestor[v] = v;
      // step 1: DFS
      uint32_t dfs_num = (uint32_t)-1;
      initial_DFS(N.get_root(), dfs_num);
      assert(dfs_num == N.num_nodes());
      // step 2: compute semi-dominators
      // step 3: indicate (implicitly) dominators
      compute_semi_dominator();
      // step 4: compute dominators (explicitly)
      size[0] = 0;
      compute_dominators();
    }

    ~LSATree()
    {
      free(child);
      free(size);
      free(ancestor);
      free(best_ancestor);

      free(dominator);
      free(semi_dominator);
      free(DFS_vertex);
      free(DFS_parent);
    }

    uint32_t operator[](const uint32_t v) const
    {
      return dominator[v];
    }

  };
}

