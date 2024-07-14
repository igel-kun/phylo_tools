
#pragma once


namespace PT {

  template<StrictPhylogenyType Net, NodeContainerType Nodes, class GammaFunctor, class InheritanceProbFunctor>
  void compute_gammas(const Net& N, const Nodes& nodes_to_save, GammaFunctor&& gamma, InheritanceProbFunctor&& p){
    for(const auto uv: N.edges_postorder()) {
      const auto& [u, v] = uv.as_pair();
      float& current_gamma = gamma(uv);
      float tmp = 1.0f;
      const float p_val = (N.is_leaf(v) && !test(nodes_to_save, v)) ? 0.0f : p(uv);
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
}

