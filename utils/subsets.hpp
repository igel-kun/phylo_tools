
// infrastructure for enumerating subsets of a set X such that all subsets of an enumerated set have already been enumerated before

#include "stl_utils.hpp"
#include "optional_tuple.hpp"
#include "set_interface.hpp"
#include "iter_bitset.hpp"
#include "linear_interval.hpp"

#pragma once

namespace mstd {
  /* iterating over all subsets S of X with |S| in the interval [a,b]
   * theory:
   *  1. we iterate from smaller sets to larger sets, starting with all subsets of size a
   *  2. given a set 000110101101011111000, the next set is obtained by
   *    (a) adding 1000 - that is: adding 1 << x where x = #trailing zeros
   *    (b) flipping 0000 - that is: flipping n-1 trailing zeros where n = size of last 1s block
   *  3. when we arrive at 1111....11110000....0000, then we go to the next set-size, or turn ourselves invalid
   */



  // ========== Subset Iteration ==========
  // ------- Subset Iteration: helpers ---------

  // change bits into the next bitset that contains the same number of ones,
  //    unless that would set a bit outside the range of the bitset, in which case set bits to contain the first |bits|+1 bits,
  //    unless that's over upper_bound, in which case return false
  bool advance(ordered_bitset& bits, const uint32_t upper_bound) {
    DEBUG5(std::cout << "advancing (bitset mode)\n");
    if(!bits.empty()) {
      const size_t trailing_zeros = bits.num_trailing_zeros();
      // NOTE: flip_upwards will also flip (and count!) the 0 to the left to the 1s-block, unless there is none(!)
      const ssize_t ones_block = bits.flip_upwards_until_kth_zero(trailing_zeros, 1);
      if(bits.empty()) {
        // if bits had the format 111...11000...00 before, then it's now empty (after flipping); thus, we're going to the next set-size...
        if(ones_block >= upper_bound) {
          // ...unless we already hit upper_bound, in which case, we're now invalid
          return false;
        } else bits.flip_lowest_k(ones_block + 1);
      } else bits.flip_lowest_k(ones_block - 2);
    } else if(upper_bound > 0) {
      bits.set(0);
    } else return false;
    return true;
  }

  // change bits into the next bitset that contains the same number of ones,
  //    unless that would set a bit outside the range of the bitset, in which case set bits to contain the first |bits|+1 bits,
  //    unless that's over upper_bound, in which case return false
  template<IterableType Container, class Iter>
  bool advance(Container&& c, std::vector<Iter>& bits, const uint32_t upper_bound) {
    DEBUG5(std::cout << "advancing (vector mode)\n");
    if(upper_bound > 0) {
      Iter c_it = std::begin(c);
      if(not bits.empty()) {
        size_t i = 0;
        while(true) {
          if(c_it != std::end(c)) {
            if(i + 1 < bits.size()) {
              // if we're not at the end of bits, then we increase bits[i]
              if(++(bits[i]) == bits[i+1]) {
                // if we're still in the first block, keep going
                bits[i] = c_it; // while we're here, we can reset the first block to the first iters
              } else return true;
            } else if(++(bits[i]) == std::end(c)) {
              // if we're at the end of c, then give bits one more iter, unless upper_bound is reached
              if(bits.size() < upper_bound) {
                bits.emplace_back(c_it);
                return true;
              } else return false;
            } else return true;
            ++c_it; ++i;
          } else return false;
        }
      } else bits.push_back(c_it); // bits is empty
    }
    return false;
  }


  // ------- Subset Iteration: main class ---------
  template<StrictIterableType Container_, bool _partial = false, StrictContainerType OutputContainer_ = std::remove_const_t<Container_>>
    requires (not std::is_const_v<OutputContainer_>)
  struct SubsetIterator:
    public iter_traits_from_reference<OutputContainer_>
  {
    using Traits = iter_traits_from_reference<OutputContainer_>;
    using typename Traits::reference;
    using typename Traits::pointer;

    static constexpr bool partial = _partial;
    
    using OutVal = std::remove_const_t<value_type_of_t<OutputContainer_>>;

    static constexpr bool store_iters = is_any_of<OutVal, iterator_of_t<Container_>, const_iterator_of_t<Container_>>;

    using SubsetState = std::conditional_t<store_iters, std::vector<OutVal>, ordered_bitset>;

    Container_* c = nullptr;
    SubsetState state;
    [[ no_unique_address ]] std::conditional_t<_partial, uint32_t, mstd::monostate> upper_bound;

    SubsetIterator(Container_& _c): c(&_c)
    {
      if constexpr (not store_iters)
        state.set_capacity(_c.size());
    }

    SubsetIterator(Container_& _c, uint32_t low, uint32_t high) requires(partial):
      SubsetIterator(_c) 
    {
      DEBUG4(std::cout << "constructing SubsetIterator for partial subsets of sizes "<<low<<" -- "<<high<<'\n');
      DEBUG6(std::cout << "input container: "<<type_name<Container_>() <<'\n');
      DEBUG6(std::cout << "output container: "<<type_name<OutputContainer_>() <<'\n');
      DEBUG6(std::cout << "SubsetState: "<<type_name<SubsetState>() << " (storing iters: "<<store_iters<<")\n");
      if(low > high) std::swap(low, high);
      if(low <= _c.size()) {
        if(high > _c.size()) high = _c.size();
        upper_bound = high;
        if constexpr (store_iters) {
          auto it = std::begin(_c);
          while(low--) {
            state.emplace_back(it);
            ++it;
          }
        } else state.flip_lowest_k(low);
      } else c = nullptr; // if low > _c.size() then so set falls within the boundaries
    }

    template<class T> requires (partial)
    SubsetIterator(Container_& _c, const linear_interval<T> bounds):
      SubsetIterator(_c, bounds.low(), bounds.high())
    {}
    SubsetIterator(Container_& _c, const uint32_t low) requires (partial):
      SubsetIterator(_c, low, low) 
    {}

    bool is_valid() const { return c != nullptr; }
    size_t current_size() const { return state.size(); }
    
    uint32_t get_upper_bound() const {
      if constexpr (partial)
        return upper_bound;
      else return UINT32_MAX;
    }
    
    void next_state() {
      if(is_valid()) {
        if constexpr (store_iters) {
          if(not advance(*c, state, get_upper_bound()))
            c = nullptr;
        } else {
          if(not advance(state, get_upper_bound()))
            c = nullptr;
        }
      }
    }

    auto deref() const {
      if constexpr (not store_iters) {
        OutputContainer_ out;
        auto it = std::begin(*c);
        size_t last = 0;
        DEBUG4(std::cout << "collecting items of "<<*c<<" with mask "; state.print(std::cout); std::cout << '\n');
        for(auto b_iter = state.begin(); b_iter; ++b_iter){
          const size_t current = *b_iter;
          std::advance(it, current - last);

          assert(it != std::end(*c));
          append(out, *it);
          last = current;
        }
        DEBUG4(std::cout << "collection: "<<out<<'\n');
        return out;
      } else return state;
    }

    //! increment operator
    auto& operator++() { next_state(); return *this; }
    auto operator++(int) { SubsetIterator result = *this; ++(*this); return result; }
    auto& operator--() = delete;
    auto operator--(int) = delete;

    SubsetIterator& operator=(const SubsetIterator&) = default;

    bool operator==(const SubsetIterator& other) const {
      if(is_valid()) {
        if(c == other.c) {
          return state == other.state;
        } else return false;
      } else return not other.is_valid();
    }

    // dereference
    reference operator*() const { return deref(); }
    pointer operator->() const { return operator*(); }
  };

  static_assert(STLLegacyInputIterator<SubsetIterator<std::vector<int>>>);
  static_assert(HasIterTraits<SubsetIterator<std::vector<int>>>);

  // ------- Subset Iteration: factories ---------
  template<StrictIterableType Container_, bool partial = false, StrictContainerType OutputContainer_ = std::remove_const_t<Container_>>
  using SubsetFactory = IterFactory<SubsetIterator<Container_, partial, OutputContainer_>>;

  template<StrictIterableType Container_, StrictContainerType OutputContainer_ = std::remove_const_t<Container_>>
  using BoundedSubsetFactory = IterFactory<SubsetIterator<Container_, true, OutputContainer_>>;

  // ------- Subset Iteration: concepts ---------
  // ------- Subset Iteration: deduction guides ---------
  template<StrictIterableType Container_> SubsetIterator(Container_&&) -> SubsetIterator<Container_>;

  template<class T = void, IterableType Container> requires (std::is_void_v<T> || CompatibleValueTypes<T, std::remove_cvref_t<Container>>)
  auto make_subset_factory(Container&& C, uint32_t lo, uint32_t hi) {
    using OutputContainer = FirstNonVoid<T, std::remove_cvref_t<Container>>;
    return SubsetFactory<std::remove_reference_t<Container>, true, OutputContainer>(std::forward<Container>(C), lo, hi);
  }
  template<class T = void, IterableType Container> requires (std::is_void_v<T> || CompatibleValueTypes<T, std::remove_cvref_t<Container>>)
  auto make_subset_factory(Container&& C, linear_interval<T> li) {
    using OutputContainer = FirstNonVoid<T, std::remove_cvref_t<Container>>;
    return SubsetFactory<std::remove_reference_t<Container>, true, OutputContainer>(std::forward<Container>(C), li);
  }

  // ------- Subset Iteration: defaults ---------

}// namespace
