
#pragma once

#include <utility>

#include "subsets.hpp"
#include "solution_accu.hpp"

namespace mstd {
  // given a vector of indirections (iterators) to T, return a set of k indirections whose combined score is maximum
  template<OptionalContainerType OutputContainer_ = void, StrictIterableType Container, class ScoreFunc = SetSize, class Cmp = std::greater<>>
    requires (mstd::is_invocable_v<ScoreFunc, mstd::TR_ConstRefOK, const Container&>)
  auto brute_force(const linear_interval<uint32_t> subset_size_bounds, const Container& container,
      size_t keep_best_solutions = 1, ScoreFunc&& score = ScoreFunc{}, Cmp&& better = Cmp{}) {
    using Value = std::remove_const_t<value_type_of_t<Container>>;
    using Iter = iterator_of_t<const Container>;
    // if the Container iterators are not random access, then we'll have to use the iterator-variant of the brute-force routine
    static constexpr bool container_ra = std::random_access_iterator<iterator_of_t<Container>>;
    DEBUG5(std::cout << "container "<<type_name<Container>()<<" is RandomAccess? "<<container_ra<<'\n');
    // by default, the output container has the same type as the input container
    using ContainerFromInput = std::conditional_t<ContainerType<Container>, Container, std::vector<Value>>;
    using OutputContainer = FirstNonVoid<OutputContainer_, ContainerFromInput>;
    using SubsetInternal = std::conditional_t<container_ra, OutputContainer, std::vector<Iter>>;
    using Subsets = BoundedSubsetFactory<const Container, SubsetInternal>;
    DEBUG5(std::cout << "storing iters? "<< Subsets::store_iters << "\ninternal: "<<type_name<SubsetInternal>()<<'\n');
    static_assert(container_ra || Subsets::store_iters);
    using ScoreType = std::invoke_result_t<std::remove_cvref_t<ScoreFunc>, const Container&>;
    using SolAccu = SolutionAccumulator<SubsetInternal, ScoreType, Cmp>;

    SolAccu accu(keep_best_solutions);
    for(const auto S: Subsets{container, subset_size_bounds}) {
      if constexpr (Subsets::store_iters) {
        DEBUG5(std::cout << "subset: "<<(S | std::ranges::views::transform(default_deref{}))<<'\n');
        static_assert(std::ranges::range<decltype(S | std::ranges::views::transform(default_deref{}))>);
        accu.add(std::move(S), score(S | std::ranges::views::transform(default_deref{})));
      } else accu.add(std::move(S), score(S));
    }
    // if we used iterators but the user requested something else, we'll have to try and convert...
    if constexpr (not mstd::is_same_v<OutputContainer, SubsetInternal>) {
      SolutionAccumulator<OutputContainer, ScoreFunc, Cmp> out{keep_best_solutions};
      for(const auto& sol: accu.solutions) {
        if constexpr (Subsets::store_iters)
          out.add(sol.first | std::ranges::views::transform(default_deref{}), sol.second);
        else
          out.add(sol.first, sol.second);
      }
      return out;
    } else return accu;
  }
 
  template<OptionalIterableType OutputContainer_ = void, IterableType Container, class ScoreFunc, class Cmp = std::greater<>>
  auto brute_force(const uint32_t k, Container& container, size_t keep_best_solutions, ScoreFunc&& score, Cmp&& better = Cmp{}) {
    return brute_force<OutputContainer_>(
        linear_interval{k,k},
        container,
        keep_best_solutions,
        std::forward<ScoreFunc>(score),
        std::forward<Cmp>(better));
  }

  template<OptionalIterableType OutputContainer_ = void, IterableType Container, class ScoreFunc, class Cmp = std::greater<>>
  auto brute_force(const Container& container, size_t keep_best_solutions, ScoreFunc&& score, Cmp&& better = Cmp{}) {
    return brute_force<OutputContainer_>(
        linear_interval<uint32_t>{0,UINT32_MAX},
        container,
        keep_best_solutions,
        std::forward<ScoreFunc>(score),
        std::forward<Cmp>(better));
  }

}
