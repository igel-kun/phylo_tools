
#pragma once

#include "solution_accu.hpp"
#include "subsets.hpp"

#include "net_generator.hpp"
#include "switchings.hpp"
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
  // In order to treat blobss individually, we'll need to store tables in the leaves
  // that can tell us for eahc number of leaves that we save below a bridge how much diversity
  // we can score by saving that many leaves.
  // We use the infrastructure provided by the TreeDiversity class.
  // However, we will need to modify the PDTreeScoreMap slightly...

  template<StrictPhylogenyType Network, class Utility, class LeafTable = NoLeafTable<typename Utility::Weight>>
    requires (not std::is_reference_v<LeafTable> and not std::is_reference_v<Utility>)
  struct PDGeneratorScoreMap:
    public PDTreeScoreMap<Network, Utility, LeafTable>
  {
    // ------- static stuff --------
    using Parent = PDTreeScoreMap<Network, Utility, LeafTable>;
    using Weight = typename Parent::Weight;
    using Edge = typename Parent::Edge;
    using ScoredDirections = typename Parent::ScoredDirections;
    using Parent::util;

    // ------- members --------
    // ------- construction & desctruction ---------
    INHERIT_ALL_CONSTRUCTORS(PDGeneratorScoreMap, Parent)
    
    // ------- operators --------
    // ------- methods: initialization --------
  
    // setup the score map directly below x
    // NOTE: if used on a generator side:
    //          use 'forbidden' to indicate the bottom of the side and
    //          use 'offset' to indicate the probability of the lowest edge of the side (due to saved leaves below)
    void setup_scorable_at(const NodeDesc x, const auto& forbidden = NoNode,
        NodeDesc* const current_bottom = nullptr, const Weight offset = 0)
    {
      ScoredDirections& score_dir = Parent::get_directions(x);
      score_dir.reserve(Network::out_degree(x));
      
      for(const auto& y: Network::children(x)) {
        if(not mstd::test(forbidden, y.get_desc())) {
          const auto weight = util.weight(Edge(x,y));
          auto y_scorable = Parent::best_score(y);
          DEBUG4(std::cout << y << " scores "<<y_scorable<<" ("<<y<<" is bottom? "<<(current_bottom and (y == *current_bottom)) << '\n');
          // if we see the end of the side, then we know the switching probability for the edge xy is vw_prob
          if(current_bottom and (y == *current_bottom)) {
            y_scorable += (1 - offset) * weight;
            *current_bottom = x; // bottom is now x
          } else y_scorable += weight;
          // register that we can go to y to score y_scorable
          score_dir.emplace(y, y_scorable);
        } else if(current_bottom) *current_bottom = x;
      }
      DEBUG4(std::cout << "set up direction vector of "<<x<<": "<<score_dir<<'\n');
    }
    void setup_scorable_below(const NodeDesc v, const auto& forbidden) {
      // add the "non-side leaves" below v_in_N (leaves that are below v_in_N but not on any side of v)
      // NOTE: we're using a custom node traversal that has no seenset and uses the constructed map as a forbidden set
      //NodeTraversal<postorder, Net, NodeDesc, const NodeSet, void> traversal{v_in_N, forbidden};
      Traversal<postorder, Network, NodeDesc, const NodeSet, void> traversal{v, forbidden};
      for(const NodeDesc x: std::move(traversal)) {
        DEBUG4(std::cout << "next node in traversal: "<<x<<'\n');
        setup_scorable_at(x, forbidden);
      }
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
    using NodeInfo = GeneratorNodeInfo;
   
    // NOTE: GeneratorEdgeInfo stores the first adjacency into the generator side and whether it has leaves;
    //       we'll also store the proportion of switchings in which it survives
    struct EdgeInfo: public GeneratorEdgeInfo<NetAdjacency> { Probability prob = 0; };
    // NOTE: we'll use the generator of N and all nodes referring to the generator are called g<something>, like gu, gx, gv, ...; non prefixed nodes are in N
    using Generator = Phylogeny<vecS, vecS, NodeInfo, EdgeInfo, void, Net::RootStorage>;
 
    using ScoreMap = PDGeneratorScoreMap<Net, Utility, LeafTable>;
    using NodeHistogram = typename ScoreMap::NodeHistogram;
   
    // hilarious: STL priority queue doesn't allow updating priorities
    //using NodesByScore = std::priority_queue<NodeWith<double>, std::vector<NodeWith<double>>, SecondSmaller>;
    using NodesByScore = typename ScoreMap::NodesByScore;


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
    ScoreMap score_map;
    Generator Gen;


    // ------- construction & desctruction ---------
    AveragePDEngine() = delete;

    template<class UtilInit, class... Args>
    AveragePDEngine(const Net& N, UtilInit&& util_init, Args&&... args):
      score_map(std::forward<UtilInit>(util_init), std::forward<Args>(args)...),
      Gen(PT::compute_generator<Generator>(N))
    {}

    // ------- operators --------
    // ------- methods: initialization --------
    // get the best investment table for the generator sides and leaves directly below v, that is,
    // the table entry at i equals the maximum diversity score_map on the sides below v with i leaves
    // NOTE: this can be done by greedily selecting the heaviest leaves
    // NOTE: the generator edges might already have some probability of a switching containing them, so this has to be taken into account
    // NOTE: we assume that we are called in a top-down manner (we need the children in the generator to have no score_map entries)
    void setup_score_map(const NodeDesc gv) {
      const NodeDesc v_in_N = get_original_node(gv);
      DEBUG4(std::cout << "setting up score map below "<<v_in_N<<" (generator: "<<gv<<")\n");
      auto& v_score_dir = score_map.get_directions(v_in_N);
      v_score_dir.clear(); // clear out anything that might remain from upper calls

      // we produce the table by extending the tables bottom-up
      NodeSet forbidden; // NOTE: remember the first nodes on the generator sides in N in order to forbid them from the traversal catching the other leaves
      DEBUG4(std::cout << "direction vector of "<<v_in_N<<" before sides: "<<v_score_dir<<'\n');
      for(const auto& gw: Generator::children(gv)) {
        const auto gvw_prob = gw.data().prob; // probability of drawing a switching where the last edge of the generator side vw reaches a saved leaf
        const auto& start = gw.data().start_adj; // the first node in N on the side gvw
        append(forbidden, start); // recall the first node on the side to forbid going there later
        const NodeDesc side_bottom = get_original_node(gw); // keep the top node on the spine of the side, since that one might have vw_prob != 0
        if(side_bottom != start) { // only register this direction if it has more than 1 edge
          NodeDesc current_bottom = side_bottom;
          DEBUG4(std::cout << "generator side: "<<gv<<"-"<<gw<<" corresponding to path "<<v_in_N<<"-->"<<side_bottom<<" in N (free prob: "<<gvw_prob<<")\n");
          for(const NodeDesc x: Net::nodes_below_postorder(start, NodeSingleton{side_bottom})) // mark side_bottom as forbidden
            score_map.setup_scorable_at(x, side_bottom, &current_bottom, gvw_prob);
          // treat the edge v->start
          const auto weight = score_map.util.weight(start);
          v_score_dir.emplace(start, score_map.best_score(start) + weight * (1 - gvw_prob));
        }
        DEBUG4(std::cout << "direction vector of "<<v_in_N<<" after side "<<gv<<'-'<<gw<<": "<<v_score_dir<<'\n');
      } // for all sides vw of v

      DEBUG4(std::cout << "score_map: "<<score_map<<'\n');
      DEBUG4(std::cout << "forbidden: "<<forbidden<<'\n');

      // finally setup for the tree-part below v_in_N that's not on any generator side
      score_map.setup_scorable_below(v_in_N, forbidden);
    }

    NodesByScore compute_best_scores_map() const {
      NodesByScore result;
      for(const NodeDesc gu: Gen.nodes()) {
        const NodeDesc u = get_original_node(gu);
        const auto bs = score_map.best_score_for_node(u);
        if(bs > 0) result.emplace(u, bs);
      }
      return result;
    }


    // ------- methods: query --------
    NodeDesc get_root_in_N() const { return get_original_node(Gen.root()); }
    
    // return the diversity obtained below u if we don't save any leaf reachable by a tree-path from u
    Weight get_free_score(const NodeDesc gu) const {
      DEBUG4(std::cout << "computing free score from the sides of "<<gu<<'\n');
      Weight weight = 0;
      for(const auto& gv: Generator::children(gu)) {
        const auto* end_adj = &(gv.data().end_adj);
        const auto guv_prob = gv.data().prob;

        if(guv_prob > 0) {
          while(1) {
            DEBUG4(std::cout << "getting free score from "<<gv<<" upwards via "<<*end_adj<<" with probability factor "<<guv_prob<<'\n');
            weight += guv_prob * score_map.util.weight(*end_adj);
            if(*end_adj != get_original_node(gu)) {
              assert(Net::in_degree(*end_adj) == 1);
              end_adj = &mstd::front(Net::parents(*end_adj));
            } else break;
          }
        } // uf uv survives in some switching
      } // foreach child v of u
      return weight;
    }

    // return the local probability of switching-on a side of the generator
    // NOTE: if the lowest node of the side is a tree-node, then this is 1
    //       if the lowest node of the side is a reticulation, then this is the inheritence probability on the reticulation edge
    Probability get_side_probability(const auto& guv) const {
      if(Generator::is_reti(guv.head())) {
        return score_map.util.iprob(guv.data().end_adj);
      } else return 1;
    }


    // ------- methods: modification --------
    // given that each side of the generator knows its probability and whether it takes a leaf,
    // treat each side of the generator individually top to bottom
    void optimize_diversity_for_sides(const NodeVec& gleaf_guess_preorder, const uint32_t k) {
      Weight global_score = 0; // score implied by the promised leaves
     
      // Step 1: setup the score_map map, but only for nodes that we want to save leaves below
      DEBUG3(std::cout << "--- setting up score_map map ---\n");
      score_map.clear();

      for(const NodeDesc gu: gleaf_guess_preorder)  
        setup_score_map(gu);

      // Step 2: calculate free score for all sides
      DEBUG3(std::cout << "--- getting free score ---\n");
      for(const NodeDesc gu: Gen.nodes_preorder())
        global_score += get_free_score(gu);
      DEBUG4(std::cout << "free score from promises: " << global_score << '\n');

      // now, score_map is filled, we can save the best scoring leaves with the remaining budget
      // NOTE: only generator nodes that we promised to have saved leaves will have contributed to score_map,
      //       so only those can get additional score
      NodeHistogram solution;
      size_t sol_size = 0;

      // Step 3: fulfill our promises to the generator nodes
      DEBUG4(std::cout << "--- fulfilling promises to "<<gleaf_guess_preorder<<" ---\n");
      for(const NodeDesc gu: gleaf_guess_preorder) {
        const auto [leaf, score] = score_map.save_best_leaf_below(get_original_node(gu));
        ++solution[leaf];
        ++sol_size;
        global_score += score;
        DEBUG4(std::cout << "saving leaf "<<leaf<<" (score "<<score<<", total: "<<global_score<<")\n");
      } 
      DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
      auto& accu_table = score_map.get_leaf_table().emplace_table(get_root_in_N(), k);
      DEBUG4(std::cout << "got accus : "<<accu_table<<'\n');
      accu_table[sol_size].add(score_map.get_leaf_table().accu_from_histogram(solution, global_score));
     
      if(sol_size < k) {
        DEBUG4(std::cout << "attainable scores: "<< score_map << "\n");
        // Step 4: take additional leaves while the budget lasts
        DEBUG3(std::cout << "--- preparing to save more leaves ---\n");
        
        // generator nodes, sorted by their attainable score
        NodesByScore best_scores = compute_best_scores_map();
        DEBUG4(std::cout << "best scores: "<< best_scores << '\n');

        if(not best_scores.empty()) {
          DEBUG3(std::cout << "--- saving "<< k - sol_size << " additional leaves ---\n");
          while((sol_size < k) && (mstd::front(best_scores).second > 0)) {
            const auto [u, score] = mstd::value_pop(best_scores);
            DEBUG4(std::cout << "\nsaving leaf below "<<u<<" (score "<<score<<")\n");
            const auto [leaf, score2] = score_map.save_best_leaf_below(u);
            DEBUG4(std::cout << "score_map map says "<<u<<" has a path to leaf "<<leaf<<" with score "<<score2<<'\n');
            assert(score == score2);
            append(best_scores, u, score_map.best_score(u)); // update best_scores[u]
            ++solution[leaf];
            ++sol_size;
            global_score += score;

            DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
            accu_table[sol_size].add(score_map.get_leaf_table().accu_from_histogram(solution, global_score));
          } // while we still have budget to save leaves
        } // if there are scores to attain
      } // if we still have budget to save leaves
    }


    void optimize_diversity(const size_t k, const size_t lower_bnd = 1) {
      // ==== step 2: produce the generator network: done at init

      // ==== step 3: guess at most k sides of the generator that contain selected leaves
      // step 3.1: get all sides and nodes of the generator
      // accumulate all nodes of the generator that can have tree-paths to leaves
      const NodeVec gen_nodes_preorder = Gen.template nodes_with<preorder>(has_tree_path_to_leaf).template to_container<NodeVec>();
      DEBUG3(std::cout << "generator nodes with tree-paths to leaves: "<<gen_nodes_preorder<<'\n');

      // guess which at most k generator nodes have tree-paths to saved leaves
      for(const auto gsaved_nodes_preorder: mstd::make_subset_factory(gen_nodes_preorder, lower_bnd, k)) {
        DEBUG3(std::cout << "\n=== new guess! ===\n"<<gsaved_nodes_preorder.size() << " nodes with saved leaves below: "<<gsaved_nodes_preorder<<'\n');
        
        // clear the probabilities of the previous guess
        for(const auto guv: Gen.edges())
          guv.data().prob = 0;
       
        // ==== step 4: for each side S in the generator, compute proportion of switchings that the lowest edge of S is in
#warning "TODO: improve this using a dominator tree: not all switchings need to be iterated in order to compute the proportions!"
        for(const auto gswitching: SwitchingFactory<Generator, const NodeVec*>{&gsaved_nodes_preorder}) {
          Probability switching_prob = 1;
          for(const auto guv: gswitching)
            if(Generator::is_reti(guv.head()))
              switching_prob *= get_side_probability(guv);
          DEBUG4(std::cout << "switching "<<gswitching<<" has probability "<<switching_prob<<'\n');
          for(const auto guv: gswitching)
            guv.data().prob += switching_prob;
        }
        
        // ==== step 5: treat each side individually (DP over the sides to maximize diversity with our k leaves)
        optimize_diversity_for_sides(gsaved_nodes_preorder, k);
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
      DEBUG4(std::cout << "found non-trivial biconnected comp ("<<bcc.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(bcc) <<"\n");
      DEBUG4(bcc.print_summary(std::cout));
      AvgTreeEngine engine(bcc, std::forward<Utility>(util), &leaf_table);
      engine.optimize_diversity(k);
      DEBUG4(std::cout << "\nfinal tables: "<<leaf_table.result_table << "\n\n");
    } // for all nontrivial biconnected components bcc

    // if we don't have a table entry for the root, it means that the root is in a trivial BCC,
    // so we'll have to treat the tree-component of the root seperately
    if(not leaf_table.has_table(N.root(), false)) {
      // to get the tree component of the root, we run a DFS in which it is forbidden to enter nodes whose parents have an accu_table
      auto root_edges = N.edges([&](const NodeDesc x){ return (Net::in_degree(x) != 0) and leaf_table.has_table(Net::parent(x), false); }).to_container();
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
