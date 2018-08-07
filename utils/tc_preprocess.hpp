
#pragma once

namespace PT {
  class TC_Preprocessor
  {
    Network& N;
    LSATree& lsa;
    ComponentRootInfo& cr_info;

    // map each component root r to any leaf that r is stable on
    std::unordered_map<uint32_t, uint32_t> leaf_stability;


    // compute the leaf_stability mapping by scanning the LSA tree bottom-up
    void compute_leaf_stability()
    {
      std::queuek<uint32_t> next_lsa;
      // add dominators of all leaves
      for(const uint32_t& l: N.get_leaves()){
        const uint32_t l_dom = lsa[l];
        leaf_stability[l_dom] = l;
        next_lsa.push(l_dom);
      }
      while(!next_lsa.empty()){
        const uint32_t u = next_lsa.front();
        const uint32_t u_dom = lsa[u];
        next_lsa.pop();

        // if u_dom is not already stable on a leaf, register his stability on the leaf that u is stable on
        const auto emplace_result = leaf_stability.emplace(u_dom);
        if(emplace_result.second){
          emplace_result.first->second = leaf_stability[u];
          next_lsa.push(u_dom);
        }
      }
    }

  public:
    
    TC_Preprocessor(Network& _N, LSATree& _lsa, ComponentRoots& _croots):
      N(_N),
      lsa(_lsa),
      croots(_croots),
    {
      compute_leaf_stability();
    }

    void apply()
    {
      // step 1: get the component roots in the order of processing
      std::stack<uint32_t> to_process;
      for(const uint32_t& u: croots.get_comp_roots_preordered())
        to_process.push(u);

      // step 2: for each component root, check if it is stable on a leaf and, if so,
      assert(false); <-- CONTINUE HERE     
    }
  };
}

