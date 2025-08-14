
#pragma once

#include <algorithm> // for max_element

#include "solution_accu.hpp"
#include "brute_force.hpp"

#include "types.hpp"
#include "subsets.hpp"
#include "switchings.hpp"
#include "diversity_avg_tree.hpp"

#ifdef DFSCORO
#include "dfs_coro.hpp"

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

  template<class T> concept has_weight = requires(T t) { t.weight; };
  template<class T> concept has_iprob = requires(T t) { t.iprob; };
  template<class T> concept has_gamma = requires(T t) { t.gamma; };
  template<class T, class... Args> concept has_weight_func = requires(T t, Args... args) { t.weight(args...); };
  template<class T, class... Args> concept has_iprob_func = requires(T t, Args... args) { t.iprob(args...); };
  template<class T, class... Args> concept has_gamma_func = requires(T t, Args... args) { t.gamma(args...); };

  template<class T> concept weight_compatible = has_weight<T> or has_weight_func<T> or std::is_arithmetic_v<T>;
  template<class T> concept iprob_compatible = has_iprob<T> or has_iprob_func<T> or std::is_arithmetic_v<T>;
  template<class T> concept gamma_compatible = has_gamma<T> or has_gamma_func<T> or std::is_arithmetic_v<T>;

  // per default, get iprob, weight, and gamma from the edge-data
  struct GetEdgeData {
    template<class Data>
    auto& operator()(const PT::Edge<Data>& uv) const { return uv.data(); }

    template<class Data>
    auto& operator()(const PT::Adjacency<Data>& uv) const { return uv.data(); }

    template<class Data> requires (mstd::is_arithmetic_v<Data> and std::is_reference_v<Data>)
    auto& operator()(Data data) const { return data; }
  };


  template<class EdgeData> 
  struct DefaultGamma {
    static constexpr bool external_gamma = (has_gamma<EdgeData> or has_gamma_func<EdgeData>);
    using GammaMap = std::conditional_t<external_gamma, std::monostate, HashMap<NodePair, double>>;
    [[ no_unique_address ]] GammaMap gamma_map = {};

    auto& operator()(const Edge<EdgeData>& uv) const {
      if constexpr (external_gamma) {
        return uv.data();
      } else return gamma_map[uv.as_pair()];
    }
    auto& operator()(const Adjacency<EdgeData>& uv) const requires external_gamma {
      return uv.data();
    }
    template<class Data> requires (mstd::is_arithmetic_v<Data> and std::is_reference_v<Data>)
    auto& operator()(Data data) const { return data; }
  };



  // The default FuncWeight will try to grab the weight off of the EdgeData.
  // If you don't want that, then you'll have to provide a function mapping edges the weight weight
  template<class EdgeData, class FuncWeight = GetEdgeData>
    requires (std::is_invocable_v<FuncWeight, const Edge<EdgeData>&>)
  struct pd_score_util_w {
    using weight_result = std::invoke_result_t<FuncWeight, const Edge<EdgeData>&>;
    static_assert((mstd::is_arithmetic_v<weight_result>) or (has_weight<weight_result>) or (has_weight_func<weight_result>));

    [[ no_unique_address ]] FuncWeight _weight = {};

    decltype(auto) weight(const auto& uv) const {
      if constexpr (mstd::is_arithmetic_v<weight_result>) {
        return _weight(uv);
      } else if constexpr (has_weight<weight_result>) {
        return _weight(uv).weight;
      } else if constexpr (has_weight_func<weight_result, EdgeData>) {
        return _weight(uv).weight();
      }
    }
    decltype(auto) operator()(const auto& uv) const { return weight(uv); }

    using Weight = std::remove_cvref_t<decltype(std::declval<pd_score_util_w>().weight(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Weight>);
    
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, Weight>;
  };


  template<class EdgeData, class FuncIProb = GetEdgeData>
    requires (std::is_invocable_v<FuncIProb, const Edge<EdgeData>&>)
  struct pd_score_util_p {
    using iprob_result = std::invoke_result_t<FuncIProb, const Edge<EdgeData>&>;
    static_assert((mstd::is_arithmetic_v<iprob_result>) or (has_iprob<iprob_result>) or (has_iprob_func<iprob_result>));

    [[ no_unique_address ]] FuncIProb _iprob = {};

    decltype(auto) iprob(const auto& uv) const {
      if constexpr (mstd::is_arithmetic_v<iprob_result>) {
        return _iprob(uv);
      } else if constexpr (has_iprob<iprob_result>) {
        return _iprob(uv).iprob;
      } else if constexpr (has_iprob_func<iprob_result, EdgeData>) {
        return _iprob(uv).iprob();
      }
    }
    decltype(auto) operator()(const auto& uv) const { return iprob(uv); }

    using Probability = std::remove_cvref_t<decltype(std::declval<pd_score_util_p>().iprob(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Probability>);
  };


  template<class EdgeData, class FuncGamma = DefaultGamma<EdgeData>>
    requires (std::is_invocable_v<FuncGamma, const Edge<EdgeData>&>)
  struct pd_score_util_g {
    using gamma_result = std::invoke_result_t<FuncGamma, const Edge<EdgeData>&>;
    static_assert((mstd::is_arithmetic_v<gamma_result>) or (has_gamma<gamma_result>) or (has_gamma_func<gamma_result>));

    [[ no_unique_address ]] FuncGamma _gamma = {};

    auto& gamma(const auto& uv) const {
      if constexpr (mstd::is_arithmetic_v<gamma_result>) {
        return _gamma(uv);
      } else if constexpr (has_gamma<gamma_result>) {
        return _gamma(uv).gamma;
      } else if constexpr (has_gamma_func<gamma_result, EdgeData>) {
        return _gamma(uv).gamma();
      }
    }
    auto& operator()(const auto& uv) const { return gamma(uv); }

    using Gamma = std::remove_cvref_t<decltype(std::declval<pd_score_util_g>().gamma(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Gamma>);
  };

  template<class EdgeData, class FuncWeight = GetEdgeData, class FuncIProb = GetEdgeData>
  struct pd_score_util_wp: public pd_score_util_w<EdgeData, FuncWeight>, public pd_score_util_p<EdgeData, FuncIProb> {};
  template<class EdgeData, class FuncWeight = GetEdgeData, class FuncIProb = GetEdgeData, class FuncGamma = DefaultGamma<EdgeData>>
  struct pd_score_util_wpg: public pd_score_util_wp<EdgeData, FuncWeight, FuncIProb>, public pd_score_util_g<EdgeData, FuncGamma> {};


  // ===================== diversity modules ==========================
  // a PD score module can be called with a network and a leaf-set and returns the diversity score of that leaf-set
  // it can also be called with a network and a number k and computes sets of k leaves in the network maximizing the score
  // you can also provide functors iprob and weight to return inheritence probabilities and weights if they are not
  // stored directly in the edges

  // ===================== phylogenetic network diversity ==========================
  // the network diversity is the expected number of features surviving when saving a given set of taxa;
  // herein, a feature on a leaf-edge survives with prob 1 (leaf saved) or 0 (not saved),
  // a feature of an edge going to a tree-node dies (does not survive) if it dies in all successors, and
  // a feature of an edge going to a reticulation survives if it is inherited by the reticulation and survives below it
  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData,
    class FuncGamma = DefaultGamma<EdgeDataOf<Network>>>
  struct pd_network_diversity:
    public pd_score_util_wpg<EdgeDataOf<Network>, FuncWeight, FuncIProb, FuncGamma>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wpg<EdgeData, FuncWeight, FuncIProb, FuncGamma>;
    using typename Util::SolutionAccu;
    using typename Util::Weight;
    using typename Util::Gamma;
    using Util::iprob;
    using Util::weight;
    using Util::gamma;
    
    template<NodeContainerType Nodes>
    constexpr void compute_gammas(const Network& N, const Nodes& leaves_to_save) const {
      for(const auto uv: N.edges_postorder()) {
        auto& current_gamma = gamma(uv);
        const NodeDesc v = uv.head();
        const Gamma p_val = (Network::is_leaf(v) && (not test(leaves_to_save, v))) ? 0 : iprob(uv);
        
        Gamma tmp = 1;
        if(not Network::is_leaf(v)) {
          for(const auto vw: Network::out_edges(v)) {
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
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      const NodeVec leaves(N.leaves().template to_container<NodeVec>());
      DEBUG3(std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Network::label(x);})) << '\n');
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return score_for_leaf_set(N, S); });
    }

  };
 
  // ===================== diversity (average contained-tree formulation, brute force) ==========================
  // the avg-tree diversity is the expected weight of a random tree displayed by the network
  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData>
  struct pd_average_tree_helper:
    public pd_score_util_p<EdgeDataOf<Network>, FuncIProb>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_p<EdgeData, FuncIProb>;
    using Util::iprob;
    using typename Util::Probability;
    using Switching = PT::Switching<Network>;

    constexpr Probability probability_of_switching(const Switching& sw, const auto& leaves) const {
      const auto multiply_probs = [&](const Probability x, const auto& uv){ return x * iprob(*(uv.second));};
      return std::ranges::fold_left(sw.active_parent, Probability{1}, multiply_probs);
    }
    
    // we require a NodeContainer here since we will iterate ALOT over the leaves
    template<NodeContainerType Nodes, class ScoreFunc>
    constexpr auto score_for_leaf_set(const Nodes& leaves_to_save, ScoreFunc&& score) const {
      using Weight = decltype(score(std::declval<Switching>(), leaves_to_save));
      Weight result = 0;
      DEBUG4(size_t count = 0);
      for(auto sw_iter = SwitchingFactory<Network, const Nodes*>{&leaves_to_save}.begin(); sw_iter.is_valid(); ++sw_iter) {
        const Switching& sw = sw_iter.get_switching();
        const auto sw_prob = probability_of_switching(sw, leaves_to_save);
        const auto sw_score = score(sw, leaves_to_save);
        result += sw_prob * sw_score;
        DEBUG4(++count);
      }
      DEBUG4(std::cout << count << " switchings; total score: "<<result<<'\n');
      return result;
    }
    
    // if the leaves are given in a DFS, then it's best to copy them over into a vector since we will iterate alot over them
    template<NodeIterableType Nodes, class ScoreFunc> requires (not NodeContainerType<Nodes>)
    constexpr auto score_for_leaf_set(const Nodes& leaves_to_save, ScoreFunc&& score) const {
      NodeVec leaves;
      mstd::append(leaves, leaves_to_save);
      return score_for_leaf_set(leaves, std::forward<ScoreFunc>(score));
    }
  };

  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  struct pd_average_tree:
    public pd_average_tree_helper<Network, FuncIProb>,
    public pd_score_util_w<EdgeDataOf<Network>, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Helper = pd_average_tree_helper<Network, FuncIProb>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using Util::weight;
    using typename Util::Weight;
    using typename Util::SolutionAccu;
    using Helper::score_for_leaf_set;
    using typename Helper::Switching;

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const {
      return score_for_leaf_set(leaves_to_save, [&](const Switching& sw, const auto& leaves) {
        return std::ranges::fold_left(sw.get_active_edges(leaves), Weight{0}, [&](const Weight x, const auto& uv){ return x + weight(uv); });
      });
    }

    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) const {
      const NodeVec leaves(N.leaves().template to_container<NodeVec>());
      DEBUG3(std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Network::label(x);})) << '\n');
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return operator()(N, S); });
    }

  };


  // ===================== diversity (average contained-tree formulation, DP) ==========================
  // the avg-tree diversity is the expected weight of a random tree displayed by the network
  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData,
    class FuncWeight = GetEdgeData>
  struct pd_average_tree_DP:
    public pd_average_tree<Network, FuncIProb, FuncWeight>
  {
    using Parent = pd_average_tree<Network, FuncIProb, FuncWeight>;
    using typename Parent::Util;
    using typename Util::Weight;
    using typename Util::SolutionAccu;

    // compute score of a given set of leaves by iterating over biconnected components and over switchings of invisible reticulations within each bcc
    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const {
      throw mstd::Unimplemented("TODO: later");
      return 0;
    }

    // return the best num_solutions solutions of size k for N
    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) const {
      return optimize_displayed_tree_diversity_level(N, k, static_cast<const Parent&>(*this), num_solutions);
      //AveragePDEngine<Network, Util, NoLeafTable<Weight>> engine(N, static_cast<const Util&>(*this), num_solutions);
      //engine.optimize_displayed_tree_diversity(k);
      //return engine.score_map.leaf_table.accus.at(k);
    }
  };
  

   // ===================== phylogenetic tree diversity ==========================
   // the tree diversity is just the sum of the weights of all edges above selected leaves
  template<StrictPhylogenyType Network, class FuncWeight = GetEdgeData>
  struct pd_tree_diversity:
    public pd_score_util_w<EdgeDataOf<Network>, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using Weight = typename Util::Weight;
    using Util::weight;
    using typename Util::SolutionAccu;
    

    template<NodeIterableType Nodes>
    Weight score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) const {
      // use a reverse DFS from the leaves to save upwards
      using UpwardsDFS = Traversal<preorder | all_edge_traversal | reverse_traversal, Network, const Nodes*>;
      Weight w = 0;
      for(const auto uv: UpwardsDFS(leaves_to_save))
        w += weight(uv);
      return w;
    }

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    // to optimize the Tree-PD score for k leaves, we'll greedily take the heaviest leaf each time
    // to do this efficiently, we precompute the weight of a heaviest path below each node and the starting outgoing edge
    // for a node x, this information needs to be updated only if x is part of a path that's just been taken,
    // but this can happen at most degree(x) many times; thus the algorithm runs in linear time
    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      TreeDiversity<Network, Util, NoLeafTable<Weight>> engine(N, static_cast<const Util&>(*this), num_solutions);
      engine.optimize_diversity(k);
      return engine.get_root_table(k).at(k);
    }

  };

   // ===================== network fair proportion index ==========================
   // the NFI of a leaf x is the expected modified weight of a random root-x-path,
   // where the modified weight of uv is the ratio of the weight of uv and the expected number of taxa below v

  // this is a helper class to compute maximum-likelihood paths
  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData,
    class FuncWeight = GetEdgeData>
  struct pd_ML_path_lengths:
    public pd_score_util_wp<EdgeDataOf<Network>, FuncWeight, FuncIProb>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wp<EdgeData, FuncWeight, FuncIProb>;
    using typename Util::Weight;
    using typename Util::Probability;
    using Util::weight;
    using Util::iprob;
    using ProbWeight = std::pair<Probability, Weight>;

    NodeDesc from = NoNode;
    // cache the length of an ML-path from x to y at index [y][x], as well as the probability of such a path
    mutable NodeMap<ProbWeight> ML_length;
    
    pd_ML_path_lengths() = default;
    
    template<class... Args>
    pd_ML_path_lengths(const NodeDesc _from, Args&&... args):
      Util{std::forward<Args>(args)...},
      from{_from}
    {}


    ProbWeight& ML_length_to(const NodeDesc to, auto&& get_length) const {
      const auto [iter, success] = mstd::append(ML_length, to, 0, 0);
      if(success) {
        auto& [to_path_prob, to_length] = iter->second;
        if(to != from) [[likely]] {
          for(const auto& x: Network::parents(to)) {
            DEBUG6(std::cout << "iprob("<<NodeDesc{x}<<") = "<<iprob(x)<<"\t\t length("<<NodeDesc{x}<<") = "<<get_length(x, to)<<'\n');
            auto& [x_path_prob, x_length] = ML_length_to(x, get_length);
            const Probability to_via_x_prob = to_path_prob * iprob(x);
            if(to_via_x_prob > to_path_prob) {
              to_path_prob = to_via_x_prob;
              to_length = x_length + get_length(x, to);
            }
          }
          DEBUG6(std::cout << "found that "<<to<<" has length "<<to_length<<" with path-probability "<<to_path_prob<<'\n');
        } else to_path_prob = 1;
      }
      return iter->second;
    }
    // per default, use Util::weight as edge-length
    ProbWeight& ML_length_to(const NodeDesc to) const {
      return ML_length_to(to, [&](const auto& adj, const NodeDesc){ return weight(adj); });
    }
  };

  // this is a helper class to compute expected path-lengths
  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData,
    class FuncWeight = GetEdgeData>
  struct pd_expected_path_lengths:
    public pd_score_util_wp<EdgeDataOf<Network>, FuncWeight, FuncIProb>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wp<EdgeData, FuncWeight, FuncIProb>;
    using typename Util::Weight;
    using typename Util::Probability;
    using Util::weight;
    using Util::iprob;
    using ProbWeight = std::pair<Probability, Weight>;

    NodeDesc from = NoNode;
    // cache the expected length of a path from x to y at index [y][x], as well as the probability of such a path
    mutable NodeMap<ProbWeight> expected_length;

    pd_expected_path_lengths() = default;
    
    template<class... Args>
    pd_expected_path_lengths(const NodeDesc _from, Args&&... args):
      Util{std::forward<Args>(args)...},
      from{_from}
    {}


    ProbWeight& expected_length_to(const NodeDesc to, auto&& get_length) const {
      const auto [iter, success] = mstd::append(expected_length, to, 0, 0);
      if(success) {
        auto& [to_path_prob, to_length] = iter->second;
        if(to != from) [[likely]] {
          for(const auto& x: Network::parents(to)) {
            DEBUG6(std::cout << "iprob("<<NodeDesc{x}<<") = "<<iprob(x)<<"\t\t length("<<NodeDesc{x}<<") = "<<get_length(x, to)<<'\n');
            auto& [x_path_prob, x_length] = expected_length_to(x, get_length);
            const Probability x_to_prob = iprob(x);
            // NOTE: the events that a path comes from one neighbor or the other are INDEPENDENT (no path enters from both parents)
            //    and the prob's can thus be summed
            to_path_prob += (x_path_prob * x_to_prob);
            // NOTE: to get the expected length of a 'from'-'to'-path
            //    we add the new length times x_path_prob to it and multiply everything by iprob(y).
            to_length += (get_length(x, to) * x_path_prob + x_length) * x_to_prob;
          }
          DEBUG6(std::cout << "found that "<<to<<" has length "<<to_length<<" with path-probability "<<to_path_prob<<'\n');
        } else to_path_prob = 1;
      }
      return iter->second;
    }
    // per default, use Util::weight as edge-length
    ProbWeight& expected_length_to(const NodeDesc to) const {
      return expected_length_to(to, [&](const auto& adj, const NodeDesc){ return weight(adj); });
    }
  };


  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData,
    class FuncWeight = GetEdgeData>
  struct pd_ML:
    public pd_score_util_wp<EdgeDataOf<Network>, FuncWeight, FuncIProb>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wp<EdgeData, FuncWeight, FuncIProb>;
    using Util::iprob;

    // extract the most probable switching as a list of edges
    // NOTE: we always remove dangling leaves
    template<NodeIterableType Nodes>
    auto get_ML_switching(const Nodes& leaves) const {
      const auto prob_of = [&](const auto& adj){ return iprob(adj); };
      // we'll use the 'parent_select'-functor of the switching to select the most probable parent for each reticulation
      const auto most_probable_parent = [&](const NodeDesc r){ return std::ranges::max_element(Network::parents(r), std::ranges::less{}, prob_of); };
      return Switching<Network>{}.get_active_edges(leaves, most_probable_parent);
    }
    auto get_ML_switching(const Network& N) const { return get_ML_switching(N.leaves()); }
  };


  template<StrictPhylogenyType Network, class FuncWeight = GetEdgeData>
  struct pd_tree_fair_proportion:
    public pd_score_util_w<EdgeDataOf<Network>, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using typename Util::Weight;
    using typename Util::SolutionAccu;
    using Util::weight;

    mutable NodeMap<size_t> num_leaves_below;
    mutable NodeMap<Weight> mod_length_to;
    
    template<class Forbidden = mstd::ConstFunction<0u>>
    size_t number_of_leaves_below(const NodeDesc x, Forbidden&& forbidden = Forbidden{}) const {
      const auto [iter, success] = mstd::append(num_leaves_below, x, 0);
      if(success) {
        if(not Network::is_leaf(x)) {
          for(const auto& y: Network::children(x))
            if(not forbidden(x, y))
              iter->second += number_of_leaves_below(y);
        } else iter->second = 1;
      }
      return iter->second;
    }

    // return the cumulative modified length from the root to the given node
    template<class Forbidden = mstd::ConstFunction<0u>>
    const Weight& modified_length_to(const NodeDesc to, Forbidden&& forbidden = Forbidden{}) const {
      const auto [iter, success] = mstd::append(mod_length_to, to, 0);
      if(success and (Network::in_degree(to) > 0)) {
        for(const auto& p: Network::parents(to)) {
          if(not forbidden(p, to)) {
            iter->second = modified_length_to(p, forbidden) + (weight(p) / number_of_leaves_below(to, forbidden));
            break;
          }
        }
      }
      return iter->second;
    }

    Weight operator()(const Network& N, const NodeDesc leaf_to_save) const { return modified_length_to(leaf_to_save); }

    template<NodeIterableType Nodes, class Forbidden = mstd::ConstFunction<0u>>
    Weight score_for_leaf_set(const Nodes& leaves_to_save, Forbidden&& forbidden = Forbidden{}) const {
      return std::ranges::fold_left(
          leaves_to_save | std::ranges::views::transform([&](const NodeDesc x){ return modified_length_to(x, forbidden);}),
          Weight{0});
    }

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(leaves_to_save); }

    // to optimize the NFI score for k leaves, we'll greedily take the heaviest leaf each time
    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      mstd::SolutionAccumulator<NodeVec, Weight> result(num_solutions);
      NodeVec leaves = N.leaves().to_container();
      std::ranges::nth_element(leaves, leaves.begin() + k-1, [&](const NodeDesc x, const NodeDesc y){ return modified_length_to(x) > modified_length_to(y); });
      leaves.resize(k);
      result.add(leaves, score_for_leaf_set(leaves));
      return result;
    }

  };


  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData,
    class FuncWeight = GetEdgeData>
  struct pd_fair_proportion:
    public pd_expected_path_lengths<Network, FuncIProb, FuncWeight>
  {
    using Parent = pd_expected_path_lengths<Network, FuncIProb, FuncWeight>;
    using EdgeData = EdgeDataOf<Network>;
    using typename Parent::Util;
    using typename Util::Weight;
    using typename Util::Probability;
    using typename Util::SolutionAccu;
    using Util::weight;
    using Util::iprob;
    using ProbWeight = std::pair<Probability, Weight>;

    // cache the expected number of leaves below
    mutable NodeMap<Weight> exp_num_leaves_below;
    
    // return the expected number of leaves below of a node, in a switching drawn according to iprob
    // To this end, iterate over all maximal paths starting in x, summing their probability
    Weight expected_number_of_leaves_below(const NodeDesc x) const {
      const auto [iter, success] = mstd::append(exp_num_leaves_below, x, 0);
      if(success) {
        if(not Network::is_leaf(x)) {
          for(const auto& y: Network::children(x))
            iter->second += expected_number_of_leaves_below(y) * iprob(y);
        } else iter->second = 1; // leaves have 1 expected leaf below them
      }
      return iter->second;
    }

    Weight modified_weight(const auto& x) const { return weight(x) / expected_number_of_leaves_below(x); }
    // score for a single leaf
    // (1) for a leaf a and a root-a-path p and an edge e on p, the NFI-score of e is
    //    the weight of e divided by the expected number of leaves below e in a random switching
    //    (drawn according to the inheritence probabilities).
    // (2) then, the NFI-score of p is the sum over all edge e on p of their NFI-score
    // (3) then, the NFI-score of a is the expected NFI-score of the root-a-path in a switching drawn at random,
    //    in other words, its the sum over all root-a-paths, of the probability of that path times the NFI-score of that path
    //
    // Note that the expected number of leaves below a node does not depend on the node for whom we compute the score,
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
    ProbWeight& expected_modified_length_to(const NodeDesc to) const {
      return Parent::expected_length_to(to, [&](const auto& adj, const NodeDesc){ return modified_weight(adj);});
    }

    Weight operator()(const Network& N, const NodeDesc leaf_to_save) const {
      return expected_modified_length_to(leaf_to_save).second;
    }

    template<NodeIterableType Nodes>
    Weight score_for_leaf_set(const Nodes& leaves_to_save) const {
      return std::ranges::fold_left(
          leaves_to_save | std::ranges::views::transform([&](const NodeDesc x){ return expected_modified_length_to(x).second;}),
          Weight{0});
    }

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(leaves_to_save); }

    // to optimize the NFI score for k leaves, we'll greedily take the heaviest leaf each time
    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      mstd::SolutionAccumulator<NodeVec, Weight> result(num_solutions);
      NodeVec leaves = N.leaves().to_container();
      std::ranges::sort(leaves, [&](const NodeDesc x, const NodeDesc y){ return expected_modified_length_to(x) > expected_modified_length_to(y); });
      leaves.resize(k);
      result.add(leaves, score_for_leaf_set(leaves));
      return result;
    }

  };

  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  struct pd_average_fair_proportion: // look how beautiful the pieces fall into place
    public pd_average_tree_helper<Network, FuncIProb>,
    public pd_tree_fair_proportion<Network, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Helper = pd_average_tree_helper<Network, FuncIProb>;
    using TreeFP = pd_tree_fair_proportion<Network, FuncWeight>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using typename Util::Weight;
    using typename Util::SolutionAccu;
    using typename Helper::Switching;

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const {
      DEBUG4(std::cout << "computing average-fair-proportion score for leaf-set "<<leaves_to_save<<'\n');
      return Helper::score_for_leaf_set(leaves_to_save, [&](const Switching& sw, const auto& leaves) {
        // use the switching to forbid visiting switched-off arcs in N
        return TreeFP::score_for_leaf_set(leaves, [&](const NodeDesc x, const NodeDesc y){ return sw.is_switched_off(x,y);});
      });
    }

    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) const {
      const NodeVec leaves(N.leaves().template to_container<NodeVec>());
      DEBUG3(std::cout << leaves.size() << " leaves: " << (leaves | std::ranges::views::transform([&](const NodeDesc x){ return Network::label(x);})) << '\n');
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return operator()(N, S); });
    }

  };


  // ===================== network subnet diversity ==========================
  // Subnet-diversity(L) = sum of weights on all root-L-paths
  // NOTE: this is NP-hard by reduction from SetCover so, for now, we'll do brute-force
  template<StrictPhylogenyType Network, class FuncWeight = GetEdgeData>
  struct pd_subnet_diversity:
    public pd_score_util_w<EdgeDataOf<Network>, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using typename Util::Weight;
    using typename Util::SolutionAccu;
    using Util::weight;
    
    Weight operator()(const Network& N, const NodeDesc leaf_to_save) const {
      using Traversal = PT::Traversal<preorder | all_edge_traversal | reverse_traversal, Network, NodeDesc>;
      return std::ranges::fold_left(Traversal{leaf_to_save}, Weight{0}, [&](const Weight w, const auto uv){ return w + weight(uv);});
    }

    template<NodeIterableType Nodes>
    auto score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) const {
      using Traversal = PT::Traversal<preorder | all_edge_traversal | reverse_traversal, Network, Nodes>;
      return std::ranges::fold_left(Traversal{leaves_to_save}, Weight{0}, [&](const Weight w, const auto uv){ return w + weight(uv);});
    }

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) const { return score_for_leaf_set(N, leaves_to_save); }

    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      const NodeVec leaves(N.leaves().to_container());
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return score_for_leaf_set(N, S); });
    }

  };

}

#else
#error "diversity engine needs to be compiled with -DDFSCORO"
#endif

