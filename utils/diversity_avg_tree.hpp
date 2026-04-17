
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
    auto setup_scorable_below(const NodeDesc u, const NodeMap<Probability>& base_proba, const bool register_score) {
      // first check if u has a base-probability
      const auto u_iter = base_proba.find(u);
      const bool u_has_base_proba = (u_iter != base_proba.end());
      std::pair<Weight, Probability> result = {0, u_has_base_proba ? u_iter->second : Probability{0}};

      // in order to set up the scorable map, we're getting the entry concerning u and reserve some space
      // NOTE: if u doesn't have a leaf in the same tree-component below it, then u will NOT get a score-dir;
      //       this will lead to the PDTreeScoreMap returning -inf when asked for the best score!
      ScoredDirections score_dir;
      score_dir.reserve(Network::out_degree(u));

      for(const auto uv: Network::out_edges(u)) {
        const NodeDesc v = uv.head();
        if(not Network::is_reti(v)) { // stay in the same tree-component!
          // NOTE: if u doesn't have its own base-probability, u is guaranteed to only have 1 child leading to a node that has a base-probability!
          const auto [weight_below_v, v_base_proba] = setup_scorable_below(v, base_proba, register_score);

          // NOTE: the weight of uv is partitioned into (v_base_proba) parts for free and (1 - v_base_proba) parts if a leaf is saved below
          const auto uv_weight = util.weight(uv);
          const auto uv_weight_free = v_base_proba * uv_weight;
          const auto uv_weight_payable = uv_weight - uv_weight_free;
          
          // step 1: update result
          result.first += weight_below_v + uv_weight_free;
          if(not u_has_base_proba and (v_base_proba > 0)) {
            assert(result.second == 0);
            result.second = v_base_proba;
          }

          // step 2: update score_dir of u
          if(register_score) {
            // if no leaf can be reached from v, then the Parent will return numeric_limits<Weight>::lowest()
            const Weight v_best_score = Parent::best_score(v);
            if(v_best_score != std::numeric_limits<Weight>::lowest())
              append(score_dir, v, v_best_score + uv_weight_payable);
          }

        } else {
          // v is a reticulation, implying that it has a base-probability
          assert(Network::is_reti(v));
          assert(test(base_proba, v));
          // NOTE: we're not setting any scorable values for u in direction of v
          // if u doesn't have its own base-probability, then we take v's base-probability and multiply by the probability of uv
          const auto uv_proba = base_proba.at(v) * util.probability(uv);
          if(not u_has_base_proba)
            result.second = uv_proba;
          // the weights of the edges below v have already been counted elsewhere (they are 0 for us now), so we only count the contribution of uv
          result.first += uv_proba * util.weight(uv);
        }
      }
      // finally register the score_dir with the parent scorable-map, unless the score_dir is empty...
      // NOTE: if u is a leaf, then we'll allow emplacing an empty score_dir, indicating that the PDTreeScoreMap should forward this to the LeafTable
      if(register_score and (Network::is_leaf(u) or not score_dir.empty()))
        append(Parent::scorable, u, std::move(score_dir));

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
    };
    struct GenEdgeInfo {
      // At some point, we will want to compute the tail's probability in a switching.
      Probability i_prob;
    };
    // we need an accumulator implementing the interface described in net_get.hpp
    struct GenNodeInfoAccu: public GenNodeInfo {
      Utility* util = nullptr;

      void operator()(const auto& uv, GenNodeInfoAccu& other, const NodeDesc u_nearest_gen, const NodeDesc v_nearest_gen) {
        assert(util);
        const NodeDesc v = uv.head();
        const bool v_is_on_gen_side = (v_nearest_gen != NoNode);
        const bool v_is_gen_node = (v_nearest_gen == v);
        
        this->original_node = uv.tail(); // our original node is u
        if(v_is_on_gen_side) {
          if(not Net::is_reti(v))
            this->has_path_to_bridge |= other.has_path_to_bridge;
          if(v_is_gen_node) {
            this->i_prob = util->probability(uv);
          } else this->i_prob = other.i_prob;
        } else this->has_path_to_bridge = true;
      }
    };
    using Generator = Phylogeny<vecS, vecS, GenNodeInfo, GenEdgeInfo, void, Net::RootStorage>;
    using GenEdge = typename Generator::Edge;

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
    AveragePDEngine(const Net& N, UtilInit&& util_init, Args&&... args):
      score_map(std::forward<UtilInit>(util_init), std::forward<Args>(args)...)
    {}

    // ------- operators --------
    // ------- methods: initialization --------
    NodesByScore compute_best_scores_map(const NodeVec& active_retis) const {
      NodesByScore result;
      for(const NodeDesc u: active_retis) {
        const Weight bs = score_map.best_score(u);
        if(bs > 0) append(result, u, bs);
      }
      return result;
    }


    // ------- methods: query --------
    Probability get_side_probabilty(const GenEdge& guv) { return Generator::data(guv).i_prob; }
    
    using NodeAndBool = std::pair<NodeDesc, bool>;
    using ActiveNodes = std::vector<NodeAndBool>;

    // ------- methods: modification --------
    // given that each tree-component knows its probability (base-proba, given as NodeMap) and
    //    whether it's active (that is, it has a saved leaf), given as bool attached to the reticulation list
    // 1. fulfill the promises to each tree-component
    // 2. save additional leaves in the tree-components that already have saved leaves
    void optimize_diversity_for_tree_components(const NodeDesc N_root, const ActiveNodes& retis, const NodeMap<Probability>& base_proba, const uint32_t k) {
      Weight global_score = 0; // score implied by the promised leaves
      NodeVec active_retis; // store the active reticulations seperately
      active_retis.reserve(retis.size()); // overestimated, but meh
     
      // Step 1: setup the score_map, but only for nodes that we want to save leaves below
      DEBUG3(std::cout << "--- setting up score_map map ---\n");
      score_map.clear();
      for(const auto [u, active]: retis) {
        global_score += score_map.setup_scorable_below(u, base_proba, active).first;
        if(active) append(active_retis, u);
      }

      // now, score_map is filled, so we can 
      // (a) fulfill our promises to the active reticulations
      // (b) save the best scoring leaves with the remaining budget
      // NOTE: only generator nodes that we promised to have saved leaves will have contributed to score_map,
      //       so only those can get additional score in step (b)
      NodeHistogram solution;
      size_t sol_size = 0;

      // Step 2: fulfill our promises to the generator nodes
      DEBUG4(std::cout << "--- fulfilling promises to " << active_retis << " ---\n");
      for(const NodeDesc u: active_retis) {
        const auto [leaf, score] = score_map.save_best_leaf_below(u);
        ++solution[leaf];
        ++sol_size;
        global_score += score;
        DEBUG4(std::cout << "saving leaf "<<leaf<<" (score "<<score<<", total: "<<global_score<<")\n");
      } 
      DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<" to the leaf-score-table\n");
      auto& leaf_table = score_map.get_leaf_table();
      auto& accu_table = leaf_table.emplace_table(N_root, k);
      accu_table[sol_size].add(leaf_table.accu_from_histogram(solution, global_score));

      // Step 3: take additional leaves while the budget lasts
      if(sol_size < k) {
        DEBUG3(std::cout << "--- saving "<< k - sol_size << " additional leaves ---\n");
        
        // generator nodes, sorted by their attainable score
        DEBUG4(std::cout << "attainable scores: "<< score_map << "\n");
        NodesByScore best_scores = compute_best_scores_map(active_retis);
        DEBUG4(std::cout << "best scores: "<< best_scores << '\n');

        if(not best_scores.empty()) {
          while((sol_size < k) and (mstd::front(best_scores).second > 0)) {
            const auto [u, score] = mstd::value_pop(best_scores);
            DEBUG4(std::cout << "\nsaving leaf below "<<u<<" (score "<<score<<")\n");
            const auto [leaf, score2] = score_map.save_best_leaf_below(u);
            DEBUG4(std::cout << "score map says "<<u<<" has a path to leaf "<<leaf<<" with score "<<score2<<'\n');
            assert(score == score2);
            append(best_scores, u, score_map.best_score(u)); // update best_scores[u]
            ++solution[leaf];
            ++sol_size;
            global_score += score;

            DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
            accu_table[sol_size].add(leaf_table.accu_from_histogram(solution, global_score));
          } // while we still have budget to save leaves
        } // if there are scores to attain
      } // if we still have budget to save leaves
    }


    void optimize_diversity(const Net& N, const size_t k, const size_t lower_bnd = 1) {
      // ==== step 2: produce the generator network
      const Generator Gen = GeneratorMaker<Generator, GenNodeInfoAccu>::make_generator(N, GenNodeInfoAccu{&(score_map.util)});

      // ==== step 3: guess at most k tree-components of N that contain saved leaves
      // accumulate all retis of the generator that can have tree-paths to leaves
      // return whether the given generator node has a tree-path to a leaf (either direct, or via a generator edge)
      auto reti_with_path_to_bridge = [](const NodeDesc gx) { return (Generator::is_reti(gx)) ? Generator::data(gx).has_path_to_bridge : false; };
      const NodeVec gvalid_retis_preorder = Gen.nodes_with_preorder(reti_with_path_to_bridge).template to_container<NodeVec>();
      DEBUG3(std::cout << "retis with tree-paths to leaves: "<<gvalid_retis_preorder<<'\n');

      // guess which at most k retis of the generator have tree-paths to saved leaves
      for(const auto gsaved_retis_preorder: mstd::make_subset_factory(gvalid_retis_preorder, lower_bnd, k)) {
        DEBUG3(std::cout << "\n=== new guess! ===\n"<<gsaved_retis_preorder.size() << " retis with saved leaves below: "<<gsaved_retis_preorder<<'\n');

        // ==== step 4: for each node x in the generator, compute proportion of switchings in which x has a path to a leaf, according to the reti-guess
        // NOTE: for non-reticulations, this is just a lower bound and can be improved to 1 when saving additional leaves!
#warning "TODO: improve this using a dominator tree: not all switchings need to be iterated in order to compute the proportions!"
        NodeMap<Probability> base_proba;
        for(const auto gswitching: SwitchingFactory<Generator, const NodeVec*>{&gsaved_retis_preorder}) {
          Probability switching_prob = 1;
          for(const auto guv: gswitching) {
            assert(Generator::is_reti(guv.head()));
            switching_prob *= get_side_probability(guv);
          }
          DEBUG4(std::cout << "switching "<<gswitching<<" has probability "<<switching_prob<<'\n');
          // to enumerate the edges above 'gsaved_retis' in the switching, we use a special bottom-up traversal with the switching as forbidden-predicate
          auto gtraversal = NodeTraversal<reverse_traversal, Generator, NodeVec, const Switching<Net>*>{gsaved_retis_preorder, &gswitching};
          for(const NodeDesc gu: gtraversal)
            base_proba[gu] += switching_prob;
        }
        
        // ==== step 5: treat each side individually (DP over the sides to maximize diversity with our k leaves)
        optimize_diversity_for_sides(N.root(), gsaved_retis_preorder, base_proba, k);
      }
    }
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
    using Table = LeafTable<Weight, InternalDataAccess<BCComponent>>;
    using SolutionAccu = typename Table::SolutionAccu;

    using AvgTreeEngine = AveragePDEngine<BCComponent, std::remove_reference_t<Utility>, Table*>;
    Table leaf_table{num_solutions, {}, {}};

    assert(leaf_table.num_solutions == num_solutions);

    // ==== step 1: split into biconnected components
    // NOTE: we're extracting u's NodeDesc in order to store it in the corresponding node in the BCC as data
    const auto bc_components = get_biconnected_components<BCComponent, false>(N, Ex_node_data{}, mstd::IdentityFunction<NodeDesc>());
    DEBUG4(std::cout << "iterating biconnected components\n");
    for(auto& bcc: std::move(bc_components)) {
      assert(bcc.num_edges() > 1);
      DEBUG4(std::cout << "found non-trivial biconnected comp ("<<bcc.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(bcc) <<"\n" << bcc.get_summary());
      AvgTreeEngine engine(bcc, std::forward<Utility>(util), &leaf_table);
      engine.optimize_diversity(k);
      DEBUG4(std::cout << "\nfinal tables: "<<leaf_table.result_table << "\n\n");
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
      BCComponent root_comp(root_edges, Ex_node_data{}, mstd::IdentityFunction<NodeDesc>());
      DEBUG4(std::cout << "\nROOT component ("<<root_comp.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(root_comp) <<"\n");
      DEBUG4(root_comp.print_summary(std::cout));
      AvgTreeEngine engine(root_comp, std::forward<Utility>(util), &leaf_table);
      engine.optimize_diversity(k);
    }
    DEBUG4(std::cout << "see how the root table (Node "<<N.root()<<") is doing...\n");
    auto& root_table = leaf_table.emplace_table(N.root(), k, false);
    SolutionAccu& size_k_solutions = mstd::back(root_table);
    return std::move(size_k_solutions);
  }

}
