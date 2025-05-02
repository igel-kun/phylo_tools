
#pragma once

#include "solution_accu.hpp"
#include "subsets.hpp"

#include "net_generator.hpp"
#include "switchings.hpp"

#include "dfs_coro.hpp"

namespace PT {

  // engine to compute/optimize average-tree diversity of a network N
  template<StrictPhylogenyType Net, class UtilityFunctors>
  struct AveragePDEngine {
    using NetAdjacency = typename Net::Adjacency;
    using NodeInfo = GeneratorNodeInfo;

    // NOTE: GeneratorEdgeInfo stores the first adjacency into the generator side and whether it has leaves;
    //       we'll also store the proportion of switchings in which it survives
    struct EdgeInfo: public GeneratorEdgeInfo<NetAdjacency> { double prob = 0; };
  
    // NOTE: we'll use the generator of N and all nodes referring to the generator are called g<something>, like gu, gx, gv, ...; non prefixed nodes are in N
    using Generator = Phylogeny<vecS, vecS, NodeInfo, EdgeInfo, void, Net::RootStorage>;

    // each node is mapped to the additional score attainable by taking a leaf below it, and in which direction to go to find it
    using GetSecond = mstd::selector<1>;
    using SecondSmaller = mstd::PointwiseCompose<GetSecond, std::less<>>;
    using SecondGreater = mstd::PointwiseCompose<GetSecond, std::greater<>>;
    // NOTE: the directions sorted vectors will have the largest element last, so use back() and pop_back()
    using ScoredDirections = mstd::sorted_vector<NodeWith<double>, SecondSmaller>;

    using SideScoreMap = NodeMap<ScoredDirections>;
    
    // hilarious: STL priority queue doesn't allow updating priorities
    //using NodesByScore = std::priority_queue<NodeWith<double>, std::vector<NodeWith<double>>, SecondSmaller>;
    using NodesByScore = std::multiset<NodeWith<double>, SecondGreater>;



  // ================== remove this ===================================
    struct _pd_score_ct {
      const Net* N;

      template<class EdgeContainer>
      static constexpr auto pd_score_for_switching_wp(const EdgeContainer& active_edges, auto& util) {
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

      template<class EdgeContainer>
      static constexpr double pd_score_for_switching(const EdgeContainer& active_edges, auto& util) {
        const auto [weight, prob] = pd_score_for_switching_wp(active_edges, util);
        return weight * prob;
      }


      template<NodeIterableType Nodes>
      double operator()(const Nodes& leaves_to_save, auto& util) const {
        double result = 0;
        DEBUG4(size_t count = 0);
        for(const auto switching: SwitchingFactory<const Net, const Nodes*>{*N, leaves_to_save}) {
          result += pd_score_for_switching(switching, util);
          DEBUG4(++count);
        }
        DEBUG4(std::cout << count << " switchings; total score: "<<result<<'\n');
        return result;
      }
    };
    _pd_score_ct brute_force;





    SideScoreMap scorable;
    Generator Gen;
    mstd::SolutionAccumulator<NodeVec, double> accu;
    [[ no_unique_address ]] UtilityFunctors util;

    AveragePDEngine() = delete;

    template<class UtilInit>
    AveragePDEngine(const Net& N, UtilInit&& util_init, size_t max_solutions):
      brute_force{&N},
      Gen(PT::compute_generator<Generator>(N)),
      accu(max_solutions),
      util(std::forward<UtilInit>(util_init))
    {}

    template<class UtilInit>
    AveragePDEngine(const Net& N, size_t max_solutions):
      brute_force{&N},
      Gen(PT::compute_generator<Generator>(N)),
      accu(max_solutions)
    {}



    // the second item in the data-pair indicates whether a edge/node has leaves in N
    template<class... Args>
    static constexpr bool has_leaf(const Args&... args){ return Generator::data(args...).has_leaf; };
    static constexpr bool has_tree_path_to_leaf(const NodeDesc& gx) {
      if(has_leaf(gx)) return true;
      return std::ranges::any_of(Generator::children(gx), has_leaf<NodeDesc>);
    };
    static constexpr NodeDesc get_original_node(const NodeDesc gu) { return Generator::data(gu).original_node; }
      
    using DPSideTable = std::vector<double>;


    // compute score given a switching of all invisible reticulations
    template<NodeIterableType Nodes, class EdgeContainer>
    static constexpr double pd_score_for_switching(const Net& N,
                                                   const Nodes& _saved_nodes,
                                                   const EdgeContainer& visible_edges,
                                                   const EdgeContainer& active_edges,
                                                   auto& util) {
#warning "TODO: write me"
      assert(false);
      return 0.0;
    }


    double best_score(const NodeDesc y) const {
      const auto y_it = scorable.find(y);
      if(y_it == scorable.end()) return std::numeric_limits<double>::lowest();
      if(y_it->second.empty()) return 0.0;
      return y_it->second.back().second; // get the best score achievable below y
    }

    // return the diversity obtained below u if we don't save any leaf reachable by a tree-path from u
    double get_free_score(const NodeDesc gu) const {
      DEBUG4(std::cout << "computing free score from the sides of "<<gu<<'\n');
      double weight = 0.0;
      for(const auto& gv: Gen.children(gu)) {
        const auto* end_adj = &(gv.data().end_adj);
        const double guv_prob = gv.data().prob;

        if(guv_prob > 0.0) {
          while(1) {
            DEBUG4(std::cout << "getting free score from "<<gv<<" upwards via "<<*end_adj<<" with probability factor "<<guv_prob<<'\n');
            weight += guv_prob * static_cast<double>(util.weight(*end_adj));
            if(*end_adj != get_original_node(gu)) {
              assert(Net::in_degree(*end_adj) == 1);
              end_adj = &mstd::front(Net::parents(*end_adj));
            } else break;
          }
        } // uf uv survives in some switching
      } // foreach child v of u
      return weight;
    }

    // save the leaf with maximum diversity gain below u in N (doesn't have to correspond to a generator node)
    // NOTE: this also updates the 'scorable' map
    NodeWith<double> save_best_leaf_below(const NodeDesc u) {
      DEBUG4(std::cout << "saving best leaf below "<<u<<'\n');
      const auto iter = scorable.find(u);
      assert(iter != scorable.end());
      ScoredDirections& directions = iter->second;
      if(not directions.empty()) {
        // recurse to the best node we know
        const auto [v, score] = mstd::value_pop_back(directions);
        DEBUG4(std::cout << "best score is below "<<v<<": score "<<score<<'\n');
        const NodeDesc saved_leaf = save_best_leaf_below(v).first;

        // update the scorable diversity
        // NOTE: we can no longer score uv, since it's been already collected now
        const double v_new_score = best_score(v);
        DEBUG4(std::cout << "new score of "<<u<<" via "<<v<<" is "<<v_new_score<<'\n');
        directions.emplace(v, v_new_score);

        // the result is the leaf we got from the recursion and the score we saved initially
        return {saved_leaf, score};
      } else return {u, 0};
    }

    // setup the score map below x
    // NOTE: if used on a generator side:
    //          use 'forbidden' to indicate the bottom of the side and
    //          use 'vw_prob' to indicate the probability of the lowest edge of the side (due to saved leaves below)
    void setup_score_map_below(const NodeDesc x, const auto& forbidden = NoNode, NodeDesc* const current_bottom = nullptr, const double gvw_prob = 0.0) {
      ScoredDirections& score_dir = scorable.try_emplace(x).first->second;
      score_dir.reserve(Net::out_degree(x));
      
      for(const auto& y: Net::children(x)) {
        if(not mstd::test(forbidden, y.get_desc())) {
          const double weight = util.weight(y);
          double y_scorable = best_score(y);
          // if we see the end of the side, then we know the switching probability for the edge xy is vw_prob
          if(current_bottom && (y == *current_bottom)) {
            y_scorable += (1 - gvw_prob) * weight;
            *current_bottom = x; // bottom is now x
          } else y_scorable += weight;
          // register that we can go to y to score y_scorable
          score_dir.emplace(y, y_scorable);
        } else if(current_bottom) *current_bottom = x;
      }
      DEBUG4(std::cout << "set up direction vector of "<<x<<": "<<score_dir<<'\n');
    }

    // get the best investment table for the generator sides and leaves directly below v, that is,
    // the table entry at i equals the maximum diversity scorable on the sides below v with i leaves
    // NOTE: this can be done by greedily selecting the heaviest leaves
    // NOTE: the generator edges might already have some probability of a switching containing them, so this has to be taken into account
    // NOTE: we assume that we are called in a top-down manner (we need the children in the generator to have no scorable entries)
    void setup_scorable(const NodeDesc gv) {
      const NodeDesc v_in_N = get_original_node(gv);
      DEBUG4(std::cout << "setting up scorable map below "<<v_in_N<<" (generator: "<<gv<<")\n");
      ScoredDirections& v_score_dir = scorable.try_emplace(v_in_N).first->second;
      v_score_dir.clear(); // clear out anything that might remain from upper calls

      // we produce the table by extending the tables bottom-up
      NodeSet forbidden; // NOTE: remember the first nodes on the generator sides in N in order to forbid them from the traversal catching the other leaves
      DEBUG4(std::cout << "direction vector of "<<v_in_N<<" before sides: "<<v_score_dir<<'\n');
      for(const auto& gw: Gen.children(gv)) {
        const auto gvw_prob = gw.data().prob; // probability of drawing a switching where the last edge of the generator side vw reaches a saved leaf
        const auto& start = gw.data().start_adj; // the first node in N on the side gvw
        append(forbidden, start); // recall the first node on the side to forbid going there later
        const NodeDesc side_bottom = get_original_node(gw); // keep the top node on the spine of the side, since that one might have vw_prob != 0
        if(side_bottom != start) { // only register this direction if it has more than 1 edge
          NodeDesc current_bottom = side_bottom;
          DEBUG4(std::cout << "generator side: "<<gv<<"-"<<gw<<" corresponding to path "<<v_in_N<<"-->"<<side_bottom<<" in N (free prob: "<<gvw_prob<<")\n");
          for(const NodeDesc x: Net::nodes_below_postorder(start, NodeSingleton{side_bottom})) // mark side_bottom as forbidden
            setup_score_map_below(x, side_bottom, &current_bottom, gvw_prob);
          // treat the edge v->start
          const double weight = util.weight(start);
          v_score_dir.emplace(start, best_score(start) + weight * (1 - gvw_prob));
        }
        DEBUG4(std::cout << "direction vector of "<<v_in_N<<" after side "<<gv<<'-'<<gw<<": "<<v_score_dir<<'\n');
      } // for all sides vw of v

      DEBUG4(std::cout << "scorable: "<<scorable<<'\n');
      DEBUG4(std::cout << "forbidden: "<<forbidden<<'\n');

      // add the "non-side leaves" below v_in_N (leaves that are below v_in_N but not on any side of v)
      // NOTE: we're using a custom node traversal that has no seenset and uses the constructed map as a forbidden set
      //NodeTraversal<postorder, Net, NodeDesc, const NodeSet, void> traversal{v_in_N, forbidden};
      PTx::Traversal<PTx::postorder, Net, NodeDesc, const NodeSet, void> traversal{v_in_N, forbidden};
      for(const NodeDesc x: traversal) {
        DEBUG4(std::cout << "next node in traversal: "<<x<<'\n');
        setup_score_map_below(x, forbidden);
      }
    }

    // given that each side of the generator knows its probability and whether it takes a leaf,
    // treat each side of the generator individually top to bottom
    void optimize_displayed_tree_diversity_for_sides(const NodeVec& gleaf_guess_preorder, uint32_t k) {
      double global_score = 0; // score implied by the promised leaves
     
      // Step 1: setup the scorable map, but only for nodes that we want to save leaves below
      DEBUG3(std::cout << "--- setting up scorable map ---\n");
      scorable.clear();
      for(const NodeDesc gu: gleaf_guess_preorder)  
        setup_scorable(gu);

      // Step 2: calculate free score for all sides
      DEBUG3(std::cout << "--- getting free score ---\n");
      for(const NodeDesc gu: Gen.nodes_preorder())
        global_score += get_free_score(gu);
      DEBUG4(std::cout << "free score from promises: " << global_score << '\n');

      // now, scorable is filled, we can save the best scoring leaves with the remaining budget
      // NOTE: only generator nodes that we promised to have saved leaves will have contributed to scorable,
      //       so only those can get additional score
      NodeVec solution;
      solution.reserve(k);

      // Step 3: fulfill our promises to the generator nodes
      DEBUG4(std::cout << "--- fulfilling promises to "<<gleaf_guess_preorder<<" ---\n");
      for(const NodeDesc gu: gleaf_guess_preorder) {
        const auto [leaf, score] = save_best_leaf_below(get_original_node(gu));
        append(solution, leaf);
        global_score += score;
        DEBUG4(std::cout << "saving leaf "<<leaf<<" (score "<<score<<", total: "<<global_score<<")\n");
      }
      k -= gleaf_guess_preorder.size();

      if(k > 0) {
        DEBUG4(std::cout << "attainable scores: "<< (scorable | std::ranges::views::filter([&](const auto& ux){ return not ux.second.empty();}) )<<'\n');
        // Step 4: take additional leaves while the budget lasts
        DEBUG3(std::cout << "--- preparing to save more leaves ---\n");
        
        // generator nodes, sorted by their attainable score
        NodesByScore best_scores;
        for(const NodeDesc gu: Gen.nodes()) {
          const NodeDesc u = get_original_node(gu);
          const auto iter = scorable.find(u);
          if(iter != scorable.end()) {
            const auto& directions = iter->second;
            if(not directions.empty())
              best_scores.emplace(u, directions.back().second); 
          }
        }
        DEBUG4(std::cout << "best scores: "<< best_scores << '\n');

        DEBUG3(std::cout << "--- saving "<< k<< " additional leaves ---\n");
        while((k-- > 0) && (not best_scores.empty())) {
          const auto [u, score] = mstd::value_pop(best_scores);
          DEBUG4(std::cout << "\nsaving leaf below "<<u<<" (score "<<score<<")\n");
          const auto [leaf, score2] = save_best_leaf_below(u);
          DEBUG4(std::cout << "scorable map says "<<u<<" has a path to leaf "<<leaf<<" with score "<<score2<<'\n');
          assert(score == score2);
          append(best_scores, u, best_score(u)); // update best_scores[u]
          append(solution, leaf);
          global_score += score;
        }
      }
      DEBUG4(std::cout << "finished solution: "<<solution<<" with diversity "<<global_score<<'\n');
      const double bf_score = brute_force(solution, util);
      DEBUG4(std::cout << "let's check against brute-force: " << bf_score << '\n');
      assert(bf_score == global_score);
      accu.add(solution, global_score);
    }

    // return the local probability of switching-on a side of the generator
    // NOTE: if the lowest node of the side is a tree-node, then this is 1
    //       if the lowest node of the side is a reticulation, then this is the inheritence probability on the reticulation edge
    double get_side_probability(const auto& guv) const {
      if(Gen.is_reti(guv.head())) {
        return util.iprob(guv.data().end_adj);
      } else return 1.0;
    }

    void optimize_displayed_tree_diversity(const size_t k) {
      // ==== step 1: split into biconnected components
#warning "TODO: write me"
      // ==== step 2: produce the generator network: done at init

      // ==== step 3: guess at most k sides of the generator that contain selected leaves
      // step 3.1: get all sides and nodes of the generator
      // accumulate all nodes of the generator that can have tree-paths to leaves
      const NodeVec gen_nodes_preorder = Gen.template nodes_with<preorder>(has_tree_path_to_leaf).template to_container<NodeVec>();
      DEBUG3(std::cout << "generator nodes with tree-paths to leaves: "<<gen_nodes_preorder<<'\n');

      // guess which at most k generator nodes have tree-paths to saved leaves
      for(const auto gsaved_nodes_preorder: mstd::make_subset_factory(gen_nodes_preorder, 0, k)) {
        DEBUG4(std::cout << "\n=== new guess! ===\n"<<gsaved_nodes_preorder.size() << " nodes with saved leaves below: "<<gsaved_nodes_preorder<<'\n');
        
        // clear the probabilities of the previous guess
        for(const auto guv: Gen.edges())
          guv.data().prob = 0;
       
        // ==== step 4: for each side S in the generator, compute proportion of switchings that the lowest edge of S is in
#warning "TODO: improve this using a dominator tree: not all switchings need to be iterated in order to compute the proportions!"
        for(const auto gswitching: SwitchingFactory<Generator, const NodeVec*>{Gen, gsaved_nodes_preorder}) {
          double switching_prob = 1;
          for(const auto guv: gswitching)
            if(Gen.is_reti(guv.head()))
              switching_prob *= get_side_probability(guv);
          DEBUG4(std::cout << "switching "<<gswitching<<" has probability "<<switching_prob<<'\n');
          for(const auto guv: gswitching)
            guv.data().prob += switching_prob;
        }
        
        // ==== step 5: treat each side individually (DP over the sides to maximize diversity with our k leaves)
        optimize_displayed_tree_diversity_for_sides(gsaved_nodes_preorder, k);
      }
    }
  };

}
