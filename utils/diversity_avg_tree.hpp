
#pragma once

#include "solution_accu.hpp"
#include "subsets.hpp"

#include "net_generator.hpp"
#include "switching.hpp"
#include "biconnected_comps.hpp"

#ifdef DFSCORO
#include "dfs_coro.hpp"
#else
#include "dfs.hpp"
#endif

#include "diversity_tree.hpp"

namespace PT {

  // ========== AveragePDEngine ==========
  // This machinery can compute the weighted average tree-PD over all switchings (displayed trees).
  //
  // It consists of a DP wrt. #reticulations, and you can treat blobs individually.
  // In order to treat blobs individually, we'll need to store tables in the leaves
  // that can tell us for each number of leaves that we save below a bridge how much diversity
  // we can score by saving that many leaves.
  // We use the infrastructure provided by the TreeDiversity class.
  // However, we will need to modify the PDTreeScoreMap slightly:
  //    the score-map maps each NodeDesc x to a pair<NodeDesc, Weight>{y,w} indicating that we can score weight w if we save a leaf below the child y of x;
  //    NOTE: herein, only children y of x within a tree-component are considered! If x has no children in the same tree-component, then score-dir(x) is empty
  template<StrictPhylogenyType Network, class Utility, class LeafTable = NoLeafTable<typename Utility::Weight>>
    requires (not std::is_reference_v<LeafTable> and not std::is_reference_v<Utility>)
  struct PDScoreMapWithBaseScores:
    public PDTreeScoreMap<Network, Utility, LeafTable>
  {
    // ------- static stuff --------
    using Parent = PDTreeScoreMap<Network, Utility, LeafTable>;
    using Weight = typename Parent::Weight;
    using Probability = typename Utility::Probability;
    using ProbWeight = typename Utility::ProbWeight;
    using Edge = typename Parent::Edge;
    using ScoredDirections = typename Parent::ScoredDirections;
    using Parent::util;

    // ------- members --------
    // ------- construction & desctruction ---------
    INHERIT_ALL_CONSTRUCTORS(PDScoreMapWithBaseScores, Parent)
    
    // ------- operators --------
    // ------- methods: initialization --------
  
    // setup the scorable map for the tree-component given by its root
    //    if register_score is true, then register the scores in Parent::scorable,
    //    otherwise, we only compute the "free-score" and return it
    // return the total base-score over all edges below v in a spanning-tree
    // return the base-probability of u
    // NOTE: base_proba maps nodes to known probabilities according to the current guess of which reticulations have leaves below them
    // NOTE: the function can be called for roots in any order, no post- or pre-order is assumed
    ProbWeight setup_scorable_below(const NodeDesc u, const NodeMap<Probability>& base_proba, const bool register_score) {
      // first check if u has a base-probability
      const auto u_iter = base_proba.find(u);
      const bool u_has_base_proba = (u_iter != base_proba.end());
      ProbWeight result{u_has_base_proba ? u_iter->second : Probability{0}, 0};

      assert(result.first <= 1);

      // in order to set up the scorable map, we're getting the entry concerning u and reserve some space
      // NOTE: if u doesn't have a leaf in the same tree-component below it, then u will NOT get a score-dir;
      //       this will lead to the PDTreeScoreMap returning -inf when asked for the best score!
      ScoredDirections score_dir;
      score_dir.reserve(Network::out_degree(u));

      DEBUG4(std::cout << "** setting up scorable map below "<< u);
      DEBUG4(if(u_has_base_proba) std::cout << " (base_proba("<<u<<") = "<<u_iter->second<<") **\n"; else std::cout << " **\n");
      for(const auto uv: Network::out_edges(u)) {
        const NodeDesc v = uv.head();
        if(not Network::is_reti(v)) { // stay in the same tree-component!
          // NOTE: if u doesn't have its own base-probability, then u is guaranteed to have at most 1 child leading to a node that has a base-probability!
          const auto [v_base_proba, weight_below_v] = setup_scorable_below(v, base_proba, register_score);
          assert(v_base_proba <= 1);

          // NOTE: the weight of uv is partitioned into (v_base_proba) parts for free and (1 - v_base_proba) parts if a leaf is saved below
          const auto uv_weight = util.weight(uv);
          const auto uv_weight_free = v_base_proba * uv_weight;
          const auto uv_weight_payable = uv_weight - uv_weight_free;
          
          // step 1: update result
          result.second += weight_below_v + uv_weight_free;
          if(not u_has_base_proba and (v_base_proba > 0)) {
            assert(result.first == 0);
            result.first = v_base_proba;
          }

          // step 2: update score_dir of u
          if(register_score) {
            // if no leaf can be reached from v, then the Parent will return numeric_limits<Weight>::lowest()
            const Weight v_best_score = Parent::best_score(v);
            if(v_best_score != std::numeric_limits<Weight>::lowest())
              append(score_dir, v, v_best_score + uv_weight_payable);
          }

        } else { // v is a reticulation
          // NOTE: by design, we're not setting any scorable values for u in direction of v (the DP is over the tree-components)
          const auto v_iter = base_proba.find(v);
          // NOTE: if v has no base-proba, then all tree-components with saved leaves are above v, so v cannot survive
          if(v_iter != base_proba.end()) {
            // If u doesn't have its own base-probability, then u is a "trivial" (non-branching, non-reticulation) node of the generator.
            // In this case, we take v's base-probability and multiply by the probability of uv.
            const auto uv_proba = v_iter->second * util.iprob(uv);
            if(not u_has_base_proba)
              result.first = uv_proba; 
            // the weights of the edges below v have already been counted elsewhere (they are 0 for us now), so we only count the contribution of uv
            result.second += uv_proba * util.weight(uv);
          }
        }
      }
      // finally register the score_dir with the parent scorable-map, unless the score_dir is empty...
      // NOTE: if u is a leaf, then we'll allow emplacing an empty score_dir, indicating that the PDTreeScoreMap should forward this to the LeafTable
      if(register_score and (Network::is_leaf(u) or not score_dir.empty()))
        append(Parent::scorable, u, std::move(score_dir));
      
      DEBUG4(std::cout << "probability and weight below "<<u<<": "<<result<<'\n');

      return result;
    }
    

    // ------- methods: modification --------
    // ------- methods: query --------
  };


  // engine to compute/optimize average-tree diversity of a network N
  template<StrictPhylogenyType Net, class Utility, class LeafTable = NoLeafTable<typename Utility::Weight>>
    requires (not std::is_reference_v<LeafTable> and not std::is_reference_v<Utility>)
  struct AveragePDEngine {
    // ------- static stuff --------
    using Weight = typename Utility::Weight;
    using Probability = typename Utility::Probability;
    using NetAdjacency = typename Net::Adjacency;

    using ScoreMap = PDScoreMapWithBaseScores<Net, Utility, LeafTable>;
    using NodeHistogram = typename ScoreMap::NodeHistogram;
    using NodesByScore = typename ScoreMap::NodesByScore;

    // NOTE: we need to know for each generator node
    //        (a) the node in the network that it corresponds to
    //        (b) whether it has tree-paths to bridges/leaves
    struct GenNodeInfo {
      NodeDesc original_node;
      bool has_path_to_bridge = false;

      friend std::ostream& operator<<(std::ostream& os, const GenNodeInfo& info) { return os << '(' << info.original_node << ", tobridge: " << info.has_path_to_bridge << ')'; }
    };
    struct GenEdgeInfo {
      // At some point, we will want to compute the tail's probability in a switching.
      Probability i_prob = 0;
      
      friend std::ostream& operator<<(std::ostream& os, const GenEdgeInfo& info) { return os << "i-prob: " << info.i_prob; }
    };
    // we need an accumulator implementing the interface described in net_get.hpp
    struct GenNodeInfoAccu:
      public GenNodeInfo,
      public GenEdgeInfo
    {
      protected:
      Utility* util = nullptr;

      public:
      GenNodeInfoAccu(Utility* _util): util{_util} {}

      void operator()(const auto& uv, GenNodeInfoAccu& other, const NodeDesc u_nearest_gen, const NodeDesc v_nearest_gen) {
        assert(util);
        const auto [u,v] = static_cast<const NodePair>(uv);
        const bool v_is_on_gen_side = (v_nearest_gen != NoNode);
        const bool v_is_gen_node = (v_nearest_gen == v);
        const bool u_is_gen_node = (u_nearest_gen == u);
        
        this->original_node = u;
        if(v_is_on_gen_side) {
          if(not Net::is_reti(v))
            this->has_path_to_bridge |= other.has_path_to_bridge;
          if(v_is_gen_node)
            other.i_prob = util->iprob(uv);
          if(not u_is_gen_node)
            this->i_prob = other.i_prob;
        } else this->has_path_to_bridge = true;
        DEBUG4(std::cout << "updated node-info along "<<uv<<" (nearest generator-nodes: "<<u_nearest_gen<<" & "<<v_nearest_gen<<"): "<<*this << '\n');
      }
      friend std::ostream& operator<<(std::ostream& os, const GenNodeInfoAccu& info) { return os << '{' << static_cast<const GenNodeInfo&>(info) << " & " << static_cast<const GenEdgeInfo&>(info) << '}'; }
    };

    using Generator = Phylogeny<vecS, vecS, GenNodeInfo, GenEdgeInfo, void, Net::RootStorage>;
    using GenEdge = typename Generator::Edge;
    using GenAdj = typename Generator::Adjacency;

    using GenSwitching = Switching<Generator, vecS>;
    using GenSwitchingIter = SwitchingIter<Generator, vecS>;

    // map generator nodes to their corresponding nodes in the network
    static constexpr NodeDesc get_original_node(const NodeDesc gu) { return Generator::data(gu).original_node; }
    
    // compute score given a switching of all invisible reticulations
    template<NodeIterableType Nodes, class EdgeContainer>
    static constexpr Weight pd_score_for_switching(const Net& N,
                                                   const Nodes& _saved_nodes,
                                                   const EdgeContainer& visible_edges,
                                                   const EdgeContainer& active_edges) {
#warning "TODO: write me"
      assert(false);
      return 0;
    }


    // ------- members --------
    // the score-map tells us for a node x, which leaf in the tree-component of x we should save in order to maximize diversity
    ScoreMap score_map;

    // ------- construction & desctruction ---------
    AveragePDEngine() = delete;

    template<class UtilInit, class... Args>
    AveragePDEngine(UtilInit&& util_init, Args&&... args):
      score_map(std::forward<UtilInit>(util_init), std::forward<Args>(args)...)
    {}

    // ------- operators --------
    // ------- methods: initialization --------
    template<NodeIterableType Nodes>
    NodesByScore compute_best_scores_map(const Nodes& active_retis) const {
      NodesByScore result;
      for(const NodeDesc u: active_retis) {
        const Weight bs = score_map.best_score(u);
        if(bs > 0) append(result, u, bs);
      }
      return result;
    }


    // ------- methods: query --------
    Probability get_side_probability(const GenAdj& gu) const { return Generator::data(gu).i_prob; }
    
    using NodeAndBool = std::pair<NodeDesc, bool>;
    using ActiveNodes = mstd::SetWithSubset<const NodeVec*>;

    // save the best leaf reachable by a tree-path from u
    // return the score gained by saving u
    auto save_best_leaf_below(const NodeDesc u, NodeHistogram& solution) {
      const auto [leaf, score] = score_map.save_best_leaf_below(u);
      ++solution[leaf];
      DEBUG4(std::cout << "saving leaf "<<leaf<<" (score "<<score<<")\n");
      return score;
    }

    // construct a new solution by adding the best leaf below u to the solution with current score 'global_score', increasing the 'solution_size';
    // then, register the solution in the given 'accumulator_table'
    void register_best_leaf_below(const NodeDesc u, NodeHistogram& solution, size_t& solution_size, auto& global_score, auto& accumulator_table) {
      register_best_leaves_below(NodeSingleton{u}, solution, solution_size, global_score, accumulator_table);
    }
    template<NodeContainerType Nodes>
    void register_best_leaves_below(const Nodes& X, NodeHistogram& solution, size_t& solution_size, auto& global_score, auto& accumulator_table) {
      DEBUG4(std::cout << "\nsaving leaves below "<<X<<" (current global score: "<< global_score<<", solution size: "<<solution_size<<")\n");
      solution_size += X.size();
      for(const NodeDesc x: X)
        global_score += save_best_leaf_below(x, solution);

      DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
      accumulator_table[solution_size].add(score_map.get_leaf_table().accu_from_histogram(solution, global_score));
    }

    auto& make_accumulator_table(const NodeDesc x, const size_t k) { return score_map.get_leaf_table().emplace_table(x, k); }

    // ------- methods: modification --------
    // given that each tree-component knows its probability (base-proba, given as NodeMap) and
    //    whether it's active (that is, it has a saved leaf), given as bool attached to the reticulation list
    // 1. fulfill the promises to each tree-component
    // 2. save additional leaves in the tree-components that already have saved leaves
    void optimize_diversity_for_tree_components(const ActiveNodes& g_current_retis,
                                                const NodeVec& g_retis_with_promises,
                                                const NodeMap<Probability>& base_proba,
                                                auto& accumulator_table,
                                                const uint32_t k)
    {
      Weight global_score = 0; // score implied by the promised leaves
      const auto& g_all_retis = g_current_retis.get_ground_set();
     
      // Step 1: setup the score_map, but only for nodes that we want to save leaves below
      DEBUG3(std::cout << "--- setting up score_map map ---\n");
      score_map.clear();
      for(size_t i = 0; i < g_all_retis.size(); ++i) {
        const NodeDesc gu = g_all_retis[i];
        const NodeDesc u = get_original_node(gu);
        const bool gu_active = test(g_current_retis.subset, i);
        const auto local_score = score_map.setup_scorable_below(u, base_proba, gu_active).second;
        DEBUG4(std::cout << "scoring "<<local_score<<" for free below "<<u<<" -- global score now "<<global_score + local_score <<'\n');
        global_score += local_score;
      }
      DEBUG4(std::cout << "--- computed score map: " << static_cast<const typename ScoreMap::Parent&>(score_map) << '\n');

      // now, score_map is filled, so we can 
      // (a) fulfill our promises to the active reticulations
      // (b) save the best scoring leaves with the remaining budget
      // NOTE: only generator nodes that we promised to have saved leaves will have contributed to score_map,
      //       so only those can get additional score in step (b)
      NodeHistogram solution;
      size_t sol_size = 0;

      // Step 1b: translate the retis with promises
      auto retis_with_promises_range = (g_retis_with_promises | std::ranges::views::transform(get_original_node));
      const NodeVec retis_with_promises(retis_with_promises_range.begin(), retis_with_promises_range.end());

      // Step 2: fulfill our promises to the generator nodes
      DEBUG4(std::cout << "--- fulfilling promises to " << g_retis_with_promises << " (in generator) ---\n");
      DEBUG4(std::cout << "--- fulfilling promises to " << retis_with_promises << " (in network) ---\n");
      register_best_leaves_below(retis_with_promises, solution, sol_size, global_score, accumulator_table);

      // Step 3: take additional leaves while the budget lasts
      if(sol_size < k) {
        DEBUG3(std::cout << "--- saving "<< k - sol_size << " additional leaves ---\n");
        
        // generator nodes, sorted by their attainable score
        DEBUG4(std::cout << "attainable scores: "<< score_map << "\n");
        NodesByScore best_scores = compute_best_scores_map(retis_with_promises);
        DEBUG4(std::cout << "best scores: "<< best_scores << '\n');

        if(not best_scores.empty()) {
          while((sol_size < k) and (mstd::front(best_scores).second > 0)) {
            const auto [u, score] = mstd::value_pop(best_scores);
            register_best_leaf_below(u, solution, sol_size, global_score, accumulator_table);
            append(best_scores, u, score_map.best_score(u)); // update best_scores[u]
          } // while we still have budget to save leaves
        } // if there are scores to attain
      } // if we still have budget to save leaves
    }

    auto get_switching_probability(const GenSwitching& gswitching) const {
      Probability switching_prob = 1;
      for(const auto gvu: gswitching.active_parent) {
        assert(Generator::is_reti(gvu.first));
        switching_prob *= get_side_probability(gvu.second);
      }
      DEBUG4(std::cout << "switching "<<gswitching.active_parent<<" has probability "<<switching_prob<<'\n');
      return switching_prob;
    }


    void optimize_diversity(const Net& N, const size_t k, const size_t lower_bnd = 1) {
      assert(lower_bnd <= k);

      auto& accu_table = make_accumulator_table(N.root(), k);
      
      // ==== step 2: produce the generator network
      const Generator Gen = GeneratorMaker<Generator, GenNodeInfoAccu>::make_generator(N, GenNodeInfoAccu{&(score_map.util)});

      // if the root of the network is not in a biconnected component, then the generator is empty, so we'll just adapt the child-table
      if(not Gen.empty()) {
        DEBUG4(std::cout << "computed generator:\n" << ExtendedDisplay(Gen) << '\n');
        DEBUG4(std::cout << "SUMMARY: " << Gen.get_summary(true) << '\n');

        // ==== step 3: guess at most k tree-components of N that contain saved leaves
        // accumulate all retis of the generator that can have tree-paths to leaves
        // return whether the given generator node has a tree-path to a leaf (either direct, or via a generator edge)
        auto reti_with_path_to_bridge = [](const NodeDesc gx) { return (Generator::is_reti(gx)) ? Generator::data(gx).has_path_to_bridge : false; };
        NodeVec gvalid_retis_preorder = Gen.nodes_with_preorder(reti_with_path_to_bridge).template to_container<NodeVec>();
        // add root if possible
        if(Generator::data(Gen.root()).has_path_to_bridge)
          append(gvalid_retis_preorder, Gen.root());
        DEBUG3(std::cout << "retis with tree-paths to leaves: "<<gvalid_retis_preorder<<'\n');

        // prepare the sets for the traversal
        NodeSet seen; seen.reserve(Gen.num_nodes());
        NodeMap<Probability> base_proba; base_proba.reserve(Gen.num_nodes());

        // guess which at most k retis of the generator have tree-paths to saved leaves
        STAT(size_t sw_num = 0);
        STAT(const auto before = mstd::get_time());
        for(auto gactive_retis_it = mstd::SubsetIterator(std::as_const(gvalid_retis_preorder), lower_bnd, k); gactive_retis_it.is_valid(); ++gactive_retis_it) {
          const NodeVec gactive_retis = *gactive_retis_it;
          DEBUG2(std::cout << "\n=== new guess (with k = "<<k<<")! ===\n"<<gactive_retis.size() << " retis with saved leaves below: "<< gactive_retis<< '\n');

          // ==== step 4: for each node x in the generator, compute proportion of switchings in which x has a path to a leaf, according to the reti-guess
          // NOTE: for non-reticulations, this is just a lower bound and can be improved to 1 when saving additional leaves!
#warning "TODO: improve this using a dominator tree: not all switchings need to be iterated in order to compute the proportions!"
          STAT(size_t local_sw_num = 0);
          // the main switching-iterator, tracking reticulation-switches above gactive_retis;
          // We advance this using advance_above, which ONLY tracks reticulations reachable from gactive_retis! This saves ALOT of time!
          pred::UnseenPredicate<NodeSet> gseen_pred;
          auto gsw_iter = GenSwitchingIter{};
          gsw_iter.advance_above(gactive_retis, gseen_pred);
          do {
            const auto& gswitching = gsw_iter.get_switching();
            const Probability switching_prob = get_switching_probability(gswitching);
            STAT(++local_sw_num);
            DEBUG3(std::cout << " sw (in generator): "<<gswitching.active_parent << ", (size: "<<gswitching.active_parent.size()<<") probability: "<<switching_prob<<'\n');

            for(NodeDesc gu: gseen_pred.c) {
              const NodeDesc u = get_original_node(gu);
              Probability& u_proba = base_proba[u]; //.emplace(u, 0).first->second;
              u_proba += switching_prob;
              DEBUG4(std::cout << "increased base-proba of "<< u <<" to "<< u_proba <<'\n');
              assert(u_proba <= 1);
            }
            gseen_pred.c.clear();
          } while(gsw_iter.advance_above(gactive_retis, true, gseen_pred)); // NOTE: we mark gsw_iter as invalid if it's empty in order to catch the root component case

          STAT(sw_num += local_sw_num);
          STAT(std::cout << gactive_retis << ":\t" << local_sw_num << " switchings\n");

          // ==== step 5: treat each side individually (DP over the sides to maximize diversity with our k leaves)
          // NOTE: we use the iterator here, since it can be converted to its SetWithSubset base-class
          optimize_diversity_for_tree_components(gactive_retis_it, gactive_retis, base_proba, accu_table, k);
          base_proba.clear();
        }
        STAT(const auto elapsed = mstd::ms_between(before, mstd::get_time()));
        STAT(std::cout << sw_num << " switchings considered in "<< elapsed <<"ms = "<< sw_num / elapsed << "sw/ms\n");
      } else { // generator is empty, so N is the tree sitting on top of our network
        // we basically perfom a light version of optimize_diversity_for_tree_components here, where the only "reticulation" is N.root()
        const NodeMap<Probability> base_proba{std::make_pair(N.root(), 1)};
        Weight global_score = score_map.setup_scorable_below(N.root(), base_proba, true).second;
        NodeHistogram solution;

        for(size_t sol_size = 0; sol_size < k;) {
          const Weight best_score = score_map.best_score(N.root());
          if(best_score > 0) {
            register_best_leaf_below(N.root(), solution, sol_size, global_score, accu_table);
          } else break;
        } // for sol_size up to k
      } // if generator is empty

      DEBUG3(std::cout << "final accumulator table for the root "<<N.root()<<":\n" << accu_table <<'\n');
    } // optimize_diversity function
  };

  // NOTE: this is external to the engine since it has to partition the network before constructing the generator
  //        (which runs in the constructor of the engine)
  // NOTE: make sure Utility has callable functions weight() and iprob() returning the weight/probability of an adjacency
  template<StrictPhylogenyType Net, class Utility>
  auto optimize_displayed_tree_diversity_level(const Net& N, const size_t k, Utility&& util, const size_t num_solutions = 1) {
    // ===== treat Biconnected Components individually =====
    // we're storing the original node of the network as node-data in the biconnected component
    using NodeData = NodeDesc;
    using EdgeData = typename Net::EdgeData; // NOTE: we'll copy the edge-data onto the biconnected component and work with the copies
    using Weight = typename std::remove_cvref_t<Utility>::Weight;
    using BCComponent = CompatibleNetwork<Net, NodeData, EdgeData, void>; // no edge-data or labels necessary

    // NOTE: since we're storing the original node as node-data, InternalDataAccess returns for each node of the BCC its original node
    //       the LeafTable will then automatically translate each node of the BCC to the original node in N
    using Table = LeafTable<Weight, InternalDataAccess<BCComponent>>;
    using SolutionAccu = typename Table::SolutionAccu;

    using AvgTreeEngine = AveragePDEngine<BCComponent, std::remove_reference_t<Utility>, Table*>;
    Table leaf_table{num_solutions, {}, {}};

    assert(leaf_table.num_solutions == num_solutions);

    // ==== step 1: split into biconnected components
    // NOTE: we're extracting u's NodeDesc to store it in the corresponding BCC-node-data; also we copy each edge-data and ignore node-labels
    using Extracter = DataExtracter<Net, mstd::IdentityFunction<NodeDesc>, DefaultExtractData<Ex_edge_data, Net>, void>;
    const auto bc_components = get_nontrivial_biconnected_components<BCComponent>(N, Extracter{});
    DEBUG4(std::cout << "iterating biconnected components\n");
    for(auto& bcc: std::move(bc_components)) {
      assert(bcc.num_edges() > 1);
      DEBUG4(std::cout << "found non-trivial biconnected comp ("<<bcc.num_nodes()<<" nodes):\n" << ExtendedDisplay(bcc) << '\n');
      DEBUG4(std::cout << "SUMMARY: " << bcc.get_summary(true) << '\n');
      AvgTreeEngine engine(util, &leaf_table);
      engine.optimize_diversity(bcc, k);
      DEBUG4(std::cout << "\nfinal tables: "<<leaf_table.result_table << "\n\n");
      DEBUG4(std::cout << "We're done with this BCC now\n");
    } // for all nontrivial biconnected components bcc

    // if we don't have a table entry for the root, it means that the root is in a trivial BCC,
    // so we'll have to treat the tree-component of the root seperately
    if(not leaf_table.has_table(N.root(), false)) {
      struct HasTable { // what the actual hell, C++20 lambdas with captures have deleted move-assignment operators? This is crazy!
        const Table* lt_ptr;
        auto operator()(const NodeDesc x) const { return (Net::in_degree(x) != 0) and lt_ptr->has_table(Net::parent(x), false); }
      };
      //auto has_table = [&](const NodeDesc x){ return (Net::in_degree(x) != 0) and lt_ptr->has_table(Net::parent(x), false); };
      // to get the tree component of the root, we run a DFS in which it is forbidden to enter nodes whose parents have an accu_table
      auto root_edges = N.edges(HasTable{&leaf_table}).to_container();
      DEBUG4(std::cout << "building root component with edges "<<root_edges<<'\n');
      BCComponent root_comp(root_edges, Extracter{});
      DEBUG4(std::cout << "\nROOT component ("<<root_comp.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(root_comp) <<"\n");
      DEBUG4(std::cout << root_comp.get_summary(true) << '\n');
      AvgTreeEngine engine(std::forward<Utility>(util), &leaf_table);
      engine.optimize_diversity(root_comp, k);
    }
    DEBUG4(std::cout << "see how the root table (Node "<<N.root()<<") is doing...\n");
    auto& root_table = leaf_table.get_table(N.root(), false);
    SolutionAccu& size_k_solutions = mstd::back(root_table);
    return std::move(size_k_solutions);
  }

}
