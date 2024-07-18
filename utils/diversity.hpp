
#pragma once


namespace PT {

  template<StrictPhylogenyType Net, NodeContainerType Nodes, class UtilityFunctors>
  void compute_gammas(const Net& N, const Nodes& nodes_to_save, UtilityFunctors&& f){
    using Gamma = typename std::remove_reference_t<UtilityFunctors>::Gamma;
    for(const auto uv: N.edges_postorder()) {
      auto& current_gamma = f.gamma(uv);
      const NodeDesc v = uv.head();
      const float p_val = (N.is_leaf(v) && !test(nodes_to_save, v)) ? 0.0f : f.iprob(uv);
      
      Gamma tmp = 1.0f;
      if(!N.is_leaf(v)) {
        for(const auto vw: N.out_edges(v)) {
          const auto g_vw = f.gamma(vw);
          if(g_vw == 1.0f) {
            tmp = 0.0f;
            break;
          } else tmp *= 1.0f - f.gamma(vw);
        }
        tmp = 1.0f - tmp;
      } 
      current_gamma = tmp * p_val;
    }
  }

  template<StrictPhylogenyType Net, NodeContainerType Nodes, class UtilityFunctors>
  double pd_score(const Net& N, const Nodes& nodes_to_save, UtilityFunctors&& f) {
    compute_gammas(N, nodes_to_save, std::forward<UtilityFunctors>(f));
    // NOTE: (note: we're not using std::accumulate since N.edges().end() has different type than it's begin())
    // TODO: in C++23, use std::ranges::fold_left
    double D = 0.0;
    for(const auto e: N.edges())
      D += static_cast<double>(f.score(e));
    return D;
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

  template<StrictPhylogenyType Net, class UtilityFunctors>
  auto optimize_diversity_brute_force(const Net& N, const size_t k, UtilityFunctors&& f) {
    std::pair<NodeSet, double> max;
    const auto L = N.leaves();
    std::cout << "leaves: "<<L << '\n';
    std::cout << mstd::type_name<decltype(L)>() << '\n';
    //const NodeSet leaves = L.template to_container<NodeSet>();
    const NodeSet leaves = L;
    std::cout << "N = "<<N<<'\n';
    std::cout << "testing all size-"<<k<<" subsets of "<<leaves<<'\n';
#warning "TODO: make a subset-iterator"
    apply_for_all_subsets(leaves, k, [&](const auto& S){
        const auto score = pd_score(N, S, f);
        if(score > max.second) max = {S, score}; });
    return max;
  }

  template<StrictPhylogenyType Net, class UtilityFunctors>
  auto optimize_diversity(const Net& N, const size_t k, UtilityFunctors&& f) {
    // NOTE: for now, we brute-force this
    return optimize_diversity_brute_force(N, k, std::forward<UtilityFunctors>(f));
  }
}

