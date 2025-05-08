
#pragma once

#include "brute_force.hpp"

#include "types.hpp"
#include "subsets.hpp"
#include "switchings.hpp"
#include "diversity_avg_tree.hpp"

namespace PT {

  // ===================== diversity (classic formulation) ==========================

  struct _pd_score_classic {
    template<StrictPhylogenyType Net, NodeContainerType Nodes>
    static constexpr void compute_gammas(const Net& N, const Nodes& leaves_to_save, auto& util){
      using Gamma = typename std::remove_reference_t<decltype(util)>::Gamma;
      for(const auto uv: N.edges_postorder()) {
        auto& current_gamma = util.gamma(uv);
        const NodeDesc v = uv.head();
        const Gamma p_val = (N.is_leaf(v) && (not test(leaves_to_save, v))) ? 0 : util.iprob(uv);
        
        Gamma tmp = 1;
        if(!N.is_leaf(v)) {
          for(const auto vw: N.out_edges(v)) {
            const Gamma g_vw = static_cast<Gamma>(util.gamma(vw));
            if(g_vw == 1) {
              tmp = 0;
              break;
            } else tmp *= 1 - static_cast<Gamma>(util.gamma(vw));
          }
          tmp = 1 - tmp;
        } 
        current_gamma = tmp * static_cast<Gamma>(p_val);
      }
    }

    template<StrictPhylogenyType Net, NodeIterableType Nodes>
    double operator()(const Net& N, const Nodes& leaves_to_save, auto& util) const {
      // if we only have some iterable of nodes, we'd better convert it to a set, as we'll need efficient query in compute_gammas
      if constexpr (not NodeContainerType<Nodes>) {
        NodeSet leaf_set{std::begin(leaves_to_save), std::end(leaves_to_save)};
        compute_gammas(N, leaf_set, util);
      } else compute_gammas(N, leaves_to_save, util);
      // NOTE: (note: we're not using std::accumulate since N.edges().end() has different type than it's begin())
      // TODO: in C++23, use std::ranges::fold_left
      double D = 0.0;
      for(const auto e: N.edges()) {
        D += static_cast<double>(util.score(e));
        DEBUG3(std::cout << "collecting score "<<util.score(e)<<" from edge "<<e<<" --- sum is now "<<D<<'\n');
      }
      DEBUG3(std::cout << "final score: "<<D<<'\n');
      return D;
    }
  };

  template<StrictPhylogenyType Net, NodeContainerType Nodes>
  double pd_score_classic(const Net& N, const Nodes& leaves_to_save, auto&& util) {
    return _pd_score_classic{}(N, leaves_to_save, util);
  }

 
  // ===================== diversity (contained-tree formulation) ==========================

  struct _pd_score_ct {
    template<StrictPhylogenyType Net, class EdgeContainer>
    static constexpr auto pd_score_for_switching_wp(const Net& N, const EdgeContainer& active_edges, auto& util) {
      std::pair<double, double> result{0,1};
      DEBUG5(NodeSet seen; std::cout << "\nnew switching\n");
      for(const auto uv: active_edges) {
        DEBUG5(std::cout << uv <<'\n');
        result.first += static_cast<double>(util.weight(uv));
        if(Net::is_reti(uv.head())) result.second *= static_cast<double>(util.iprob(uv));
      }
      DEBUG5(std::cout << "switching has weight "<<result.first<<" & prob "<<result.second<<'\n');
      return result;
    }

    template<StrictPhylogenyType Net, class EdgeContainer>
    static constexpr double pd_score_for_switching(const Net& N, const EdgeContainer& active_edges, auto& util) {
      const auto [weight, prob] = pd_score_for_switching_wp(N, active_edges, util);
      return weight * prob;
    }


    template<StrictPhylogenyType Net, NodeIterableType Nodes>
    double operator()(const Net& N, const Nodes& leaves_to_save, auto&& util) const {
      double result = 0;
      DEBUG4(size_t count = 0);
      for(const auto switching: SwitchingFactory<Net, const Nodes*>{N, leaves_to_save}) {
        result += pd_score_for_switching(N, switching, util);
        DEBUG4(++count);
      }
      DEBUG4(std::cout << count << " switchings; total score: "<<result<<'\n');
      return result;
    }
  };

  template<StrictPhylogenyType Net, NodeContainerType Nodes>
  double pd_score_ct(const Net& N, const Nodes& leaves_to_save, auto&& util) {
    return _pd_score_ct{}(N, leaves_to_save, util);
  }

  // ===================== diversity (contained-tree formulation with DP) ==========================

  // compute the diversity score of a given set
  struct _pd_score_ct_dp {

    // compute score of a given set of leaves by iterating over biconnected components and over switchings of invisible reticulations within each bcc
    template<StrictPhylogenyType Net, NodeIterableType Nodes>
    double operator()(const Net& N, const Nodes& leaves_to_save, auto& util) const {
#warning "TODO: write me!"
    assert(false);
    }
  };

  template<StrictPhylogenyType Net, class UtilityFunctors>
  auto optimize_displayed_tree_diversity(const Net& N, const size_t k, UtilityFunctors&& util, const size_t num_solutions = 1) {
    AveragePDEngine<Net, UtilityFunctors, NoLeafTable> engine(N, std::forward<UtilityFunctors>(util), num_solutions);
    engine.optimize_displayed_tree_diversity(k);
    return engine.leaf_query.accus.at(k);
  }
  
  template<StrictPhylogenyType Net, NodeContainerType Nodes, class UtilityFunctors>
  double pd_score_ct_dp(const Net& N, const Nodes& leaves, UtilityFunctors&& util){
#warning "TODO: write me"
    assert(false);
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

  template<StrictPhylogenyType Net, class PDScore, class UtilityFunctors>
  auto optimize_diversity_brute_force(const Net& N, const size_t k, UtilityFunctors&& util, PDScore&& pd_score, const size_t num_of_solutions = 1) {
    const NodeVec leaves(N.leaves().template to_container<NodeVec>());
    DEBUG3(std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Net::label(x);})) << '\n');
    return mstd::brute_force(k, leaves, num_of_solutions, [&](const auto& S){ return pd_score(N, S, util); });
  }

  template<StrictPhylogenyType Net, class... Args>
  auto optimize_diversity(const Net& N, const size_t k, Args&&... args) {
    // NOTE: for now, we brute-force this
    return optimize_diversity_brute_force(N, k, std::forward<Args>(args)...);
  }
 

}

