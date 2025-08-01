
#pragma once

#include "solution_accu.hpp"

#include "types.hpp"

#ifdef DFSCORO
#include "dfs_coro.hpp"
#else
#include "dfs.hpp"
#endif



namespace PT {
  // ========== TreeDiversity ==========
  // The TreeDiversity class is a phylogenetic diversity engine that can compute
  // the PD score of a tree or a network without reticulations
  // (I mean a structure that has been declared as a network but does not currently hold any reticulations)
  //
  // NOTE: [PG'05] prove that it is optimal to repeatedly select a leaf maximizing the current PD score

  // ------- TreeDiversity: helpers ---------
  // To support extended formulizations of diversity, we allow the leaves of the tree to be associated to
  // tables indicating cost-gain pairs.
  // The following two classes provide the infrastructure for that.
  // NoLeafTable: each leaf has a single leaf-entry, namely "cost 1, no additional gain"
  // LeafTable: this can store tables (x,y) so for the cost of x, we can get y additional score
  // Since the two classes have identical interface, they are drop-in replacable by each other.

  template<class _Weight> requires std::is_arithmetic_v<_Weight>
  struct ProtoLeafTable {
    // ------- static stuff --------
    using Weight = _Weight;
    // the level-DP assigns a table to each bridge (represented as a leaf in the BiconnectedComponent)
    //    mapping each i<k to the diversity that can be gained by saving exactly i leaves below
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, Weight>;
    // the AccuTable maps solution sizes to solutions of that size (best scoring ones, see mstd::SolutionAccumulator)
    using AccuTable = std::vector<SolutionAccu>;

    // ------- members --------
    size_t num_solutions; // number of different solutions to keep per k

    auto new_singleton_accu(const NodeDesc x) const { 
      SolutionAccu result{num_solutions};
      result.add(NodeVec{x}, 0);
      return result;
    }
    static void normalize_solutions_at(SolutionAccu& accu, const auto shift) {
      // add the delta between the best solution and 'score' to all solutions
      const auto delta = shift - accu.get_best().second;
      for(auto& [sol, score]: accu.solutions)
        score += delta;
    }
  };


  template<class _Weight> requires std::is_arithmetic_v<_Weight>
  struct NoLeafTable:
    public ProtoLeafTable<_Weight>
  {
    // ------- static stuff --------
    using Parent = ProtoLeafTable<_Weight>;
    using typename Parent::Weight;
    using typename Parent::SolutionAccu;
    using typename Parent::AccuTable;
    
    // ------- members --------
    using Parent::num_solutions;

    AccuTable root_table = {}; // for each i, store all solutions of size i at the root in a SolutionAccumulator
    
    // ------- construction & desctruction ---------
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: modification --------
    static void reset_use_counts() {}

    // ------- methods: query --------
    static bool has_table(const NodeDesc) { return true; }
    AccuTable& get_table(const NodeDesc) { return root_table; }

    // return the accu table for x -- if necessary, create with entries [0,...,sol_size]
    AccuTable& emplace_table(const NodeDesc x, const size_t sol_size) {
      root_table.reserve(sol_size + 1);
      while(root_table.size() <= sol_size)
        root_table.emplace_back(num_solutions);
      return root_table;
    }
    const AccuTable& get_accu_table(const NodeDesc) const { return root_table; }

    static Weight best_score_below(const NodeDesc x) { return 0; }
    static NodeWith<Weight> save_leaf(const NodeDesc x) { return {x, 0}; }

    // translate a node histogram to a SolutionAccumulator that can be added to the solution accumulator vector of the root
    // in the simple case, we can assume that each node occurs at most once in the histogram
    SolutionAccu accu_from_histogram(const auto& hist, const auto global_score) {
      SolutionAccu result{num_solutions};
      for(const auto [x, i]: hist) {
        assert(i == 1);
        result += Parent::new_singleton_accu(x);
      }
      Parent::normalize_solutions_at(result, global_score);

      DEBUG4(std::cout << "turned history "<<hist<<" into accu " << result<< '\n');
      return result;
    }

  };


  template<class _Weight, class Translate = mstd::IdentityFunction<NodeDesc>> // Translate = how to translate given nodes into indices of the result_table?
    requires std::is_arithmetic_v<_Weight>
  struct LeafTable:
    public ProtoLeafTable<_Weight>
  {
    // ------- static stuff --------
    using Parent = ProtoLeafTable<_Weight>;
    using typename Parent::Weight;
    using typename Parent::SolutionAccu;
    using typename Parent::AccuTable;
    // in the table, we store for each k the diversity attainable by saving k leaves; we'll also store how many leaves were already saved below
    using ResultTable = std::pair<AccuTable, size_t>;

    // ------- members --------
    using Parent::num_solutions;

    NodeMap<ResultTable> result_table; // we're mapping translated nodes to their tables
    [[ no_unique_address ]] Translate translate;

    // ------- construction & desctruction ---------
    // ------- operators --------
    // ------- methods: initialization --------
    
    // ------- methods: modification --------
        // resets the use-counters of all result_table entries to 0
    void reset_use_counts() {
      for(auto& [x, sol_with_counter]: result_table)
        sol_with_counter.second = 0;
    }

    // save the best leaf in the network below a given node
    NodeWith<Weight> save_leaf(const NodeDesc x) {
      const NodeDesc orig_x = translate(x);
      static_assert(not std::is_const_v<decltype(result_table)>);
      return {orig_x, access_next_diff(result_table, orig_x)};
    }

    // ------- methods: query --------
    bool has_table(const NodeDesc x, const bool translate_x = true) const { return test(result_table, translate_x ? translate(x) : x); }

    // return the accumulator-vector for x (create if necessary)
    AccuTable& emplace_table(const NodeDesc x, const size_t sol_size, const bool translate_x = true) {
      return mstd::append(result_table, translate_x ? translate(x) : x,
          std::piecewise_construct, std::tuple{sol_size + 1, num_solutions}, std::tuple{0}).first->second.first;
    }
    const AccuTable& get_table(const NodeDesc x, const bool translate_x = true) const {
      return result_table.at(translate_x ? translate(x) : x);
    }

    template<class Table, bool use_leaf = not std::is_const_v<Table>>
      requires mstd::is_same_v<Table, NodeMap<ResultTable>>
    static Weight access_next_diff(Table& tab, const NodeDesc x) {
      const auto iter = tab.find(x);
      if(iter != tab.end()) {
        auto& [x_table, saved_so_far] = iter->second;
        if(x_table.size() > saved_so_far + 1) {
          const auto& next_sols = x_table[saved_so_far + 1];
          const auto& current_sols = x_table[saved_so_far];
          if constexpr (use_leaf) 
            ++saved_so_far;
          const Weight best_next_sol = next_sols.get_best_or(0);
          const Weight best_current_sol = current_sols.get_best_or(0);
          return best_next_sol - best_current_sol;
        } else return 0;
      } else return 0;
    }

    // query best score below a given node
    auto best_score_below(const NodeDesc x) const {
      return access_next_diff(std::as_const(result_table), translate(x));
    }

    // translate a node histogram to a SolutionAccumulator that can be added to the solution accumulator vector of the root
    // NOTE: this will translate "take reticulation x 3 times" to the best solution below x using 3 leaves
    SolutionAccu accu_from_histogram(const auto& hist, const auto global_score) {
      SolutionAccu result{num_solutions};
      for(const auto [x, i]: hist) {
        const auto iter = result_table.find(x);
        if(iter != result_table.end()) {
          auto& x_table = iter->second.first;
          assert(x_table.size() > i);
          result += x_table[i];
        } else {
          assert(i == 1);
          result += Parent::new_singleton_accu(x);
        }
      }
      Parent::normalize_solutions_at(result, global_score);
      DEBUG4(std::cout << "turned history "<<hist<<" into accu " << result<< '\n');
      return result;
    }

  };


  // The following class is essnetial for calculating PD scores on trees.
  //
  // given a node x in a tree, this class can return the PD score of the highest-scoring leaf q below x
  //  as well as the outgoing edge one has to take to get to q
  // for each node, we store (child, weight)-pairs in a sorted_vector
  // when a leaf is takes, all relevant pairs are updated
  template<StrictPhylogenyType Net, class Utility, class LeafTable = NoLeafTable<typename Utility::Weight>>
    requires (not std::is_reference_v<LeafTable> and not std::is_reference_v<Utility>)
  struct PDTreeScoreMap {
    // ------- static stuff --------
    using Weight = typename std::remove_pointer_t<LeafTable>::Weight;
    using Edge = typename Net::Edge;
    // each node is mapped to the additional score attainable by taking a leaf below it, and in which direction to go to find it
    using GetSecond = mstd::selector<1>;
    using SecondSmaller = mstd::PointwiseCompose<GetSecond, std::less<>>;
    using SecondGreater = mstd::PointwiseCompose<GetSecond, std::greater<>>;
    using NodesByScore = std::multiset<NodeWith<Weight>, SecondGreater>;
    // NOTE: the directions sorted vectors will have the largest element last, so use back() and pop_back()
    using ScoredDirections = mstd::sorted_vector<NodeWith<Weight>, SecondSmaller>;
    using SideScoreMap = NodeMap<ScoredDirections>;
    using SolutionAccu = mstd::SolutionAccumulator<NodeVec, Weight>;
    using AccuTable = std::vector<SolutionAccu>;
    using NodeHistogram = NodeMap<size_t>;

    // ------- members --------
    [[ no_unique_address ]] Utility util;
    [[ no_unique_address ]] LeafTable leaf_table;
    SideScoreMap scorable;

    // ------- construction & desctruction ---------
    PDTreeScoreMap() = default;

    template<class UtilInit, class... Args> requires (not mstd::is_any_of<UtilInit, PDTreeScoreMap>)
    PDTreeScoreMap(UtilInit&& util_init, Args&&... args):
      util{std::forward<UtilInit>(util_init)},
      leaf_table{std::forward<Args>(args)...}
    {}

    // ------- operators --------
    // ------- methods: initialization --------
    void setup_scorable_below(const NodeDesc v) {
      // NOTE: we're using a custom node traversal that has no seenset and uses the constructed map as a forbidden set
      for(const NodeDesc x: Traversal<postorder, Net, NodeDesc, void>{v}) {
        ScoredDirections& score_dir = get_directions(x);
        score_dir.reserve(Net::out_degree(x));
        
        for(const auto& y: Net::children(x)) {
          const auto weight = util.weight(Edge(x,y));
          auto y_scorable = best_score(y);
          y_scorable += weight;
          score_dir.emplace(y, y_scorable);
        }
        DEBUG4(std::cout << "set up direction vector of "<<x<<": "<<score_dir<<'\n');
      }
    }

    // ------- methods: modification --------
    // save the leaf with maximum diversity gain below u in N
    // NOTE: this also updates the 'scorable' map
    NodeWith<Weight> save_best_leaf_below(const NodeDesc u) {
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
        const auto v_new_score = best_score(v);
        DEBUG4(std::cout << "new score of "<<u<<" via "<<v<<" is "<<v_new_score<<'\n');
        directions.emplace(v, v_new_score);

        // the result is the leaf we got from the recursion and the score we saved initially
        return {saved_leaf, score};
      } else return get_leaf_table().save_leaf(u);
    }

    void clear() {
      scorable.clear();
      get_leaf_table().reset_use_counts(); // reset the use counts from the previous iteration
    }


    // ------- methods: query --------
    const auto& get_leaf_table() const { return mstd::access(leaf_table); }
    auto& get_leaf_table() { return mstd::access(leaf_table); }

    ScoredDirections& get_directions(const NodeDesc v) { return scorable.try_emplace(v).first->second; }

    size_t num_solutions() const { return get_leaf_table().num_solutions; }

    // return the best score below a node y in the network by following the scorable-pointers
    Weight best_score(const NodeDesc y) const {
      const auto y_it = scorable.find(y);
      if(y_it == scorable.end()) return std::numeric_limits<Weight>::lowest();
      if(y_it->second.empty()) return get_leaf_table().best_score_below(y);
      return y_it->second.back().second; // get the best score achievable below y
    }

    Weight best_score_for_node(const NodeDesc u) const {
      const auto iter = scorable.find(u);
      if(iter != scorable.end()) {
        const auto& directions = iter->second;
        return directions.empty() ? -1 : directions.back().second; 
      } else return -1;
    }

    friend std::ostream& operator<<(std::ostream& os, const PDTreeScoreMap& x) {
      return os << (x.scorable | std::ranges::views::filter([&](const auto& ux){ return not ux.second.empty();}) )<<'\n';
    }
  };



  // ------- TreeDiversity: main class ---------
  template<StrictPhylogenyType Net, class Utility, class LeafTable = NoLeafTable<typename Utility::Weight>>
  struct TreeDiversity {
    // ------- static stuff --------
    using ScoreMap = PDTreeScoreMap<Net, Utility, LeafTable>;
    using Weight = typename ScoreMap::Weight;
    using NodeHistogram = typename ScoreMap::NodeHistogram;
    using NodesByScore = typename ScoreMap::NodesByScore;
    
    // ------- members -------- 
  protected:
    ScoreMap score_map;
    NodeDesc root;

    // ------- construction & desctruction ---------
  public:
    TreeDiversity() = delete;

    template<class UtilInit, class... Args>
    TreeDiversity(const NodeDesc _root, UtilInit&& util_init, Args&&... args):
      score_map(std::forward<UtilInit>(util_init), std::forward<Args>(args)...),
      root(_root)
    { setup_score_map(); }

    template<class... Args>
    TreeDiversity(const Net& N, Args&&... args):
      TreeDiversity(N.root(), std::forward<Args>(args)...)
    {}

    // ------- operators --------
    // ------- methods: initialization --------
    // setup the score map
    void setup_score_map() { score_map.setup_scorable_below(root); }

    // ------- methods: query --------
    auto& get_root_table(const size_t sol_size) { return score_map.get_leaf_table().emplace_table(root, sol_size); }

    // ------- methods: modification --------
    void optimize_diversity(const size_t k) {
      NodeHistogram solution;
      Weight global_score = 0;
      for(size_t i = 0; i < k; ++i) {
        const auto [leaf, score] = score_map.save_best_leaf_below(root);
        ++solution[leaf];
        global_score += score;
        DEBUG4(std::cout << "saving leaf "<<leaf<<" (score "<<score<<", total: "<<global_score<<")\n");
      }
      DEBUG4(std::cout << "adding solution: "<<solution<<" with diversity "<<global_score<<'\n');
      auto& accu_table = get_root_table(k);
      DEBUG4(std::cout << "got accu-table: "<<accu_table<<'\n');
      accu_table[k].add(score_map.leaf_table.accu_from_histogram(solution, global_score));
    }

  };
  
  // ------- TreeDiversity: factories ---------
  // ------- TreeDiversity: concepts ---------
  // ------- TreeDiversity: deduction guides ---------
  // ------- TreeDiversity: defaults ---------

}
