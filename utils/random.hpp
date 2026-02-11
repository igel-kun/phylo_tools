
#pragma once

#include <iterator>
#include <random>
#include <ranges> // for iota_view
#include "utils.hpp"
#include "iter_bitset.hpp"

namespace mstd {
  std::uniform_real_distribution<double> zero_one_uniform(0.0, 1.0);
  std::mt19937 rand_engine(std::random_device{}());

  //! return the result of a coin flip whose 1-side has probability 'probability' of coming up
  inline bool toss_coin(const double& probability = 0.5) {
    return zero_one_uniform(rand_engine) <= probability;
  }

  //! return the result of throwing a die with 'sides' sides [0,sides-1]
  inline uint32_t throw_die(const uint32_t sides = 6) {
    return zero_one_uniform(rand_engine) * sides; // truncate after multiplying by 'sides'
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
    } else append(result, std::ranges::iota_view{0u,n-1});
  }

  // probs to Paul Crowley for this one: https://stackoverflow.com/questions/311703/algorithm-for-sampling-without-replacement/67850443#67850443
  template<mstd::VectorType Set = std::vector<uint32_t>>
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

  //! draw k DISTINCT integers from [0,n-1]
  template<class Set = std::vector<uint32_t>>
  void draw(const uint32_t k, const uint32_t n, Set& result) {
    if constexpr (mstd::VectorType<Set>) {
      if(k < mstd::config::cardchoose_threshold)
        cardchoose(k, n, result);
      else
        fisher_yates_choose(k, n, result);
    } else fisher_yates_choose(k, n, result);
  }

  //! draw k DISTINCT integers from [0,n-1]
  template<class Set = std::vector<uint32_t>>
  Set draw(const uint32_t k, const uint32_t n) {
    Set result;
    draw(k, n, result);
    return result;
  }


  // draw k (not necessarily distinct) items from the container c
  template<mstd::IndexibleType Vec, mstd::IterableType Container> requires (not std::is_pointer_v<Vec>)
  void sample(Container&& c, const size_t k, Vec& result) {
    std::ranges::sample(c, std::back_inserter(result), k, rand_engine);
  }
  template<class T, mstd::IterableType Container>
  void sample(Container&& c, const size_t k, T* const result) {
    std::ranges::sample(c, mstd::PointerIterWrapper<T*>(result), k, rand_engine);
  }
  template<mstd::IndexibleType Vec, mstd::IterableType Container>
  Vec sample(Container&& c, const size_t k) {
    Vec result;
    sample(std::forward<Container>(c), k, result);
    return result;
  }


  // reservoir sampling for getting k random **iterators** from an unknown number of samples
  template<mstd::IndexibleType Vec, mstd::IterableType Container>
    requires std::is_constructible_v<mstd::value_type_of_t<Vec>, mstd::iterator_of_t<Container>>
  Vec reservoir_sampling(Container&& c, const size_t k) {
    Vec result;
    const auto _end = std::end(c);
    auto it = std::forward<Container>(c).begin();
    size_t i = 0;
    while((++i <= k) and (it != _end)) {
      append(result, it);
      ++it;
    }
    while(it != _end){
      const size_t rnd = throw_die(i++);
      if(rnd < k) result[rnd] = it;
      ++it;
    }
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

  template<mstd::IterableType Container> requires (not mstd::IterableTypeWithSize<Container>)
  auto get_random_iterator(Container&& c) {
    using Iter = decltype(std::forward<Container>(c).begin());
    using Opt = std::optional<Iter>;
    if(!c.empty()) {
      return reservoir_sampling<mstd::singleton_set<Opt>>(std::forward<Container>(c), 1).front();
    } else throw std::logic_error("trying to pick random element from empty container");
  }


  //! get an iterator to a random item in the container, except a given iterator
  // NOTE: to this end, start at the second item and replace the iterator by begin() if it is hit
  template<mstd::IterableType Container>
  auto get_random_iterator_except(Container&& c, const auto& _except, const size_t container_size) {
    if((container_size >= 2) || (_except != std::begin(c))) {
      if(_except != std::end(c)) {
        const auto result = std::next(std::begin(c), 1 + throw_die(container_size - 1));
        // result can never equal begin(), but can hit except
        if(result == _except) 
          return std::begin(std::forward<Container>(c));
        else return result;
      } else return get_random_iterator(std::forward<Container>(c));
    } else throw std::logic_error{"no choosable item in container"};
  }

  template<mstd::IterableTypeWithSize Container>
  auto get_random_iterator_except(Container&& c, const auto& _except) {
    return get_random_iterator_except(std::forward<Container>(c), _except, c.size());
  }



  // The following classes make decisions, as in, they return a number from a range:
  // (1) a random-decider, that just draws randomly and
  // (2) a enumeration-decider, that remembers all decisions and can be advanced using operator++ to make the "next" decision in a lexicographic order

  // a random decider, returning a random decision each time
  // operator++ is noop
  template<class ZeroOneDistribution = std::uniform_real_distribution<double>>
  struct random_decider {
    using T = std::remove_reference_t<decltype(std::declval<ZeroOneDistribution>()(rand_engine))>;
    ZeroOneDistribution dist;

    auto operator()(const auto& from, const auto& to) const {
      const auto rnd = dist(rand_engine);
      return from + std::round(rnd * to - rnd * from);
    }
    void operator++() const {}
  };

  // a deterministic decider, used to enumerate all possible decisions
  // use operator++ to signal the next iteration
  template<class... Ts> requires (sizeof...(Ts) != 0)
  struct enumerating_decider {
    // if we need only make a single type of decisions, use that type, otherwise use a std::variant
    using T = std::conditional_t<sizeof...(Ts) == 1, mstd::FirstTypeOf<Ts...>, std::variant<Ts...>>;
    // a past decision is a pair (x,y) with x = last decision, y = maximum decidable number
    using Decision = std::pair<T, T>;
    using DecisionVec = std::vector<Decision>;

    DecisionVec past_decisions;
    size_t current_index = 0;

    void operator++() {
      if(not past_decisions.empty()) {
        // forget all decisions after the current one
        past_decisions.resize(current_index + 1);
        // advance the last decision (possibly advancing all others)
        while(1) {
          auto& last_decision = past_decisions.back();
          ++last_decision.first;
          if(last_decision.first == last_decision.second) {
            past_decisions.pop_back();
            if(past_decisions.empty()) break;
          } else break;
        }
        // reset the current position index
        current_index = 0;
      }
    }

    auto operator()(const auto& from, const auto& to) {
      if(current_index >= past_decisions.size()) {
        // we need to make a new decision
        append(past_decisions, from, to);
        ++current_index;
        return from;
      } else return past_decisions[current_index++]; // we need to return the (advanced) decision stored in the DecisionVec
    }
  };

}
