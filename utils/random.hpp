
#pragma once

#include <iterator>
#include "utils.hpp"
#include "iter_bitset.hpp"

namespace PT{
  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double probability = 0.5)
  {
    return ((double)rand() / RAND_MAX) <= probability;
  }
  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6)
  {
    return rand() % sides; // yadda yadda, it's not 100% uniform
  }
  //! return the result of a 0/1-die with 'good_sides' good sides among its 'sides' sides
  inline bool throw_bw_die(const uint32_t good_sides, const uint32_t sides)
  {
    return throw_die(sides) < good_sides;
  }
  //! draw k integers from [0,n-1]
  std::iterable_bitset draw(uint32_t k, const uint32_t n)
  {
    assert(k <= n);
    std::iterable_bitset result(n);
    while(k > 0)
      result.set_kth_unset(throw_die(k--));
    return result;
  }

  //! get an iterator to a (uniformly) random item in the container
  template<class Container>
  typename Container::iterator get_random_iterator(Container& c)
  {
    return std::next(c.begin(), throw_die(c.size()));
  }


}
