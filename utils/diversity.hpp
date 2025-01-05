
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
      for(const auto e: N.edges()) {
        D += static_cast<double>(f.score(e));
        DEBUG3(std::cout << "collecting score "<<f.score(e)<<" from edge "<<e<<" --- sum is now "<<D<<'\n');
      }
      DEBUG3(std::cout << "final score: "<<D<<'\n');
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
      
      DEBUG5(NodeSet seen; std::cout << "\nnew switching\n");
      // NOTE: the switching-iter guarantees us to present the edges in post-order
      for(const auto& uv: active_edges) {
        const auto [u, v] = uv.as_pair();
        DEBUG5(std::cout << uv <<'\n');
        DEBUG5(assert(seen.emplace(v).second));
        if(test(saved_nodes, v)) {
          append(saved_nodes, u);
          // update weight and prob
          switching_weight += static_cast<double>(f.weight(uv));
        }
        if(Net::is_reti(v))
          switching_prob *= static_cast<double>(f.iprob(uv));
      }
      DEBUG5(std::cout << "switching has weight "<<switching_weight<<" & prob "<<switching_prob<<'\n');
      return switching_weight * switching_prob;
    }

    template<StrictPhylogenyType Net, NodeIterableType Nodes, class UtilityFunctors>
    double operator()(const Net& N, const Nodes& leaves_to_save, UtilityFunctors&& f) const {
      double result = 0;
      DEBUG4(size_t count = 0);
      auto switchings = SwitchingFactory<Net, const Nodes*>{N, leaves_to_save};
      auto it = std::move(switchings).begin();
      while(it.is_valid()) {
        result += pd_score_for_switching(N, leaves_to_save, *it, f);
        DEBUG4(++count);
        ++it;
      }
      DEBUG4(std::cout << count << " switchings; total score: "<<result<<'\n');
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
    const NodeVec leaves(N.leaves().template to_container<NodeVec>());
    std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Net::label(x);})) << '\n';
    return mstd::brute_force(k, leaves, [&](const auto& S){ return pd_score(N, S, f); });
  }

  template<StrictPhylogenyType Net, class... Args>
  auto optimize_diversity(const Net& N, const size_t k, Args&&... args) {
    // NOTE: for now, we brute-force this
    return optimize_diversity_classic_brute_force(N, k, std::forward<Args>(args)...);
  }
 

}

