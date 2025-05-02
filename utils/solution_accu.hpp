
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
  // NOTE: if ScoreExtracter is a type that cannot be called with a Solution, then ScoreExtracter *ITSELF* is used as ScoreType
  //        (unless overwritten by _ScoreType)
  template<class Solution, class ScoreExtracter = void, class ScoreCmp = std::greater<>, class _ScoreType = void>
  struct SolutionAccumulator {
    using ScoreTypeFromExtracter = typename GetScoreType<ScoreExtracter, Solution>::type;
    using ScoreType = mstd::FirstNonVoid<_ScoreType, ScoreTypeFromExtracter>;
    using SolutionWithScore = std::pair<Solution, ScoreType>;
    using GetSecond = mstd::selector<1>;
    using CmpSeconds = mstd::PointwiseCompose<GetSecond, ScoreCmp>;
    using SolutionVec = mstd::sorted_vector<SolutionWithScore, CmpSeconds>;
    static_assert(std::invocable<CmpSeconds, SolutionWithScore, SolutionWithScore>);

    SolutionVec solutions;
    [[no_unique_address]] std::conditional_t<std::is_void_v<ScoreExtracter>, mstd::IgnoreFunction<>, ScoreExtracter> extracter;

    template<class ScoreCmpInit = ScoreCmp>
    SolutionAccumulator(size_t num_solutions_to_keep = 1, ScoreCmpInit _cmp = ScoreCmpInit()):
      solutions(CmpSeconds{GetSecond{}, ScoreCmp{std::forward<ScoreCmpInit>(_cmp)}})
    { solutions.reserve(num_solutions_to_keep); }

    template<class ScoreCmpInit, class ScoreExtracterInit>
    SolutionAccumulator(size_t num_solutions_to_keep, ScoreCmpInit _cmp, ScoreExtracterInit _extracter):
      solutions(CmpSeconds{GetSecond{}, ScoreCmp{std::forward<ScoreCmpInit>(_cmp)}}),
      extracter(std::forward<ScoreExtracterInit>(_extracter))
    { solutions.reserve(num_solutions_to_keep); }

    // Add using score extractor — only enabled if ScoreExtracter is not void
    template<class S> requires (mstd::is_convertible_v<S, Solution> && (not std::is_void_v<ScoreExtracter>))
    void add(S&& sol) { add(std::forward<S>(sol), extracter(sol)); }

    template<class S> requires mstd::is_convertible_v<S, Solution>
    void add(S&& sol, ScoreType score) {
      if (solutions.size() == solutions.capacity()) {
        // If we have reached capacity, check against worst known solution and insert if better
        // Find worst (last) element
        const ScoreType worst = solutions.back().second;

        // If the new entry is better than the worst one, replace it
        if (mstd::access(solutions.key_comp().f2)(score, worst)) {
          solutions.pop_back(); // remove the worst one (was last)
          solutions.emplace(std::forward<S>(sol), score);
        }
      } else solutions.emplace(std::forward<S>(sol), score); // If we haven't reached capacity, insert normally in sorted position
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
    template<class S> requires (not mstd::is_convertible_v<S, Solution>)
    void add(S&& sol, ScoreType score) {
      Solution s;
      s.reserve(sol.size());
      for(auto&& i: sol) mstd::append(s, std::forward<decltype(i)>(i));
      add(std::move(s), score);
    }

  };

}
