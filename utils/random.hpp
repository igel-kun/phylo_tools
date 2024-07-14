
#pragma once

#include <iterator>
#include "utils.hpp"
#include "iter_bitset.hpp"

namespace PT{
  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double& probability = 0.5) {
    return static_cast<double>(rand()) <= probability * RAND_MAX;
  }

  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6) {
    return rand() % sides; // yadda yadda, it's not 100% uniform for tiny values of RAND_MAX...
  }
  
  //! return the result of a 0/1-die with 'good_sides' good sides among its 'sides' sides
  inline bool throw_bw_die(const uint32_t good_sides = 1, const uint32_t sides = 2) {
    return throw_die(sides) < good_sides;
  }

  //! draw k DISTINCT integers from [0,n-1]
  //NOTE: if k >= n, we draw all n integers
  template<class Set = std::vector<uint32_t>>
  void fisher_yates_choose(const uint32_t k, uint32_t n, Set& result) {
    if(n < k) {
      // we use a variant of Fisher-Yates shuffle, tracking only the changed elements of [0,..,n-1]
      // (see https://stackoverflow.com/questions/196017/unique-non-repeating-random-numbers-in-o1#comment138160976_44884435)
      // entry track[x] = y means that we've replaced x with y in an iteration before
      std::unordered_map<uint32_t, uint32_t> track;
      for(uint32_t i = 0; i != k; ++i) {
        // step 1: get a random index between i and n
        //         NOTE: we're decreasing n with each throw since the previous numbers become unavailable
        const uint32_t index = throw_die(n--) + i;
        // step 2: check if the index has been drawn before
        const auto [iter, success] = track.emplace(index, i);
        // if so, select the number that index has been replaced with
        if(!success) {
          append(result, iter->second);
          // remember to update the replacement
          iter->second = i;
        } else append(result, index);
      }
    } else append(result, std::ranges::iota_view{0,n-1});
  }

  // probs to Paul Crowley for this one: https://stackoverflow.com/questions/311703/algorithm-for-sampling-without-replacement/67850443#67850443
  template<VectorType Set = std::vector<uint32_t>>
  void cardchoose(const uint32_t k, const uint32_t n, Set& result) {
    const uint32_t t = n - k + 1;
    result.reserve(k);
    for(uint32_t i = 0; i < k; ++i) {
      const uint32_t r = throw_die(t + i);
      append(result, (r < t) ? r : result[r - t]);
    }
    std::ranges::sort(result);
    for(uint32_t i = 0; i < k; ++i) 
      result[i] += i;
  }

  template<class Set = std::vector<uint32_t>>
  void draw(const uint32_t k, const uint32_t n, Set& result) {
    if constexpr (VectorType<Set>) {
      if(k < config::cardchoose_threshold)
        cardchoose(k, n, result);
      else
        fisher_yates_choose(k, n, result);
    } else fisher_yates_choose(k, n, result);
  }

  template<class Set = std::vector<uint32_t>>
  Set draw(const uint32_t k, const uint32_t n) {
    Set result;
    draw(k, n, result);
    return result;
  }


  template<mstd::IndexibleType Vec, mstd::IterableType Container>
  void sample(Container&& c, const size_t k, Vec& result) {
    std::ranges::sample(c, std::back_inserter(result), k, std::mt19937{std::random_device{}()});
  }

/*
  // reservoir sampling for getting k random iterators from an unknown number of samples
  template<mstd::IndexibleType Vec, mstd::IterableType Container>
  void reservoir_sampling(Container&& c, const size_t k, Vec& result) {
    auto it = std::begin(c);
    size_t i = 0;
    while(++i <= k) {
      if(it != std::end(c)) {
        append(result, it);
      } else return;
      std::advance(it);
    }
    while(it != std::end(c)){
      const size_t rnd = throw_die(i++);
      if(rnd < k) result[rnd] = it;
      std::advance(it);
    }
  }
  */
  template<mstd::IndexibleType Vec, mstd::IterableType Container>
  Vec sample(Container&& c, const size_t k) {
    Vec result;
    sample(std::forward<Container>(c), k, result);
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

  //! get an iterator to a random item in the container, except a given iterator
  //NOTE: the item following/preceeding the forbidden item is twice as likely to be picked...
  template<mstd::IterableType Container>
  auto get_random_iterator_except(Container&& c, const auto& _except, const size_t container_size) {
    assert((container_size >= 2) || (_except != std::begin(c)));
    auto result = std::begin(std::forward<Container>(c));
    std::advance(result, throw_die(container_size - 1));
    // if we hit _except, then take the next item
    if(result == _except) ++result;
    // if _except was the last item and we hit it, then take the item before _except
    if(result == std::end(c)) result = std::next(_except, -1);
    return result;
  }

  template<mstd::IterableTypeWithSize Container>
  auto get_random_iterator_except(Container&& c, const auto& _except) {
    return get_random_iterator_except(std::forward<Container>(c), _except, c.size());
  }


}
