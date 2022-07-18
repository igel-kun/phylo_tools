
#pragma once

#include <iterator>
#include "utils.hpp"
#include "iter_bitset.hpp"

namespace PT{
  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double& probability = 0.5)
  {
    return (static_cast<double>(rand()) / RAND_MAX) <= probability;
  }
  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6)
  {
    return rand() % sides; // yadda yadda, it's not 100% uniform for tiny values of RAND_MAX...
  }
  //! return the result of a 0/1-die with 'good_sides' good sides among its 'sides' sides
  inline bool throw_bw_die(const uint32_t good_sides = 1, const uint32_t sides = 2)
  {
    return throw_die(sides) < good_sides;
  }
  //! draw k integers from [0,n-1]
  template<class Set>
  Set& draw(uint32_t k, const uint32_t n, Set& result)
  {
    assert(k <= n);
    std::vector<uint32_t> throws(k);
    while(k > 0) throws[k] = throw_die(k--);
    std::sort(throws.begin(), throws.end());
    uint32_t offset = 0;
    for(uint32_t i: throws)
      result.insert(i + offset++);
    return result;
  }

  //! get an iterator to a (uniformly) random item in the container
  template<mstd::IterableType Container>
  auto get_random_iterator(Container&& c, const size_t container_size) {
    assert(!c.empty());
    return std::next(c.begin(), throw_die(container_size));
  }

  template<mstd::IterableTypeWithSize Container>
  auto get_random_iterator(Container&& c) { return get_random_iterator(std::forward<Container>(c), c.size()); }

  //! get an iterator to a (uniformly) random item in the container, except a given iterator
  template<mstd::IterableType Container>
  auto get_random_iterator_except(Container&& c, const auto& _except, const size_t container_size) {
    assert((container_size >= 2) || (_except == std::end(c)));
    auto result = std::begin(std::forward<Container>(c));
    std::advance(result, throw_die(container_size - 1));
    if(result == _except) ++result;
    return result;
  }

  template<mstd::IterableTypeWithSize Container>
  auto get_random_iterator_except(Container&& c, const auto& _except)
  {
    return get_random_iterator_except(std::forward<Container>(c), _except, c.size());
  }


}
