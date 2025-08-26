
#pragma once

#include "sorted_vector.hpp"
#include "linear_interval.hpp"

namespace mstd {

  // GetScoreType<T, Solution> returns the type of the score given to a Solution by T, or T itself if it's not callable with a Solution
  // In the special case that T is void, the score type is size_t
  template<class T, class Solution> struct GetScoreType { using type = T; };
  template<class Solution> struct GetScoreType<void, Solution> { using type = size_t; };
  template<class T, class Solution> requires std::is_invocable_v<T, Solution>
  struct GetScoreType<T, Solution> { using type = std::invoke_result_t<T, Solution>; };

  // keep only a certain number of best solutions
  // NOTE: if ScoreExtracterOrScore is a type that cannot be called with a Solution, then ScoreExtracter *ITSELF* is used as ScoreType
  //        (unless overwritten by ScoreType_)
  template<class Solution, class ScoreExtracterOrScore = void, class ScoreCmp = std::greater<>, class ScoreType_ = void>
  struct SolutionAccumulator {
    using ScoreTypeFromExtracter = typename GetScoreType<ScoreExtracterOrScore, Solution>::type;
    using ScoreType = mstd::FirstNonVoid<ScoreType_, ScoreTypeFromExtracter>;
    static_assert(std::is_invocable_v<ScoreCmp, ScoreType, ScoreType>);
    using SolutionWithScore = std::pair<Solution, ScoreType>;
    using GetSecond = mstd::selector<1>;
    using CmpSeconds = mstd::PointwiseCompose<GetSecond, ScoreCmp>;
    static_assert(std::is_invocable_v<CmpSeconds, SolutionWithScore, SolutionWithScore>);
    using SolutionVec = mstd::sorted_vector<SolutionWithScore, CmpSeconds>;
    static constexpr bool has_extracter = std::is_invocable_v<ScoreExtracterOrScore, Solution>;

    size_t num_solutions = 1;
    SolutionVec solutions;
    [[no_unique_address]] std::conditional_t<has_extracter, ScoreExtracterOrScore, mstd::monostate> extracter;

    template<class ScoreCmpInit = ScoreCmp>
    SolutionAccumulator(size_t num_solutions_to_keep, ScoreCmpInit _cmp = ScoreCmpInit()):
      num_solutions(num_solutions_to_keep),
      solutions(CmpSeconds{GetSecond{}, ScoreCmp{std::forward<ScoreCmpInit>(_cmp)}})
    {assert(num_solutions > 0);}

    template<class ScoreCmpInit, class ScoreExtracterInit>
    SolutionAccumulator(size_t num_solutions_to_keep, ScoreCmpInit _cmp, ScoreExtracterInit _extracter):
      num_solutions(num_solutions_to_keep),
      solutions(CmpSeconds{GetSecond{}, ScoreCmp{std::forward<ScoreCmpInit>(_cmp)}}),
      extracter(std::forward<ScoreExtracterInit>(_extracter))
    {assert(num_solutions > 0);}

    SolutionAccumulator non_empty_cross_product_with(const SolutionAccumulator& other) const {
      SolutionAccumulator result{num_solutions};
      assert(not solutions.empty());
      assert(not other.solutions.empty());
      for(const auto& sol1: solutions) {
        bool broke_on_first = true;
        for(const auto& sol2: other.solutions) {
          // step 1: combine solutions
          Solution combined_sol = sol1.first;
          mstd::append(combined_sol, sol2.first);
          // step 2: compute new score and add to resulting accumulator
          // NOTE: if we don't have an extracter, we assume that the new score is the sum of the old scores
          // NOTE: if, at any point, we're no longer adding new solutions, we can step the process -- since solutions are sorted, we won't see any better
          if constexpr (has_extracter) {
            if(not result.add(std::move(combined_sol))) break;
          } else if(not result.add(std::move(combined_sol), sol1.second + sol2.second)) break;
          broke_on_first = false;
        }
        if(broke_on_first) break; // if sol1 combined with the best sol2 was already not enough to add a new solution, then we can abandon the whole thing
      }
      return result;
    }
    // adding two accumulators means adding all solutions of the second to all solutions of the first (cross product)
    // NOTE: if you want to merge 2 accumulators (retaining the best score of both), just use add(other)
    SolutionAccumulator operator+(const SolutionAccumulator& other) const {
      if(solutions.empty()) return other;
      if(other.solutions.empty()) return *this;
      return non_empty_cross_product_with(other);
    }
    auto& operator+=(const SolutionAccumulator& other) { return *this = std::move(*this + other); }
    
    // add a flat value to all solutions
    auto& operator+=(const ScoreType& bonus) {
      for(auto it = solutions.begin(); it != solutions.end(); ++it)
        it->second += bonus;
      return *this;
    }

    auto& get_worst() const { assert(not solutions.empty()); return back(solutions); }
    auto& get_best() const { assert(not solutions.empty()); return front(solutions); }
    ScoreType get_best_or(const auto& otherwise) const { if(not solutions.empty()) return get_best().second; else return otherwise; }

    template<class S> requires mstd::is_convertible_v<S, Solution>
    bool add(S&& sol, const ScoreType score) {
      if (solutions.size() == num_solutions) {
        // If we have reached capacity, check against worst known solution and insert if better
        // Find worst (last) element
        const ScoreType worst = get_worst().second;

        // If the new entry is better than the worst one, replace it
        if (mstd::access(solutions.key_comp().f2)(score, worst)) {
          solutions.pop_back(); // remove the worst one (was last)
          solutions.emplace(std::forward<S>(sol), score);
        } else return false;
      } else solutions.emplace(std::forward<S>(sol), score); // If we haven't reached capacity, insert normally in sorted position
      return true;
    }

    // Add using score extractor — only enabled if ScoreExtracter is not void
    template<class S> requires (mstd::is_convertible_v<S, Solution> and has_extracter)
    bool add(S&& sol) { return add(std::forward<S>(sol), extracter(sol)); }

    // convert sol to a Solution using mstd::append and call add for it
    template<class S> requires (not mstd::is_convertible_v<S, Solution>)
    bool add(S&& sol, ScoreType score) {
      Solution s;
      mstd::append(s, std::forward<S>(sol));
      return add(std::move(s), score);
    }

    template<class S> requires mstd::is_same_v<S, SolutionWithScore>
    bool add(S&& sol) {
      const ScoreType score = sol.second;
      return add(std::forward<S>(sol).first, score);
    }
    template<class S> requires mstd::is_same_v<S, SolutionWithScore>
    bool add(S&& sol, const ScoreType& bonus) {
      const ScoreType score = sol.second;
      return add(std::forward<S>(sol).first, score + bonus);
    }

    // merge 2 solution accumulators (retain best solutions from both)
    template<class Accu> requires mstd::is_same_v<Accu, SolutionAccumulator>
    void add(Accu&& accu) {
      for(auto&& sol: std::forward<Accu>(accu).solutions)
        add(std::forward<decltype(sol)>(sol));
    }
    template<class Accu> requires mstd::is_same_v<Accu, SolutionAccumulator>
    void add(Accu&& accu, const ScoreType& bonus) {
      for(auto&& sol: std::forward<Accu>(accu).solutions)
        add(std::forward<decltype(sol)>(sol).first, bonus);
    }

/*
    template<class S> requires mstd::is_convertible_v<S, Solution>
    void add(S&& sol, ScoreType score) {
      auto comp = [&](const auto& a, const auto& b) { return cmp(b.second, a.second);};
      if (solutions.size() < solutions.capacity()) {
        solutions.emplace_back(std::forward<S>(sol), score);
        std::push_heap(solutions.begin(), solutions.end(), comp);
      } else if (cmp(score, solutions.front().second)) {
        std::pop_heap(solutions.begin(), solutions.end(), comp);
        solutions.back() = {std::forward<S>(sol), score};
        std::push_heap(solutions.begin(), solutions.end(), comp);
      }
    }
*/


    friend std::ostream& operator<<(std::ostream& os, const SolutionAccumulator& accu) {
      return os << "[Accu ("<<accu.num_solutions<<"): "<<accu.solutions<<']';
    }
  };

}
