
#pragma once

#include "solution_accu.hpp"
#include "brute_force.hpp"

#include "types.hpp"
#include "subsets.hpp"
#include "switchings.hpp"
#include "diversity_avg_tree.hpp"

namespace PT {
  using namespace std::literals;

  // if you want to use diversity modules on your network, you can make the networks EdgeData
  // inherit from the following
  // alternatively, you can provide 2 functors returning inheritence probs and weights, or
  // a functor returning a pair (iprob, weight)
  template<class Arithmetic = double>
  struct pd_edge_data {
    Arithmetic iprob = 1; // inheritance probabilities
    Arithmetic weight = 0;

    pd_edge_data() = default;

    // construct from a given string that's been read from the input file
    pd_edge_data(const std::string_view in) {
      auto iter = mstd::tokenize(in, ",;:"sv).begin();
      while(iter && (*iter == "")) ++iter;
      if(iter) weight = std::stod(*iter);
      while(++iter && (*iter == ""));
      if(iter)
        if(not mstd::try_reading_number<Arithmetic>(*iter, iprob))
          iprob = 1;
    }

    friend std::ostream& operator<<(std::ostream& os, const pd_edge_data& ed) {
      return os << "{inh: "<<ed.iprob<<", w: "<<ed.weight<<'}';
    }
  };
  template<class Arithmetic = double>
  struct pd_edge_data_with_gamma:
    public pd_edge_data<Arithmetic>
  {
    using Parent = pd_edge_data<Arithmetic>;
    Arithmetic gamma = 0;
    INHERIT_ALL_CONSTRUCTORS(pd_edge_data_with_gamma, Parent);
  };

  template<class T> concept has_iprob = requires(T t) { t.iprob; };
  template<class T> concept has_weight = requires(T t) { t.weight; };
  template<class T> concept has_gamma = requires(T t) { t.gamma; };
  template<class T, class... Args> concept has_iprob_func = requires(T t, Args... args) { t.iprob(args...); };
  template<class T, class... Args> concept has_weight_func = requires(T t, Args... args) { t.weight(args...); };
  template<class T, class... Args> concept has_gamma_func = requires(T t, Args... args) { t.gamma(args...); };

  // per default, get iprob, weight, and gamma from the edge-data
  struct GetEdgeData { const auto& operator()(const auto& uv) const { return uv.data(); } };

  template<class EdgeData> requires (has_iprob<EdgeData> or has_iprob_func<EdgeData>)
  using DefaultIProb = GetEdgeData;

  template<class EdgeData> requires (has_weight<EdgeData> or has_weight_func<EdgeData>)
  using DefaultWeight = GetEdgeData;
  
  template<class EdgeData> 
  struct DefaultGamma {
    static constexpr bool external_gamma = (has_gamma<EdgeData> or has_gamma_func<EdgeData>);
    using GammaMap = std::conditional_t<external_gamma, std::monostate, HashMap<NodePair, double>>;
    [[ no_unique_address ]] GammaMap gamma_map;

    auto& operator()(const Edge<EdgeData>& uv) const {
      if constexpr (external_gamma) {
        return uv.data();
      } else return gamma_map[uv.as_pair()];
    }
  };

  // The default FuncWeight will try to grab the weight off of the EdgeData.
  // If you don't want that, then you'll have to provide a function mapping edges the weight weight
  template<class EdgeData, class FuncWeight = DefaultWeight<EdgeData>>
    requires (std::is_invocable_v<FuncWeight, const Edge<EdgeData>&>)
  struct pd_score_util_w {
    using weight_result = std::invoke_result_t<FuncWeight, const Edge<EdgeData>&>;
    static_assert((std::is_arithmetic_v<weight_result>) or (has_weight<weight_result>) or (has_weight_func<weight_result>));

    [[ no_unique_address ]] FuncWeight _weight;

    auto weight(const auto& uv) const {
      if constexpr (std::is_arithmetic_v<weight_result>) {
        return _weight(uv);
      } else if constexpr (has_weight<weight_result>) {
        return _weight(uv).weight;
      } else if constexpr (has_weight_func<weight_result, EdgeData>) {
        return _weight(uv).weight();
      }
    }
    using Weight = std::remove_cvref_t<decltype(std::declval<pd_score_util_w>().weight(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Weight>);
  };

  template<class EdgeData, class FuncIProb = DefaultIProb<EdgeData>>
    requires (std::is_invocable_v<FuncIProb, const Edge<EdgeData>&>)
  struct pd_score_util_p {
    using iprob_result = std::invoke_result_t<FuncIProb, const Edge<EdgeData>&>;
    static_assert((std::is_arithmetic_v<iprob_result>) or (has_iprob<iprob_result>) or (has_iprob_func<iprob_result>));

    [[ no_unique_address ]] FuncIProb _iprob;

    auto iprob(const auto& uv) const {
      if constexpr (std::is_arithmetic_v<iprob_result>) {
        return _iprob(uv);
      } else if constexpr (has_iprob<iprob_result>) {
        return _iprob(uv).iprob;
      } else if constexpr (has_iprob_func<iprob_result, EdgeData>) {
        return _iprob(uv).iprob();
      }
    }
    using Probability = std::remove_cvref_t<decltype(std::declval<pd_score_util_p>().iprob(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Probability>);
  };

  template<class EdgeData, class FuncGamma = DefaultGamma<EdgeData>>
    requires (std::is_invocable_v<FuncGamma, const Edge<EdgeData>&>)
  struct pd_score_util_g {
    using gamma_result = std::invoke_result_t<FuncGamma, const Edge<EdgeData>&>;
    static_assert((std::is_arithmetic_v<gamma_result>) or (has_gamma<gamma_result>) or (has_gamma_func<gamma_result>));

    [[ no_unique_address ]] FuncGamma _gamma;

    auto& gamma(const auto& uv) const {
      if constexpr (std::is_arithmetic_v<gamma_result>) {
        return _gamma(uv);
      } else if constexpr (has_gamma<gamma_result>) {
        return _gamma(uv).gamma;
      } else if constexpr (has_gamma_func<gamma_result, EdgeData>) {
        return _gamma(uv).gamma();
      }
    }
    using Gamma = std::remove_cvref_t<decltype(std::declval<pd_score_util_g>().gamma(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Gamma>);
  };

  template<class EdgeData, class FuncWeight = DefaultWeight<EdgeData>, class FuncIProb = DefaultIProb<EdgeData>>
  struct pd_score_util_wp: public pd_score_util_w<EdgeData, FuncWeight>, public pd_score_util_p<EdgeData, FuncIProb> {};
  template<class EdgeData, class FuncWeight = DefaultWeight<EdgeData>, class FuncIProb = DefaultIProb<EdgeData>, class FuncGamma = DefaultGamma<EdgeData>>
  struct pd_score_util_wpg: public pd_score_util_wp<EdgeData, FuncWeight, FuncIProb>, public pd_score_util_g<EdgeData, FuncGamma> {};


  // ===================== diversity modules ==========================
  // a PD score module can be called with a network and a leaf-set and returns the diversity score of that leaf-set
  // it can also be called with a network and a number k and computes sets of k leaves in the network maximizing the score
  // you can also provide functors iprob and weight to return inheritence probabilities and weights if they are not
  // stored directly in the edges

  // ===================== phylogenetic network diversity ==========================
  template<StrictPhylogenyType Network,
    class FuncIProb = DefaultIProb<EdgeDataOf<Network>>,
    class FuncWeight = DefaultWeight<EdgeDataOf<Network>>,
    class FuncGamma = DefaultGamma<EdgeDataOf<Network>>>
  struct pd_network_diversity:
    public pd_score_util_wpg<typename Network::EdgeData, FuncIProb, FuncWeight, FuncGamma>
  {
    using Util = pd_score_util_wpg<typename Network::EdgeData, FuncIProb, FuncWeight, FuncGamma>;
    using Gamma = typename Util::Gamma;
    using Util::iprob;
    using Util::weight;
    using Util::gamma;

    using EdgeData = typename Network::EdgeData;
    
    template<NodeContainerType Nodes>
    constexpr void compute_gammas(const Network& N, const Nodes& leaves_to_save) const {
      for(const auto uv: N.edges_postorder()) {
        auto& current_gamma = gamma(uv);
        const NodeDesc v = uv.head();
        const Gamma p_val = (N.is_leaf(v) && (not test(leaves_to_save, v))) ? 0 : iprob(uv);
        
        Gamma tmp = 1;
        if(!N.is_leaf(v)) {
          for(const auto vw: N.out_edges(v)) {
            const Gamma g_vw = gamma(vw);
            if(g_vw == 1) {
              tmp = 0;
              break;
            } else tmp *= 1 - gamma(vw);
          }
          tmp = 1 - tmp;
        } 
        current_gamma = tmp * p_val;
      }
    }

    template<NodeIterableType Nodes>
    auto score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) const {
      // if we only have some iterable of nodes, we'd better convert it to a set, as we'll need efficient query in compute_gammas
      if constexpr (not NodeContainerType<Nodes>) {
        NodeSet leaf_set{std::begin(leaves_to_save), std::end(leaves_to_save)};
        compute_gammas(N, leaf_set);
      } else compute_gammas(N, leaves_to_save);
      // NOTE: (note: we're not using std::accumulate since N.edges().end() has different type than it's begin())
      // TODO: in C++23, use std::ranges::fold_left
      Gamma D = 0.0;
      for(const auto e: N.edges()) {
        const auto score = weight(e) * gamma(e);
        D += score;
        DEBUG3(std::cout << "collecting score "<<score<<" from edge "<<e<<" --- sum is now "<<D<<'\n');
      }
      DEBUG3(std::cout << "final score: "<<D<<'\n');
      return D;
    }

    template<NodeIterableType Nodes>
    auto operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    auto operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      const NodeVec leaves(N.leaves().template to_container<NodeVec>());
      DEBUG3(std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Network::label(x);})) << '\n');
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return score_for_leaf_set(N, S); });
    }

  };
 
  // ===================== diversity (average contained-tree formulation, brute force) ==========================
  template<StrictPhylogenyType Network,
    class FuncIProb = DefaultIProb<EdgeDataOf<Network>>,
    class FuncWeight = DefaultWeight<EdgeDataOf<Network>>>
  struct pd_average_tree:
    public pd_score_util_wp<typename Network::EdgeData, FuncIProb, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wp<EdgeData, FuncIProb, FuncWeight>;
    using Util::iprob;
    using Util::weight;
    using typename Util::Weight;
    using typename Util::Probability;
    using WeightAndProb = std::pair<Weight, Probability>;

    // return weight and probability of the given switching
    template<class EdgeContainer>
    constexpr auto score_for_switching_wp(const Network& N, const EdgeContainer& active_edges) const {
      WeightAndProb result{0,1};
      DEBUG5(std::cout << "\nnew switching\n");
      for(const auto uv: active_edges) {
        DEBUG5(std::cout << uv <<'\n');
        result.first += weight(uv);
        if(Network::is_reti(uv.head()))
          result.second *= iprob(uv);
      }
      DEBUG5(std::cout << "switching has weight "<<result.first<<" & prob "<<result.second<<'\n');
      return result;
    }

    template<class EdgeContainer>
    constexpr auto score_for_switching(const Network& N, const EdgeContainer& active_edges) const {
      const auto [_weight, _prob] = score_for_switching_wp(N, active_edges);
      return _weight * _prob;
    }
    
    template<NodeIterableType Nodes>
    constexpr auto score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) const {
      Weight result = 0;
      DEBUG4(size_t count = 0);
      for(const auto switching: SwitchingFactory<Network, const Nodes*>{N, leaves_to_save}) {
        result += score_for_switching(N, switching);
        DEBUG4(++count);
      }
      DEBUG4(std::cout << count << " switchings; total score: "<<result<<'\n');
      return result;
    }

    template<NodeIterableType Nodes>
    auto operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    auto operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      const NodeVec leaves(N.leaves().template to_container<NodeVec>());
      DEBUG3(std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Network::label(x);})) << '\n');
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return score_for_leaf_set(N, S); });
    }

  };


  // ===================== diversity (average contained-tree formulation, DP) ==========================
  template<StrictPhylogenyType Network,
    class FuncIProb = DefaultIProb<EdgeDataOf<Network>>,
    class FuncWeight = DefaultWeight<EdgeDataOf<Network>>>
  struct pd_average_tree_DP:
    public pd_average_tree<Network, FuncIProb, FuncWeight>
  {
    using Parent = pd_average_tree<Network, FuncIProb, FuncWeight>;
    using Util = typename Parent::Util;
    using Weight = typename Util::Weight;

    // compute score of a given set of leaves by iterating over biconnected components and over switchings of invisible reticulations within each bcc
    template<NodeIterableType Nodes>
    auto operator()(const Network& N, const Nodes& leaves_to_save) const {
      throw mstd::Unimplemented("TODO: later");
      return Weight{0};
    }

    // return the best num_solutions solutions of size k for N
    auto operator()(const Network& N, const size_t k, const size_t num_solutions = 1) const {
      return optimize_displayed_tree_diversity_level(N, k, static_cast<const Util&>(*this), num_solutions);
      //AveragePDEngine<Network, Util, NoLeafTable<Weight>> engine(N, static_cast<const Util&>(*this), num_solutions);
      //engine.optimize_displayed_tree_diversity(k);
      //return engine.score_map.leaf_table.accus.at(k);
    }
  };
  

   // ===================== phylogenetic tree diversity ==========================
  template<StrictPhylogenyType Network, class FuncWeight = DefaultWeight<EdgeDataOf<Network>>>
  struct pd_tree_diversity:
    public pd_score_util_w<EdgeDataOf<Network>, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using Weight = typename Util::Weight;
    using Util::weight;

    template<NodeIterableType Nodes>
    auto score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) const {
      // use a reverse DFS from the leaves to save upwards
      using UpwardsDFS = Traversal<preorder | all_edge_traversal | reverse_traversal, Network, const Nodes*>;
      Weight w = 0;
      for(const auto uv: UpwardsDFS(leaves_to_save))
        w += weight(uv);
      return w;
    }

    template<NodeIterableType Nodes>
    auto operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    // to optimize the Tree-PD score for k leaves, we'll greedily take the heaviest leaf each time
    // to do this efficiently, we precompute the weight of a heaviest path below each node and the starting outgoing edge
    // for a node x, this information needs to be updated only if x is part of a path that's just been taken,
    // but this can happen at most degree(x) many times; thus the algorithm runs in linear time
    auto operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      TreeDiversity<Network, Util, NoLeafTable<Weight>> engine(N, static_cast<const Util&>(*this), num_solutions);
      engine.optimize_diversity(k);
      return engine.get_root_table(k).at(k);
    }

  };

   // ===================== network fair proportion index ==========================
  template<StrictPhylogenyType Network, class FuncWeight = DefaultWeight<EdgeDataOf<Network>>>
  struct pd_fair_proportion:
    public pd_score_util_wp<EdgeDataOf<Network>, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wp<EdgeData, FuncWeight>;
    using typename Util::Weight;
    using typename Util::Probability;
    using Util::weight;
    using Util::iprob;
    using ProbWeight = std::pair<Probability, Weight>;

    // cache the expected number of descendants
    mutable NodeMap<Weight> exp_num_descendants;
    // cache the expected modified pathlength of a path from x to y at index [y][x], as well as the probability of such a path
    mutable NodeMap<ProbWeight> expected_modified_length;
    
    // return the expected number of descendants of a node, in a switching drawn according to iprob
    // To this end, iterate over all maximal paths starting in x, summing their probability
    Weight expected_number_of_descendants(const NodeDesc x) const {
      const auto [iter, success] = mstd::append(exp_num_descendants, x, 0);
      if(success) {
        if(not Network::is_leaf(x)) {
          for(const auto& y: Network::children(x))
            iter->second += expected_number_of_descendants(y) * iprob(y);
        } else iter->second = 1; // leaves have 1 expected leaf below them
      }
      return iter->second;
    }

    Weight modified_weight(const auto& x) const { return weight(x) / expected_number_of_descendants(x); }
    // score for a single leaf
    // (1) for a leaf a and a root-a-path p and an edge e on p, the NFI-score of e is
    //    the weight of e divided by the expected number of descendants in a random switching
    //    (drawn according to the inheritence probabilities).
    // (2) then, the NFI-score of p is the sum over all edge e on p of their NFI-score
    // (3) then, the NFI-score of a is the expected NFI-score of the root-a-path in a switching drawn at random,
    //    in other words, its the sum over all root-a-paths, of the probability of that path times the NFI-score of that path
    //
    // Note that the expected number of descendants of a node does not depend on the node for whom we compute the score,
    // so the weight of e divided by this quantity is constant for all edges and we consider that the "modified weight".
    //
    // Then, the NFI-score of a is the expected modified weight of a root-a-path drawn at random according to inh'prob's,
    // sum_{path p ending in a} prob(p) * mod_weight(p)
    // where
    // prob(p) = prod_{e on p} prob(e)
    // mod_weight(p) = sum_{e on p} mod_weight(e)
    //
    // To compute this, we can compute top-down for all nodes x the values
    // 1. prob(path ending in x) = sum_{path p ending in x} prob(p)
    // 2. NFI(x) = sum_{path p ending in x} prob(p) * mod_weight(p)
    //
    // so, if node z has parents x and y, then we can compute:
    // 1. prob(path ending in z) = prob(path ending in x) * prob(xz) + prob(path ending in y) * prob(yz)
    // since no path ends in both x and y, the events are independent, so the probabilities add up
    // 2. NFI(z) = sum_{path p ending in z} prob(p) * mod_weight(p)
    //           = sum_{path p ending in x} prob(p) * prob(xz) * (mod_weight(p) + mod_weight(xz))
    //            +sum_{path p ending in y} prob(p) * prob(yz) * (mod_weight(p) + mod_weight(yz))
    //           = prob(xz) * (NFI(x) + prob(path ending in x) * mod_weight(xz))
    //            +prob(yz) * (NFI(y) + prob(path ending in y) * mod_weight(yz))
    // This can be computed top-down
    ProbWeight& expected_modified_length_to(const Network& N, const NodeDesc to) const {
      const auto [iter, success] = mstd::append(expected_modified_length, to, 1, 0);
      if(success) {
        auto& [to_path_prob, to_NFI] = iter->second;
        for(const auto& x: Network::parents(to)) {
          DEBUG6(std::cout << "iprob("<<NodeDesc{x}<<") = "<<iprob(x)<<"\t\t modified_weight("<<NodeDesc{x}<<") = "<<modified_weight(x)<<'\n');
          auto& [x_path_prob, x_NFI] = expected_modified_length_to(N, x);
          const Probability x_to_prob = iprob(x);
          // NOTE: the events that a path comes from one neighbor or the other are INDEPENDENT (no path enters from both parents)
          //    and the prob's can thus be summed
          to_path_prob += (x_path_prob * x_to_prob);
          // NOTE: to get the expected modified length of a root-'to'-path
          //    we add the new length times x_path_prob to it and multiply everything by iprob(y).
          to_NFI += (modified_weight(x) * x_path_prob + x_NFI) * x_to_prob;
        }
        DEBUG6(std::cout << "found that "<<to<<" has NFI "<<to_NFI<<" with path-probability "<<to_path_prob<<'\n');
      }
      return iter->second;
    }

    Weight operator()(const Network& N, const NodeDesc leaf_to_save) const {
      return expected_modified_length_to(N, leaf_to_save).second;
    }

    template<NodeIterableType Nodes>
    Weight score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) const {
      return std::ranges::fold_left(leaves_to_save | std::ranges::views::transform([&](const NodeDesc x){ return operator()(N, x);}), Weight{0});
    }

    template<NodeIterableType Nodes>
    auto operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    // to optimize the Tree-PD score for k leaves, we'll greedily take the heaviest leaf each time
    auto operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      mstd::SolutionAccumulator<NodeVec, Weight> result(num_solutions);
      NodeVec leaves = N.leaves().to_container();
      std::ranges::sort(leaves, [&](const NodeDesc x, const NodeDesc y){ return expected_modified_length_to(N, x) > expected_modified_length_to(N, y); });
      leaves.resize(k);
      result.add(leaves, operator()(N, leaves));
      return result;
    }

  };


}

