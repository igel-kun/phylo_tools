
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
  template<class Container>
  std::iterator_of_t<Container> get_random_iterator(Container&& c)
  {
    assert(!c.empty());

    std::iterator_of_t<Container> result = c.begin();
    const int k = throw_die(c.size());
    std::cout << "incrementing "<<k<<" times (vs size of "<<c.size()<<")\n";
    for(int i = k; i; --i){
      ++result;
      std::cout << *result << '\n';
    }
    return result;
    //return std::next(c.begin(), throw_die(c.size()));
  }
  //! get an iterator to a (uniformly) random item in the container, except a given iterator
  template<class Container>
  std::iterator_of_t<Container> get_random_iterator_except(Container&& c, const std::iterator_of_t<Container> _except)
  {
    assert((c.size() >= 2) || (_except == c.end()));
    std::iterator_of_t<Container> result = c.begin();
    std::advance(result, throw_die(c.size()-1));
    if(result == _except)
      return std::prev(c.end());
    else
      return result;
  }


}
