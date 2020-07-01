
/*
 * this is an implementation of the code of Lengauer & Tarjan, 1979
 * to compute dominators in a DAG
 */

#pragma once

#include "network.hpp"

namespace PT{

  template<class Network>
  class LSATree{
    /* computation of the LSA tree follows [Lengauer & Tarjan, 1979]
     * it runs in O((n+m)*alpha(n,m)) which is as good as linear time for all intends and purposes
     */
    const Network& N;

    using DFS_num = size_t;

    template<class T>
    using NodeMap = typename Network::template NodeMap<T>;
    using Bucket = NodeMap<std::vector<Node>>;

    NodeMap<Node> DFS_parent; // indicate the parent in the DFS-tree
    NodeMap<Node> dominator; // map each node to its dominator
    NodeMap<DFS_num> semi_dominator; // map each node to the DFS_num of its semi dominator
    RawConsecutiveMap<DFS_num, Node> DFS_vertex; // DFS node order

    Bucket bucket;

    // arrays used in LINK & EVAL
    NodeMap<Node> ancestor, best_ancestor; //<-- this is called "label" in LT'79
    NodeMap<size_t> size;

    void initial_DFS(const Node v, DFS_num& dfs_num)
    {
      semi_dominator[v] = dfs_num;
      DFS_vertex[dfs_num++] = v;
      for(const Node succ: N.children(v))
        if(semi_dominator[succ] == 0){
          DFS_parent[succ] = v;
          initial_DFS(succ, dfs_num);
        }
    }

    void link(const Node v, const Node w, NodeMap<Node>& child)
    {
      Node s = w;
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

    void compress(const Node v)
    {
      if(ancestor[ancestor[v]] != 0){
        compress(ancestor[v]);
        Node& v_anc = ancestor[v];
        if(semi_dominator[best_ancestor[v_anc]] < semi_dominator[best_ancestor[v]])
          best_ancestor[v] = best_ancestor[v_anc];
        v_anc = ancestor[v_anc];
      }
    }

    // this is called "EVAL" in LT'79
    Node ancestor_with_min_semi_dominator(const Node v)
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
      NodeMap<Node> child(N.num_nodes());
      for(DFS_num i = N.num_nodes(); i != 0; --i){
        // step 2
        const Node w = DFS_vertex[i];
        DFS_num& semi_dom_w = semi_dominator[w];
        for(const Node parent: N.parents(w)){
          const DFS_num u = semi_dominator[ancestor_with_min_semi_dominator(parent)];
          semi_dom_w = std::min(semi_dom_w, u);
        }
        bucket[DFS_vertex[semi_dom_w]].push_back(w);
        link(DFS_parent[w], w, child);
        
        // step 3
        std::vector<Node>& parent_bucket = bucket[DFS_parent[w]];
        for(const Node v: parent_bucket){
          const Node u = ancestor_with_min_semi_dominator(v);
          dominator[v] = (semi_dominator[u] < semi_dominator[v]) ? u : DFS_parent[w];
        }
        parent_bucket.clear();        
      }
    }

    void compute_dominators()
    {
      for(DFS_num i = 1; i < N.num_nodes(); ++i){
        const Node w = DFS_vertex[i];
        if(dominator[w] != DFS_vertex[semi_dominator[w]])
          dominator[w] = dominator[dominator[w]];
      }
    }

  public:

    LSATree(const Network& _N):
      N(_N),
      DFS_parent((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      DFS_vertex((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      semi_dominator((uint32_t*)calloc(_N.num_nodes(), sizeof(uint32_t))), // NOTE the calloc
      dominator((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      best_ancestor((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
      ancestor((uint32_t*)calloc(_N.num_nodes(), sizeof(uint32_t))), // NOTE the calloc
      size((uint32_t*)malloc(_N.num_nodes() * sizeof(uint32_t))),
    {
      std::cout << "computing LSA-tree for network with "<<N.num_nodes()<<" nodes & "<<N.num_edges()<<" edges ...\n";
      // step 0: initialization
      for(const Node v: N) {
        size[v] = 1;
        best_ancestor[v] = v;
      }
      // step 1: DFS
      uint32_t dfs_num = 0;
      initial_DFS(N.root(), dfs_num);
      assert(dfs_num == N.num_nodes());
      // step 2: compute semi-dominators
      // step 3: indicate (implicitly) dominators
      compute_semi_dominator();
      // step 4: compute dominators (explicitly)
      size[0] = 0;
      compute_dominators();
    }

    Node operator[](const Node v) const { return dominator[v]; }

  };
}

