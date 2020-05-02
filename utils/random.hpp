
#pragma once

#include <iterator>
#include "utils.hpp"
#include "iter_bitset.hpp"

namespace PT{
  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double& probability = 0.5)
  {
    return ((double)rand() / RAND_MAX) <= probability;
  }
  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6)
  {
    return rand() % sides; // yadda yadda, it's not 100% uniform for tiny values of RAND_MAX...
  }
  //! return the result of a 0/1-die with 'good_sides' good sides among its 'sides' sides
  inline bool throw_bw_die(const uint32_t good_sides, const uint32_t sides)
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
  template<class Container>
  typename Container::iterator get_random_iterator(Container& c)
  {
    assert(!c.empty());
    return std::next(c.begin(), throw_die(c.size()));
  }
  //! get an iterator to a (uniformly) random item in the container, except a given iterator
  template<class Container>
  typename Container::iterator get_random_iterator_except(Container& c, const typename Container::iterator _except)
  {
    assert(c.size() >= 2);
    size_t k = throw_die(c.size() - 1);
    const size_t except_dist = std::distance(c.begin(), _except);
    std::cout << "got container "<<c<<"\ngetting item #"<<k<<" avoiding "<<*_except<<std::endl;
    if(k < except_dist)
      return std::next(c.begin(), k);
    else
      return std::next(_except, k - except_dist + 1);

  }


}
