
#pragma once

#include "brute_force.hpp"

#include "types.hpp"
#include "subsets.hpp"
#include "switchings.hpp"

namespace PT {

  // ===================== diversity (classic formulation) ==========================

  struct _pd_score_classic {
    template<StrictPhylogenyType Net, NodeContainerType Nodes, class UtilityFunctors>
    static constexpr void compute_gammas(const Net& N, const Nodes& leaves_to_save, UtilityFunctors&& f){
      using Gamma = typename std::remove_reference_t<UtilityFunctors>::Gamma;
      for(const auto uv: N.edges_postorder()) {
        auto& current_gamma = f.gamma(uv);
        const NodeDesc v = uv.head();
        const Gamma p_val = (N.is_leaf(v) && (not test(leaves_to_save, v))) ? 0 : f.iprob(uv);
        
        Gamma tmp = 1;
        if(!N.is_leaf(v)) {
          for(const auto vw: N.out_edges(v)) {
            const Gamma g_vw = static_cast<Gamma>(f.gamma(vw));
            if(g_vw == 1) {
              tmp = 0;
              break;
            } else tmp *= 1 - static_cast<Gamma>(f.gamma(vw));
          }
          tmp = 1 - tmp;
        } 
        current_gamma = tmp * static_cast<Gamma>(p_val);
      }
    }

    template<StrictPhylogenyType Net, NodeIterableType Nodes, class UtilityFunctors>
    double operator()(const Net& N, const Nodes& leaves_to_save, UtilityFunctors&& f) const {
      // if we only have some iterable of nodes, we'd better convert it to a set, as we'll need efficient query in compute_gammas
      if constexpr (not NodeContainerType<Nodes>) {
        NodeSet leaf_set{std::begin(leaves_to_save), std::end(leaves_to_save)};
        compute_gammas(N, leaf_set, std::forward<UtilityFunctors>(f));
      } else compute_gammas(N, leaves_to_save, std::forward<UtilityFunctors>(f));
      // NOTE: (note: we're not using std::accumulate since N.edges().end() has different type than it's begin())
      // TODO: in C++23, use std::ranges::fold_left
      double D = 0.0;
      for(const auto e: N.edges())
        D += static_cast<double>(f.score(e));
      return D;
    }
  };

  template<StrictPhylogenyType Net, NodeContainerType Nodes, class UtilityFunctors>
  double pd_score_classic(const Net& N, const Nodes& leaves_to_save, UtilityFunctors&& f) {
    return _pd_score_classic{}(N, leaves_to_save, std::forward<UtilityFunctors>(f));
  }

 
  // ===================== diversity (contained-tree formulation) ==========================

  struct _pd_score_ct {
    template<StrictPhylogenyType Net, NodeIterableType Nodes, class UtilityFunctors, class EdgeContainer>
    static constexpr double pd_score_for_switching(const Net& N, const Nodes& _saved_nodes, const EdgeContainer& active_edges, UtilityFunctors&& f) {
      NodeSet saved_nodes{std::begin(_saved_nodes), std::end(_saved_nodes)};
      double switching_weight = 0;
      double switching_prob = 1;
      // NOTE: the switching-iter guarantees us to present the edges in pre-order, so if we reverse the order, it'll be a post-order
      for(const auto& uv: std::ranges::reverse_view{active_edges}) {
        const auto [u, v] = uv.as_pair();
        if(test(saved_nodes, v)) {
          append(saved_nodes, u);
          // update weight and prob
          switching_weight += static_cast<double>(f.weight(uv));
          if(Net::is_reti(v))
            switching_prob *= static_cast<double>(f.iprob(uv));
        }
      }
      return switching_weight * switching_prob;
    }

    template<StrictPhylogenyType Net, NodeIterableType Nodes, class UtilityFunctors>
    double operator()(const Net& N, const Nodes& leaves_to_save, UtilityFunctors&& f) const {
      double result = 0;
      for(const auto s: SwitchingFactory<Net>{N}) {
        result += pd_score_for_switching(N, leaves_to_save, s, f);
      }
      return result;
    }
  };

  template<StrictPhylogenyType Net, NodeContainerType Nodes, class UtilityFunctors>
  double pd_score_ct(const Net& N, const Nodes& leaves_to_save, UtilityFunctors&& f) {
    return _pd_score_ct{}(N, leaves_to_save, std::forward<UtilityFunctors>(f));
  }


  // ====================== general convenience ================================
  
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

  template<StrictPhylogenyType Net, class UtilityFunctors, class PDScore>
  auto optimize_diversity_brute_force(const Net& N, const size_t k, UtilityFunctors&& f, PDScore&& pd_score) {
    //std::pair<NodeSet, double> max;
    const auto L = N.leaves();
    //const NodeSet leaves = L.template to_container<NodeSet>();
    const NodeSet leaves = L;
    std::cout << leaves.size() << " leaves: "<<leaves<<'\n';
    //std::cout << "N = "<<N<<'\n';
    /*
    for(const auto S: mstd::BoundedSubsetFactory<NodeSet>{leaves, k}) {
      const auto score = pd_score(N, S, std::forward<UtilityFunctors>(f));
      if(score > max.second) max = {S, score};
    }
    return max;
    */
    return mstd::brute_force(k, leaves, [&](const auto& S){ return pd_score(N, S, f); });
  }

  template<StrictPhylogenyType Net, class... Args>
  auto optimize_diversity(const Net& N, const size_t k, Args&&... args) {
    // NOTE: for now, we brute-force this
    return optimize_diversity_classic_brute_force(N, k, std::forward<Args>(args)...);
  }
 

}

