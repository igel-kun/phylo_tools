
#pragma once

#include "subsets.hpp"
#include "linear_interval.hpp"

namespace mstd {
  // given a vector of indirections (iterators) to T, return a set of k indirections whose combined score is maximum
  template<OptionalContainerType _OutputContainer = void, IterableType Container, class Score, class Cmp = std::greater<>>
  auto brute_force(const linear_interval<uint32_t> bounds, Container& container, Score&& score, Cmp&& better = Cmp{}) {
    using Value = std::remove_const_t<value_type_of_t<Container>>;
    using Iter = iterator_of_t<Container>;
    // if the Container iterators are not random access, then we'll have to use the iterator-variant of the brute-force routine
    static constexpr bool container_ra = std::random_access_iterator<iterator_of_t<Container>>;
    DEBUG5(std::cout << "container "<<type_name<Container>()<<" is RandomAccess? "<<container_ra<<'\n');
    // by default, the output container has the same type as the input container
    using ContainerFromInput = std::conditional_t<ContainerType<Container>, Container, std::vector<Value>>;
    using OutputContainer = FirstNonVoid<_OutputContainer, std::remove_const_t<ContainerFromInput>>;
    using SubsetInternal = std::conditional_t<container_ra, OutputContainer, std::vector<Iter>>;
    using Subsets = BoundedSubsetFactory<Container, SubsetInternal>;
    DEBUG5(std::cout << "storing iters? "<< Subsets::store_iters << "\ninternal: "<<type_name<SubsetInternal>()<<'\n');

    std::pair<SubsetInternal, int64_t> result{};
    bool first = true;
    for(const auto S: Subsets{container, bounds}) {
      int64_t current;
      if constexpr (Subsets::store_iters) {
        DEBUG5(std::cout << "subset: "<<(S | std::ranges::views::transform(default_deref{}))<<'\n');
        static_assert(std::ranges::range<decltype(S | std::ranges::views::transform(default_deref{}))>);
        current = score(S | std::ranges::views::transform(default_deref{}));
      } else current = score(S);

      if(first || better(current, result.second)) {
        result.first = std::move(S);
        result.second = current;
        first = false;
      }
    }
    // if we used iterators but the user requested something else, we'll have to try and convert...
    if constexpr (not std::is_same_v<OutputContainer, SubsetInternal>) {
      std::pair<OutputContainer, int64_t> res{};
      res.second = result.second;
      if constexpr (Subsets::store_iters)
        append(res.first, result.first | std::ranges::views::transform(default_deref{}));
      else
        append(res.first, result.first);
      return res;
    } else return result;
  }
 
  template<OptionalIterableType _OutputContainer = void, IterableType Container, class Score, class Cmp = std::greater<>>
  auto brute_force(const uint32_t k, Container& container, Score&& score, Cmp&& better = Cmp{}) {
    return brute_force<_OutputContainer>(linear_interval{k,k}, container, std::forward<Score>(score), std::forward<Cmp>(better));
  }

  template<OptionalIterableType _OutputContainer = void, IterableType Container, class Score, class Cmp = std::greater<>>
  auto brute_force(const Container& container, Score&& score, Cmp&& better = Cmp{}) {
    return brute_force<_OutputContainer>(linear_interval<uint32_t>{0,UINT32_MAX}, container, std::forward<Score>(score), std::forward<Cmp>(better));
  }

}
