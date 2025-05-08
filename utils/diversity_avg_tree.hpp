
#pragma once

#include "solution_accu.hpp"
#include "subsets.hpp"

#include "net_generator.hpp"
#include "switchings.hpp"
#include "biconnected_comps.hpp"

#include "dfs_coro.hpp"

namespace PT {

  struct NoLeafTable {
    // the level-DP assigns a table to each bridge (represented as a leaf in the BiconnectedComponent)
    //    mapping each i<k to the diversity that can be gained by saving exactly i leaves below
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, double>;
    using AccuTable = std::vector<SolutionAccu>;

    size_t num_solutions; // number of different solutions to keep per k
    AccuTable accus = {};

    static double best_score_below(const NodeDesc x) { return 0.0; }
    static NodeWith<double> save_leaf(const NodeDesc x) { return {x, 0.0}; }

    template<class Solution> requires mstd::is_same_v<Solution, NodeVec>
    void add_solution(const NodeDesc, Solution&& sol, const double score) {
      accus.reserve(sol.size());
      while(accus.size() <= sol.size())
        accus.emplace_back(num_solutions);
      accus[sol.size()].add(std::forward<Solution>(sol), score);
    }

    const AccuTable& get_root_table(const NodeDesc) const { return accus; }

    // make a new leaf-table for x
    // NOTE: don't use this to get the root table
    AccuTable* leaf_table_ptr(const NodeDesc x) const { return nullptr; }

    auto& get_accu_vector(const NodeDesc, const size_t sol_size) {
      while(accus.size() <= sol_size)
        accus.emplace_back(num_solutions);
      return accus;
    }
    const auto& get_accu_vector(const NodeDesc) const { return accus; }

    static void reset_use_counts() {}
  };

  template<class Translate = mstd::IdentityFunction<NodeDesc>> // Translate = how to translate given nodes into indices of the result_table?
  struct LeafTable {
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, double>;
    using AccuTable = std::vector<SolutionAccu>;
    // in the table, we store for each k the diversity attainable by saving k leaves; we'll also store how many leaves were already saved below
    using ResultTable = std::pair<AccuTable, size_t>;

    size_t num_solutions; // number of different solutions to keep per k
    NodeMap<ResultTable> result_table; // we're mapping translated nodes to their tables
    [[ no_unique_address ]] Translate translate;

    template<class Table, bool use_leaf = not std::is_const_v<Table>>
      requires mstd::is_same_v<Table, NodeMap<ResultTable>>
    static double access_next_diff(Table& tab, const NodeDesc x) {
      const auto iter = tab.find(x);
      if(iter != tab.end()) {
        auto& [x_table, saved_so_far] = iter->second;
        if(x_table.size() > saved_so_far + 1) {
          const auto& next_sols = x_table[saved_so_far + 1];
          const auto& current_sols = x_table[saved_so_far];
          if constexpr (use_leaf) 
            ++saved_so_far;
          const double best_next_sol = next_sols.get_best_or(0.0);
          const double best_current_sol = current_sols.get_best_or(0.0);
          return best_next_sol - best_current_sol;
        } else return 0.0;
      } else return 0.0;
    }

    const AccuTable* leaf_table_ptr(const NodeDesc x) const { 
      const auto iter = result_table.find(x);
      if(iter != result_table.end())
        return &(iter->second.first);
      else return nullptr;
    }
    const AccuTable& get_root_table(const NodeDesc x) const { assert(test(result_table, x)); return result_table.at(x).first; }

    // return the accumulator-vector for x (create if necessary)
    auto& get_accu_vector(const NodeDesc x, const size_t sol_size) {
      return mstd::append(result_table, translate(x), std::piecewise_construct, std::tuple{sol_size + 1, num_solutions}, std::tuple{0}).first->second.first;
    }
    const auto& get_accu_vector(const NodeDesc x) const { return result_table.at(translate(x)); }

    // resets the use-counters of all result_table entries to 0
    void reset_use_counts() {
      for(auto& [x, sol_with_counter]: result_table)
        sol_with_counter.second = 0;
    }

    // query best score below a given node
    double best_score_below(const NodeDesc x) const {
      return access_next_diff(std::as_const(result_table), translate(x));
    }

    // save the best leaf in the network below a given node
    NodeWith<double> save_leaf(const NodeDesc x) {
      const NodeDesc orig_x = translate(x);
      static_assert(not std::is_const_v<decltype(result_table)>);
      return {orig_x, access_next_diff(result_table, orig_x)};
    }

    template<class Solution> requires mstd::is_same_v<Solution, NodeVec>
    void add_solution(const NodeDesc x, Solution&& sol, const double score) {
      assert(num_solutions != 0);
      auto& accus = get_accu_vector(x, sol.size());
      accus.reserve(sol.size());
      while(accus.size() <= sol.size())
        accus.emplace_back(num_solutions);
      accus[sol.size()].add(std::forward<Solution>(sol), score);
      DEBUG4(std::cout << "added solution to table[Node "<<translate(x)<<"] = "<<accus<<'\n');
    }
  };

  // engine to compute/optimize average-tree diversity of a network N
  template<StrictPhylogenyType Net, class UtilityFunctors, class LeafQuery = NoLeafTable>
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
    using NodeHistogram = NodeMap<size_t>;

    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, double>;
    using AccuTable = std::vector<SolutionAccu>;

    SideScoreMap scorable;
    Generator Gen;
    [[ no_unique_address ]] LeafQuery leaf_query;
    [[ no_unique_address ]] UtilityFunctors util;

    

    AveragePDEngine() = delete;

    template<class UtilInit, class... Args>
    AveragePDEngine(const Net& N, UtilInit&& util_init, Args&&... args):
      Gen(PT::compute_generator<Generator>(N)),
      leaf_query(std::forward<Args>(args)...),
      util(std::forward<UtilInit>(util_init))
    {}


    // return whether the given generator node or side has a leaf that is reachable outside the generator
    template<class... Args>
    static constexpr bool has_leaf(const Args&... args){ return Generator::data(args...).has_leaf; };
    // return whether the given generator node has a tree-path to a leaf (either direct, or via a generator edge)
    static constexpr bool has_tree_path_to_leaf(const NodeDesc& gx) {
      if(has_leaf(gx)) return true;
      return std::ranges::any_of(Generator::children(gx), has_leaf<typename Generator::Adjacency>);
    };
    // map generator nodes to their corresponding nodes in the network
    static constexpr NodeDesc get_original_node(const NodeDesc gu) { return Generator::data(gu).original_node; }

    NodeDesc get_root_in_N() const { return get_original_node(Gen.root()); }

    auto& leaf_table() { return mstd::access(leaf_query); }
    const auto& leaf_table() const { return mstd::access(leaf_query); }
    
    size_t num_solutions() const { return leaf_table().num_solutions; }

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


    // return the best score below a node y in the network by following the scorable-pointers
    double best_score(const NodeDesc y) const {
      const auto y_it = scorable.find(y);
      if(y_it == scorable.end()) return std::numeric_limits<double>::lowest();
      if(y_it->second.empty()) return leaf_table().best_score_below(y);
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
      } else return leaf_table().save_leaf(u);
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
          DEBUG4(std::cout << y << " scores "<<y_scorable<<" ("<<y<<" is bottom? "<<(current_bottom and (y == *current_bottom)) << '\n');
          // if we see the end of the side, then we know the switching probability for the edge xy is vw_prob
          if(current_bottom and (y == *current_bottom)) {
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

    SolutionAccu make_leaf_table(const NodeDesc x) const { 
      SolutionAccu result(num_solutions());
      result.add(NodeVec{x}, 0.0);
      return result;
    }

    // translate a node histogram to a SolutionAccumulator that can be added to the solution accumulator vector of the root
    // NOTE: this will translate "take reticulation x 3 times" to the best solution below x using 3 leaves
    SolutionAccu accu_from_histogram(const NodeHistogram& hist, const double global_score) {
      SolutionAccu result{num_solutions()};
      for(const auto [x, i]: hist) {
        const auto x_table_ptr = leaf_table().leaf_table_ptr(x);
        if(x_table_ptr != nullptr) {
          assert(x_table_ptr->size() > i);
          result += (*x_table_ptr)[i];
        } else {
          assert(i == 1);
          result += make_leaf_table(x);
        }
      }
      // add the delta between the best solution and 'score' to all solutions
      const double score_delta = global_score - result.get_best().second;
      assert(score_delta >= 0.0);
      for(auto& [sol, score]: result.solutions)
        score += score_delta;

      DEBUG4(std::cout << "turned history "<<hist<<" into accu " << result<< '\n');
      return result;
    }

    NodesByScore compute_best_scores_map() const {
      NodesByScore result;
      for(const NodeDesc gu: Gen.nodes()) {
        const NodeDesc u = get_original_node(gu);
        const auto iter = scorable.find(u);
        if(iter != scorable.end()) {
          const auto& directions = iter->second;
          if(not directions.empty())
            result.emplace(u, directions.back().second); 
        }
      }
      return result;
    }

    // given that each side of the generator knows its probability and whether it takes a leaf,
    // treat each side of the generator individually top to bottom
    void optimize_displayed_tree_diversity_for_sides(const NodeVec& gleaf_guess_preorder, const uint32_t k) {
      double global_score = 0; // score implied by the promised leaves
     
      // Step 1: setup the scorable map, but only for nodes that we want to save leaves below
      DEBUG3(std::cout << "--- setting up scorable map ---\n");
      scorable.clear();
      leaf_table().reset_use_counts(); // reset the use counts from the previous iteration

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
      NodeHistogram solution;
      size_t sol_size = 0;

      // Step 3: fulfill our promises to the generator nodes
      DEBUG4(std::cout << "--- fulfilling promises to "<<gleaf_guess_preorder<<" ---\n");
      for(const NodeDesc gu: gleaf_guess_preorder) {
        const auto [leaf, score] = save_best_leaf_below(get_original_node(gu));
        ++solution[leaf];
        ++sol_size;
        global_score += score;
        DEBUG4(std::cout << "saving leaf "<<leaf<<" (score "<<score<<", total: "<<global_score<<")\n");
      } 
      DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
      auto& accu_table = leaf_table().get_accu_vector(get_root_in_N(), k);
      DEBUG4(std::cout << "got accus : "<<accu_table<<'\n');
      accu_table[sol_size].add(accu_from_histogram(solution, global_score));
     
      if(sol_size < k) {
        DEBUG4(std::cout << "attainable scores: "<< (scorable | std::ranges::views::filter([&](const auto& ux){ return not ux.second.empty();}) )<<'\n');
        // Step 4: take additional leaves while the budget lasts
        DEBUG3(std::cout << "--- preparing to save more leaves ---\n");
        
        // generator nodes, sorted by their attainable score
        NodesByScore best_scores = compute_best_scores_map();
        DEBUG4(std::cout << "best scores: "<< best_scores << '\n');

        if(not best_scores.empty()) {
          DEBUG3(std::cout << "--- saving "<< k - sol_size << " additional leaves ---\n");
          while((sol_size < k) && (mstd::front(best_scores).second > 0.0)) {
            const auto [u, score] = mstd::value_pop(best_scores);
            DEBUG4(std::cout << "\nsaving leaf below "<<u<<" (score "<<score<<")\n");
            const auto [leaf, score2] = save_best_leaf_below(u);
            DEBUG4(std::cout << "scorable map says "<<u<<" has a path to leaf "<<leaf<<" with score "<<score2<<'\n');
            assert(score == score2);
            append(best_scores, u, best_score(u)); // update best_scores[u]
            ++solution[leaf];
            ++sol_size;
            global_score += score;

            DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
            accu_table[sol_size].add(accu_from_histogram(solution, global_score));
          } // while we still have budget to save leaves
        } // if there are scores to attain
      } // if we still have budget to save leaves
    }

    // return the local probability of switching-on a side of the generator
    // NOTE: if the lowest node of the side is a tree-node, then this is 1
    //       if the lowest node of the side is a reticulation, then this is the inheritence probability on the reticulation edge
    double get_side_probability(const auto& guv) const {
      if(Gen.is_reti(guv.head())) {
        return util.iprob(guv.data().end_adj);
      } else return 1.0;
    }

    void optimize_displayed_tree_diversity(const size_t k, const size_t lower_bnd = 1) {
      // ==== step 2: produce the generator network: done at init

      // ==== step 3: guess at most k sides of the generator that contain selected leaves
      // step 3.1: get all sides and nodes of the generator
      // accumulate all nodes of the generator that can have tree-paths to leaves
      const NodeVec gen_nodes_preorder = Gen.template nodes_with<preorder>(has_tree_path_to_leaf).template to_container<NodeVec>();
      DEBUG3(std::cout << "generator nodes with tree-paths to leaves: "<<gen_nodes_preorder<<'\n');

      // guess which at most k generator nodes have tree-paths to saved leaves
      for(const auto gsaved_nodes_preorder: mstd::make_subset_factory(gen_nodes_preorder, lower_bnd, k)) {
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

  template<StrictPhylogenyType Net, class UtilityFunctors>
  auto optimize_displayed_tree_diversity_level(const Net& N, const size_t k, UtilityFunctors&& util, const size_t num_solutions = 1) {
    // ===== treat Biconnected Components individually =====
    // we're storing the original node of the network as node-data in the biconnected component
    using NodeData = NodeDesc;
    using EdgeData = typename Net::EdgeData; // NOTE: we'll copy the edge-data onto the biconnected component and work with the copies
    using BCComponent = CompatibleNetwork<Net, NodeData, EdgeData, void>; // no edge-data or labels necessary

    using Table = LeafTable<InternalDataAccess<BCComponent>>;
    using AvgTreeEngine = AveragePDEngine<BCComponent, UtilityFunctors, Table*>;
    Table table{num_solutions, {}, {}};

    assert(table.num_solutions == num_solutions);

    // ==== step 1: split into biconnected components
    // NOTE: we're extracting u's NodeDesc in order to store it in the corresponding node in the BCC as data
    const auto bc_components = get_biconnected_components<BCComponent, false>(N, Ex_node_data{}, mstd::IdentityFunction<NodeDesc>());
    DEBUG4(std::cout << "iterating biconnected components\n");
    for(auto& bcc: std::move(bc_components)) {
      assert(bcc.num_edges() > 1);
      DEBUG4(std::cout << "found non-trivial biconnected comp ("<<bcc.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(bcc) <<"\n");
      DEBUG4(bcc.print_summary(std::cout));
      AvgTreeEngine engine(bcc, std::forward<UtilityFunctors>(util), &table);
      engine.optimize_displayed_tree_diversity(k);
      DEBUG4(std::cout << "\nfinal tables: "<<table.result_table << "\n\n");
    } // for all nontrivial biconnected components bcc

    // if we don't have a table entry for the root, it means that the root is in a trivial BCC,
    // so we'll have to treat the tree-component of the root seperately
    if(not test(table.result_table, N.root())) {
      // to get the tree component of the root, we run a DFS in which it is forbidden to enter children of reticulations
      auto root_edges = N.edges([&](const NodeDesc x){
          const bool res = (Net::in_degree(x) != 0) and test(table.result_table, Net::parent(x));
          return res;
        }).to_container();
      DEBUG4(std::cout << "building root component with edges "<<root_edges<<'\n');
      BCComponent root_comp(root_edges, Ex_node_data{}, mstd::IdentityFunction<NodeDesc>());
      DEBUG4(std::cout << "\nROOT component ("<<root_comp.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(root_comp) <<"\n");
      DEBUG4(root_comp.print_summary(std::cout));
      AvgTreeEngine engine(root_comp, std::forward<UtilityFunctors>(util), &table);
      engine.optimize_displayed_tree_diversity(k);
    }
    DEBUG4(std::cout << "see how the root table (Node "<<N.root()<<") is doing...\n");
    auto& root_table = table.get_root_table(N.root());
    auto& size_k_solutions = root_table.back();
    return std::move(size_k_solutions);
  }

}
