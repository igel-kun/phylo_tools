
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

  // tags for the data extracter
  struct pd_weight_tag {};
  struct pd_iprob_tag {};

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

  template<class T> concept has_weight = requires(T t) { t.weight; };
  template<class T> concept has_iprob = requires(T t) { t.iprob; };
  template<class T, class... Args> concept has_weight_func = requires(T t, Args... args) { t.weight(args...); };
  template<class T, class... Args> concept has_iprob_func = requires(T t, Args... args) { t.iprob(args...); };
  template<class T> concept weight_compatible = has_weight<T> or has_weight_func<T> or std::is_arithmetic_v<T>;
  template<class T> concept iprob_compatible = has_iprob<T> or has_iprob_func<T> or std::is_arithmetic_v<T>;

  // per default, get iprob and weight from the edge-data
  struct GetEdgeData {
    template<class Data>
    auto& operator()(const PT::Edge<Data>& uv) const { return uv.data(); }

    template<class Data>
    auto& operator()(const PT::Adjacency<Data>& uv) const { return uv.data(); }

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
    decltype(auto) operator()(const pd_weight_tag, const auto& uv) const { return weight(uv); }

    using Weight = std::remove_cvref_t<decltype(std::declval<pd_score_util_w>().weight(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Weight>);
    
    // for compatibility with score helpers
    template<class... Args> static void init(Args&&... args){}

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
    decltype(auto) operator()(const pd_iprob_tag, const auto& uv) const { return iprob(uv); }

    using Probability = std::remove_cvref_t<decltype(std::declval<pd_score_util_p>().iprob(std::declval<Edge<EdgeData>>()))>;
    static_assert(mstd::is_arithmetic_v<Probability>);
  };


  template<class EdgeData, class FuncWeight = GetEdgeData, class FuncIProb = GetEdgeData>
  struct pd_score_util_wp:
    public pd_score_util_w<EdgeData, FuncWeight>,
    public pd_score_util_p<EdgeData, FuncIProb>
  {
    decltype(auto) operator()(const pd_weight_tag, const auto& uv) const { return this->weight(uv); }
    decltype(auto) operator()(const pd_iprob_tag, const auto& uv) const { return this->iprob(uv); }
  };

  // ================== Score Modules ========================
  // score modules can be plugged into strategy modules like BruteForce
  // score modules have a 'switching mode' so you can pass a switching instead of a network

  template<class NetOrSwitch, class FuncWeight = GetEdgeData>
    requires (StrictPhylogenyType<NetOrSwitch> or SwitchingType<NetOrSwitch>)
  struct ProtoScore {
    using Network = NetworkOf<NetOrSwitch>;
    using EdgeData = EdgeDataOf<Network>;

    static constexpr bool switching_mode = SwitchingType<NetOrSwitch>;

    using Switching = std::conditional_t<switching_mode, const NetOrSwitch*, std::monostate>;

    [[ no_unique_address ]] Switching sw;
  };

  // score module for gamma-scoring (normal PD)
  template<class NetOrSwitch,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
    requires (StrictPhylogenyType<NetOrSwitch> or SwitchingType<NetOrSwitch>)
  struct pd_score_util_wpg:
    public ProtoScore<NetOrSwitch>,
    public pd_score_util_wp<EdgeDataOf<NetworkOf<NetOrSwitch>>, FuncWeight, FuncIProb>
  {
    using Parent = ProtoScore<NetOrSwitch>;
    using typename Parent::Network;
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_wp<EdgeData, FuncWeight, FuncIProb>;
    using typename Util::Weight;
    using Util::weight;

    // cache the gamma for the in-edge of v if v is a tree-node, or the out-edge of v if v is a reticulation
    mutable NodeMap<Weight> gamma_cache;

    Weight gamma(const Adjacency<EdgeData>& v) const {
      const auto [iter, success] = mstd::append(gamma_cache, v, 1);
      if(success) {
        Weight& tmp = iter->second;
        if(not Network::is_leaf(v)) {
          for(const auto w: Network::children(v)) {
            if constexpr (Parent::switching_mode)
              if(Parent::sw->is_switched_off(v, w)) continue;
            tmp *= 1 - gamma(w) * Util::operator()(pd_iprob_tag{}, w);
            if(tmp == 0) break;
          }
          tmp = 1 - tmp;
        } // if v is an unsaved leaf, it's gamma-value is 0, as initialized
      }
      return iter->second;
    }

    // init all saved leaves to gamma = 1
    template<NodeIterableType Leaves>
    void init(const NetOrSwitch& N, const Leaves& saved_leaves) {
      if constexpr (Parent::switching_mode) Parent::sw = &N;
      gamma_cache.clear();
      for(const NodeDesc x: saved_leaves)
        mstd::append(gamma_cache, x, 1);
    }

    Weight operator()(const pd_weight_tag, const Adjacency<EdgeData>& v) const { return weight(v) * gamma(v); }
    Weight operator()(const pd_weight_tag, const Edge<EdgeData>& uv) const { return weight(uv) * gamma(uv.head()); }
  };

  // score module for Shapeley scoring
  template<class NetOrSwitch, class FuncWeight = GetEdgeData>
    requires (StrictPhylogenyType<NetOrSwitch> or SwitchingType<NetOrSwitch>)
  struct pd_score_ws:
    public ProtoScore<NetOrSwitch>,
    public pd_score_util_w<EdgeDataOf<NetworkOf<NetOrSwitch>>, FuncWeight>
  {
    using Parent = ProtoScore<NetOrSwitch>;
    using Network = typename Parent::Network;
    using EdgeData = EdgeDataOf<Network>;
    using Util = pd_score_util_w<EdgeData, FuncWeight>;
    using typename Util::Weight;
    using Util::weight;

    mutable NodeMap<uint32_t> num_dying_taxa_cache;

    uint32_t num_dying_taxa_below(const NodeDesc x) const {
      const auto [iter, success] = mstd::append(num_dying_taxa_cache, x, 0u);
      if(success) {
        if(not Network::is_leaf(x)) {
          for(const auto& y: Network::children(x)) {
            if constexpr (Parent::switching_mode)
              if(Parent::sw->is_switched_off(x, y)) continue;
            iter->second += num_dying_taxa_below(y);
          }
        } else iter->second = 1; // unmarked leaves have 1 leaf below them
      }
      return iter->second;
    }

    template<NodeIterableType Leaves>
    void init(const NetOrSwitch& N, const Leaves& saved_leaves) {
      if constexpr (Parent::switching_mode) Parent::sw = &N;
      num_dying_taxa_cache.clear();
      for(const NodeDesc x: saved_leaves)
        mstd::append(num_dying_taxa_cache, x, 0);
    }

    // NOTE: the modified weight is only correct for edges above saved leaves!
    Weight operator()(const pd_weight_tag, const Adjacency<EdgeData>& v) const { return weight(v) / (num_dying_taxa_below(v) + 1); }
    Weight operator()(const pd_weight_tag, const Edge<EdgeData>& uv) const { return  weight(uv) / (num_dying_taxa_below(uv.head()) + 1);}
  };

  // ===================== advanced helper classes ==========================

  // this helper class can iterate over all switchings
  template<StrictPhylogenyType Network,
    class FuncIProb = GetEdgeData>
  struct pd_switching_helper:
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

  // this brute-forced score calculation can wrap around any score-calculator for a set of edges
  // NOTE: make sure your score'a-calculator is either default constructible or constructible with a network and a NodeIterable
  template<class Network, class Score>
  struct pd_brute_force_helper:
    public Score
  {
    using Weight = std::invoke_result_t<Score, const pd_weight_tag, const typename Network::Edge&>;
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, Weight>;
    
    template<class... Args>
    static Score make_score(Args&&... args) {
      if constexpr (std::is_constructible_v<Score, Args&&...>)
        return Score(std::forward<Args>(args)...);
      else return Score{};
    }

    template<NodeIterableType Nodes>
      requires (std::is_constructible_v<Score, const Network&, const Nodes&> or std::is_default_constructible_v<Score>)
    auto score_for_leaf_set(const Network& N, const Nodes& leaves_to_save) {
      using Traversal = PT::Traversal<preorder | all_edge_traversal | reverse_traversal, Network, const Nodes*>;
      Score::init(N, leaves_to_save);
      return std::ranges::fold_left(Traversal{leaves_to_save}, Weight{0},
          [&](const Weight w, const auto& uv){ return w + Score::operator()(pd_weight_tag{}, uv);});
    }

    template<NodeIterableType Nodes>
    Weight operator()(const Network& N, const Nodes& leaves_to_save) { return score_for_leaf_set(N, leaves_to_save); }

    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      const NodeVec leaves(N.leaves().to_container());
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return score_for_leaf_set(N, S); });
    }
  };

  // the average-tree helper can compute the expected value of a score function over all switchings
  template<StrictPhylogenyType Network,
    class Score,
    class FuncIProb = GetEdgeData>
  struct pd_average_switching_helper:
    public Score,
    public pd_switching_helper<Network, FuncIProb>
  {
    using Weight = std::invoke_result_t<Score, const pd_weight_tag, const typename Network::Edge&>;
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, Weight>;
    using Helper = pd_switching_helper<Network, FuncIProb>;
    using typename Helper::Switching;

    template<class... Args>
    static Score make_score(Args&&... args) {
      if constexpr (std::is_constructible_v<Score, Args&&...>)
        return Score(std::forward<Args>(args)...);
      else return Score{};
    }

    template<NodeIterableType Nodes>
      requires (std::is_constructible_v<Score, const Network&, const Nodes&> or std::is_default_constructible_v<Score>)
    Weight operator()(const Network& N, const Nodes& leaves_to_save) {
      Score::init(N, leaves_to_save);
      return Helper::score_for_leaf_set(leaves_to_save, [&](const Switching& sw, const auto& leaves) {
        return std::ranges::fold_left(sw.get_active_edges(leaves), Weight{0},
            [&](const Weight x, const auto& uv){ return x + Score::operator()(pd_weight_tag{}, uv); });
      });
    }

    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1) {
      const NodeVec leaves(N.leaves().template to_container<NodeVec>());
      return mstd::brute_force(k, leaves, num_solutions, [&](const auto& S){ return operator()(N, S); });
    }
  };



  // these helper classes can extract 
  // 1. maximum-probability-path-lengths and
  // 2. expected path-lengths
  // both of them will have corresponding accumulators that can be plugged into the following length-getter

  // NOTE: the ProbWeightGetter must inherit from pd_score_util_p, so we can inherit from it later on
  template<StrictPhylogenyType Network,
    class ProbWeightGetter,
    class LengthAccumulator>
      requires mstd::is_derived_from_template_v<ProbWeightGetter, pd_score_util_p>
  struct ProtoLengthGetter:
    public ProbWeightGetter
  {
    using EdgeData = EdgeDataOf<Network>;
    using Adjacency = AdjacencyOf<Network>;

    static_assert(std::is_invocable_v<ProbWeightGetter, pd_weight_tag, const Adjacency&>);

    using Weight = std::remove_cvref_t<std::invoke_result_t<ProbWeightGetter, pd_weight_tag, const Adjacency&>>;
    using typename ProbWeightGetter::Probability;
    using typename ProbWeightGetter::SolutionAccu;
    using ProbWeight = std::pair<Probability, Weight>;
    
    static_assert(std::is_invocable_v<LengthAccumulator, ProbWeight&, ProbWeight, ProbWeight>);

    [[ no_unique_address ]] LengthAccumulator accumulate_prob_len;
    
    NodeDesc from = NoNode;
    mutable NodeMap<ProbWeight> length_cache;

    ProtoLengthGetter() = default;

    ProtoLengthGetter(const NodeDesc _from):
      from{_from}
    {}

    template<class ProbWeightInit, class... Args>
    ProtoLengthGetter(const NodeDesc _from, ProbWeightInit&& pw_init, Args&&... args):
      ProbWeightGetter{std::forward<ProbWeightInit>(pw_init)},
      accumulate_prob_len{std::forward<Args>(args)...},
      from{_from}
    {}

    template<EdgePredicateType<Network> Forbidden = mstd::ConstFunction<0u>>
    ProbWeight& length_to(const NodeDesc to, Forbidden&& forbidden = {}) const {
      const auto [iter, success] = mstd::append(length_cache, to, 0, 0);
      if(success) {
        if((to != from) and not Network::is_root(to)) [[likely]] {
          for(const auto& x: Network::parents(to)) {
            if(not forbidden(x, to)) {
              const ProbWeight x_pw = ProbWeight{ProbWeightGetter::operator()(pd_iprob_tag{}, x), ProbWeightGetter::operator()(pd_weight_tag{}, x)};
              DEBUG5(std::cout << "prob-weight("<<NodeDesc{x}<<") = "<<x_pw<<'\n');
              accumulate_prob_len(iter->second, length_to(x), x_pw);
            }
          }
          DEBUG5(std::cout << "found that "<<to<<" has prob-length "<<iter->second<<'\n');
        } else iter->second.first = 1;
      }
      return iter->second;
    }

    // the score of a set is just the sum of the singleton-scores of its elements
    template<NodeIterableType Nodes, EdgePredicateType<Network> Forbidden = mstd::ConstFunction<0u>>
    Weight score_for_leaf_set(const Nodes& leaves_to_save, Forbidden&& forbidden = {}) const {
      Weight result = std::ranges::fold_left(leaves_to_save, Weight{0}, [&](const Weight x, const NodeDesc v){ return x + length_to(v, forbidden).second;});
      std::cout << "got score "<<result<<" for leaf set "<<leaves_to_save<<'\n';
      return result;
    }

    template<NodeIterableType Nodes, EdgePredicateType<Network> Forbidden = mstd::ConstFunction<0u>>
    Weight operator()(const Network& N, const Nodes& leaves_to_save, Forbidden&& forbidden = {}) const {
      return score_for_leaf_set(leaves_to_save, forbidden);
    }

    // to optimize the score for k leaves, we'll greedily take the heaviest leaf each time
    template<EdgePredicateType<Network> Forbidden = mstd::ConstFunction<0u>>
    SolutionAccu operator()(const Network& N, const size_t k, const size_t num_solutions = 1, Forbidden&& forbidden = {}) const {
      mstd::SolutionAccumulator<NodeVec, Weight> result(num_solutions);
      NodeVec leaves = N.leaves().to_container();
      std::ranges::sort(leaves, [&](const NodeDesc x, const NodeDesc y){ return length_to(x, forbidden) > length_to(y, forbidden); });
      leaves.resize(k);
      result.add(leaves, score_for_leaf_set(leaves));
      return result;
    }

  };

  // how the maximum-likelihood strategy accumulates probabilities and weights
  struct accumulate_prob_len_ML {
    void operator()(auto& accu, const auto to_parent, const auto parent_to_us) const {
      const auto new_prob = to_parent.first * parent_to_us.first;
      if(new_prob > accu.first)
        accu = {new_prob, to_parent.second + parent_to_us.second};
    }
  };
  // this is a helper class to compute maximum-likelihood paths
  template<StrictPhylogenyType Network,
    class ProbWeightGetter = pd_score_util_wp<EdgeDataOf<Network>, GetEdgeData, GetEdgeData>>
  using pd_ML_path_lengths = ProtoLengthGetter<Network, ProbWeightGetter, accumulate_prob_len_ML>;
  
  // how the expected-path-length strategy accumulates probabilities and weights
  struct accumulate_prob_len_exp {
    void operator()(auto& accu, const auto to_parent, const auto parent_to_us) const {
      // NOTE: the events that a path comes from one neighbor or the other are INDEPENDENT (no path enters from both parents)
      //    and the prob's can thus be summed
      accu.first += (to_parent.first * parent_to_us.first);
      // NOTE: to get the expected length of a 'from'-'to'-path
      //    we add the new length times the old probability to it and multiply everything by the parent->us probability
      accu.second += (parent_to_us.second * to_parent.first + to_parent.second) * parent_to_us.first;
      DEBUG5(std::cout << "score to parent: "<<to_parent<< " + score to us: "<<parent_to_us<<" --> accu "<<accu<<'\n');
    }
  };
  // this is a helper class to compute expected path lengths
  template<StrictPhylogenyType Network,
    class ProbWeightGetter = pd_score_util_wp<EdgeDataOf<Network>, GetEdgeData, GetEdgeData>>
  using pd_expected_path_lengths = ProtoLengthGetter<Network, ProbWeightGetter, accumulate_prob_len_exp>;


  // this helper class is used to get switchings with maximum probability
  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
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
    class FuncIProb = GetEdgeData>
  using pd_network_diversity = pd_brute_force_helper<Network, pd_score_util_wpg<Network, FuncWeight, FuncIProb>>;


  // ===================== diversity (average contained-tree formulation, brute force) ==========================
  // the avg-tree diversity is the expected weight of a random tree displayed by the network
  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  using pd_average_tree = pd_average_switching_helper<Network, pd_score_util_w<EdgeDataOf<Network>, FuncWeight>, FuncIProb>;


  // ===================== diversity (average contained-tree formulation, DP) ==========================
  // the avg-tree diversity is the expected weight of a random tree displayed by the network
  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  struct pd_average_tree_DP:
    public pd_average_tree<Network, FuncWeight, FuncIProb>
  {
    using Parent = pd_average_tree<Network, FuncWeight, FuncIProb>;
    using typename Parent::Weight;
    using typename Parent::SolutionAccu;

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
      if constexpr (NodeContainerType<Nodes>) {
        DEBUG4(std::cout << "computing score for leaf-set "<<leaves_to_save<<'\n');
      }
      // use a reverse DFS from the leaves to save upwards
      using UpwardsDFS = Traversal<preorder | all_edge_traversal | reverse_traversal, Network, const Nodes*>;

      UpwardsDFS tmp{leaves_to_save};
      Weight w = 0;
      for(const auto uv: std::move(tmp)) {
        w += weight(uv);
      }
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

  template<class Network, class FuncWeight, class FuncIProb>
  struct NetFairProportionWeights:
    public pd_score_util_wp<EdgeDataOf<Network>, FuncWeight, FuncIProb>
  {
    using Weights = pd_score_util_w<EdgeDataOf<Network>, FuncWeight>;
    using Probs = pd_score_util_p<EdgeDataOf<Network>, FuncIProb>;
    using typename Weights::Weight;
    using typename Probs::Probability;
    using Weights::weight;
    using Probs::operator();
    
    static Weight modified_weight(const Weight w, const Probability num_leaves) { return num_leaves ? w / num_leaves : 0; }

    // cache the expected number of leaves below
    mutable NodeMap<Probability> num_leaf_cache = {};
 
    // return the expected number of leaves below of a node, in a switching drawn according to iprob
    // To this end, iterate over all maximal paths starting in x, summing their probability
    template<EdgePredicateType<Network> Forbidden = mstd::ConstFunction<0u>>
    Weight expected_number_of_leaves_below(const NodeDesc x, Forbidden&& forbidden = {}) const {
      const auto [iter, success] = mstd::append(num_leaf_cache, x, 0);
      if(success) {
        if(not Network::is_leaf(x)) {
          for(const auto& y: Network::children(x))
            if(not forbidden(x, y))
              iter->second += expected_number_of_leaves_below(y) * Probs::operator()(y);
        } else iter->second = 1; // leaves have 1 expected leaf below them
      }
      return iter->second;
    }


    Weight operator()(const pd_weight_tag, const AdjacencyOf<Network>& v) const { return modified_weight(weight(v), expected_number_of_leaves_below(v)); }
    Weight operator()(const pd_weight_tag, const typename Network::Edge& uv) const { return modified_weight(weight(uv), expected_number_of_leaves_below(uv.head())); }
  };

  /*
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
  */
  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  using pd_fair_proportion = pd_expected_path_lengths<Network, NetFairProportionWeights<Network, FuncWeight, FuncIProb>>;

  // on trees, all edge-probabilities are 1
  // there is really no significantly more efficient way than just passing the 1-function as probability-function
  template<class Network, class FuncWeight>
  using TreeFairProportionWeights = NetFairProportionWeights<Network, FuncWeight, mstd::ConstFunction<1u>>;

  template<StrictPhylogenyType Network, class FuncWeight = GetEdgeData>
  using pd_tree_fair_proportion = pd_expected_path_lengths<Network, TreeFairProportionWeights<Network, FuncWeight>>;


  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  struct pd_average_fair_proportion: // look how beautiful the pieces fall into place
    public pd_switching_helper<Network, FuncIProb>,
    public pd_tree_fair_proportion<Network, FuncWeight>
  {
    using EdgeData = EdgeDataOf<Network>;
    using Helper = pd_switching_helper<Network, FuncIProb>;
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


  // ===================== Shapeley index ==========================
  // Shapeley scores each taxon with the average contribution it brings when added as a last leaf to any set of leaves ("coalition")
  //    NOTE: for trees, this is equivalent to the Fair-Proportion index [FJ'15]
  // For a set S of taxa, the Shapeley-score is the average contribution of adding this entire set to the coalition
  //    NOTE: this can be seen as "merging" all leaves in S into a new "player" joining the coalition
  //    NOTE: a closed form for this is: sum_{e above S} len(e)/(#(non-S descendant-leaves of e) + 1)
  //    NOTE: note that this coincides with the singleton defintion for |S|=1

  template<StrictPhylogenyType Network, class FuncWeight = GetEdgeData>
  using pd_tree_shapeley = pd_brute_force_helper<Network, pd_score_ws<Network, FuncWeight>>;

  template<StrictPhylogenyType Network,
    class FuncWeight = GetEdgeData,
    class FuncIProb = GetEdgeData>
  using pd_average_shapeley = pd_average_switching_helper<Network, pd_score_ws<Network, FuncWeight>, FuncIProb>;


  // ===================== network subnet diversity ==========================
  // Subnet-diversity(L) = sum of weights on all root-L-paths
  // NOTE: this is NP-hard by reduction from SetCover so, for now, we'll do brute-force
  // NOTE: this is as if you assigned prob=1 to all edges and ran expected-switching-PD
  // NOTE: conversely, any sensible way to using the inheritence probabilities will give you the expected-switching-PD
  template<StrictPhylogenyType Network, class FuncWeight = GetEdgeData>
  using pd_subnet_diversity = pd_brute_force_helper<Network, pd_score_util_w<EdgeDataOf<Network>, FuncWeight>>;

}

#else
#error "diversity engine needs to be compiled with -DDFSCORO"
#endif

