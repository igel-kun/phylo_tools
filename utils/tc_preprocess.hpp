
#pragma once

#include<stack>

namespace PT {

  template<class _Network>
  class TC_Preprocessor
  {
    using Network = _Network;

    Network& N;
    LSATree& lsa;
    ComponentRootInfo& cr_info;

    // map each component root r to any leaf that r is stable on
    std::unordered_map<Node, Node> leaf_stability;


    // compute the leaf_stability mapping by scanning the LSA tree bottom-up
    void compute_leaf_stability()
    {
      std::queue<Node> next_lsa;
      // add dominators of all leaves
      for(const Node l: N.get_leaves()){
        const Node l_dom = lsa[l];
        leaf_stability[l_dom] = l;
        next_lsa.push(l_dom);
      }
      while(!next_lsa.empty()){
        const Node u = next_lsa.front();
        const Node u_dom = lsa[u];
        next_lsa.pop();

        // if u_dom is not already stable on a leaf, register his stability on the leaf that u is stable on
        const auto emplace_result = leaf_stability.emplace(u_dom, leaf_stability.at(u));
        if(emplace_result.second){
          emplace_result.first->second = leaf_stability[u];
          next_lsa.push(u_dom);
        }
      }
    }

  public:
    
    TC_Preprocessor(Network& _N, LSATree& _lsa, ComponentRootInfo& _cr_info):
      N(_N),
      lsa(_lsa),
      cr_info(_cr_info)
    {
      compute_leaf_stability();
    }

    void apply()
    {
      // step 1: get the component roots in the order of processing
      std::stack<Node> to_process;
      for(const auto& cr: cr_info.get_comp_roots_preordered())
        to_process.push(cr);

      while(!to_process.empty()){
        std::cout << to_process.top() << " ";
        to_process.pop();
      }
      std::cout << std::endl;

      // step 2: for each component root, check if it is stable on a leaf and, if so,
#error CONTINUE HERE
    }
  };
}

