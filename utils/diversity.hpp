
#pragma once


namespace PT {

  template<StrictPhylogenyType Net, NodeContainerType Nodes, class GammaFunctor, class InheritanceProbFunctor>
  void compute_gammas(const Net& N, const Nodes& nodes_to_save, GammaFunctor&& gamma, InheritanceProbFunctor&& p){
    for(const auto uv: N.edges_postorder()) {
      float& current_gamma = gamma(uv);
      const NodeDesc v = uv.head();
      const float p_val = (N.is_leaf(v) && !test(nodes_to_save, v)) ? 0.0f : p(uv);
      
      float tmp = 1.0f;
      if(!N.is_leaf(v)) {
        for(const auto vw: N.out_edges(v)) {
          const float g_vw = gamma(vw);
          if(g_vw == 1.0f) {
            tmp = 0.0f;
            break;
          } else tmp *= 1.0f - gamma(vw);
        }
        tmp = 1.0f - tmp;
      } 
      current_gamma = tmp * p_val;
    }
  }

  // return a list of tree-component roots r that have a "private" leaf, that is, each r has a tree-path to some leaf
  template<StrictPhylogenyType Net>
  NodeSet get_comp_roots_with_leaf_path(const Net& N) {
    NodeSet result;
    NodeSet has_leaf_path;
    for(const auto uv: N.edges_postorder()) {
      const auto [u,v] = uv.as_pair();
      switch(Net::type_of(v)) {
        default:
          assert(false);
        case NODE_TYPE_LEAF:
          append(has_leaf_path, {u, v});
          break;
        case NODE_TYPE_INTERNAL_TREE:
          if(test(has_leaf_path, v))
            append(has_leaf_path, u);
          break;
        case NODE_TYPE_INTERNAL_RETI:
          if(test(has_leaf_path, v))
            append(result, v);
      }
    }
    return result;
  }

  void apply_for_all_subsets(auto it, const auto end_it, NodeSet& S, const unsigned int subset_size, const auto& f) {
    while(it != end_it){
      const NodeDesc u = *it;
      append(S, u);
      if(subset_size > 1)
        apply_for_all_subsets(std::next(it), end_it, S, subset_size - 1, f);
      else f(S);
      erase(S, u);
      ++it;
    }
  }

  void apply_for_all_subsets(const auto& leaves, const unsigned int subset_size, const auto& f) {
    NodeSet S;
    apply_for_all_subsets(leaves.begin(), leaves.end(), S, subset_size, f);
  }

  template<StrictPhylogenyType Net>
  NodeSet optimize_diversity_brute_force(const Net& N, const size_t k) {
    float max_score = 0;
    NodeSet max_set;
#warning "TODO: make a subset-iterator"
    apply_for_all_subsets(L, k, [&](const auto& S){
        const float score = pd_score(N, inheritence_probs, S);
        if(score > max_score) { max_score = score; max_set = S; }; });
  }


  template<StrictPhylogenyType Net>
  NodeSet get_optimal_leaves_to_save(const Net& N, const size_t k) {
  }
}

