
// infrastructure for enumerating subsets of a set X such that all subsets of an enumerated set have already been enumerated before

#include "stl_utils.hpp"
#include "set_interface.hpp"
#include "iter_bitset.hpp"
#include "linear_interval.hpp"

#pragma once

namespace mstd {

  //! note: if you want to modify the original container, set the _OutputContainer to contain std::reference_wrapper<_Container::value_type> 
  //NOTE: this assumes that the underlying container's order does not change!
  template<class _Container, class _OutputContainer = _Container>
  class SubsetIterator {
  protected:
    const _Container* c;
    ordered_bitset bits;

  public:
    using difference_type = ptrdiff_t;
    using value_type = _OutputContainer;
    using reference  = value_type;
    using const_reference = const value_type;
    using pointer    = self_deref<_OutputContainer>;

    SubsetIterator(const _Container& _c):
      c(&_c), bits(_c.size())
    {}
    // construction with an initialization set

/*    template<class _InitSet>
    SubsetIterator(_Container& _c, const _InitSet& init_set):
      c(_c), bits(init_set.begin(), init_set.end(), _c.size())
    {}
    SubsetIterator(_Container& _c, const std::ordered_bitset& init_set):
      c(_c), bits(init_set)
    {}
    SubsetIterator(_Container& _c, const std::unordered_bitset& init_set):
      c(_c), bits(init_set)
    {}
*/
    SubsetIterator(const _Container& _c, const uint64_t item):
      c(&_c), bits(_c.size())
    { bits.set(item); }

    //! increment operator
    SubsetIterator& operator++() { ++bits; return *this; }
    SubsetIterator operator++(int) { SubsetIterator result = *this; ++bits; return result; }
    SubsetIterator& operator--() { --bits; return *this; }
    SubsetIterator operator--(int) { SubsetIterator result = *this; --bits; return result; }

    SubsetIterator& operator=(const SubsetIterator&) = default;

    bool operator==(const SubsetIterator& it) const { return bits == it.bits; }

    // dereference
    value_type operator*() {
      value_type out;
      auto container_iter = std::begin(*c);
      uint64_t last = 0;
      // emplace the items of 
      DEBUG4(std::cout << "collecting items of "<<*c<<" with mask "; bits.print(std::cout); std::cout << "\n");
      for(auto b_iter = bits.begin(); b_iter; ++b_iter){
        const uint64_t current = *b_iter;
        std::advance(container_iter, current - last);

        assert(container_iter != std::end(*c));
        append(out, *container_iter);
        last = current;
      }
      DEBUG4(std::cout << "collection: "<<out<<'\n');
      return out;
    }
    pointer operator->() { return operator*(); }
  };

  static_assert(__LegacyInputIterator<SubsetIterator<int*>>);
  static_assert(HasIterTraits<SubsetIterator<int*>>);

  template<class Container, class _OutputContainer = Container>
  struct SubsetBeginEndIters {
    using iterator = SubsetIterator<Container, _OutputContainer>;
    using const_iterator = SubsetIterator<const Container, _OutputContainer>;
    static iterator begin(std::remove_cv_t<Container>& c) { return c; }
    static iterator end(std::remove_cv_t<Container>& c) { return {c, c.size() }; }
    static const_iterator begin(const Container& c) { return c; }
    static const_iterator end(const Container& c) { return {c, c.size() }; }
  };



  /* iterating over all subsets S of X with |S| in the interval [a,b]
   * theory:
   *  1. we iterate from smaller sets to larger sets, starting with all subsets of size a
   *  2. given a set 000110101101011111000, the next set is obtained by
   *    (a) adding 1000 - that is: adding 1 << x where x = #trailing zeros
   *    (b) flipping 0000 - that is: flipping n-1 trailing zeros where n = size of last 1s block
   *  3. when we arrive at 1111....11110000....0000, then we go to the next set-size, or turn ourselves invalid
   */

  // iterate over all subsets with size in the given interval [lower,upper]
  // NOTE: if lower > container size, we'll produce the end-iterator
  template<class _Container, class _OutputContainer = _Container>
  class BoundedSubsetIterator: public SubsetIterator<_Container, _OutputContainer> {
    using Parent = SubsetIterator<_Container, _OutputContainer>;
    using Parent::c;
    using Parent::bits;

    ssize_t upper_bound;

    void next_set() {
      if(!bits.empty()) {
        const size_t trailing_zeros = bits.num_trailing_zeros();
        // NOTE: flip_upwards will also flip (and count!) the 0 to the left to the 1s-block, unless there is none(!)
        const ssize_t ones_block = bits.flip_upwards_until_kth_zero(trailing_zeros, 1);
        if(bits.empty()) {
          // if bits had the format 111...11000...00 before, then it's now empty (after flipping); thus, we're going to the next set-size...
          if(ones_block == upper_bound) {
            // ...unless we already hit upper_bound, in which case, we're now invalid
            upper_bound = -1;
          } else bits.flip_lowest_k(ones_block + 1);
        } else bits.flip_lowest_k(ones_block - 2);
      } else if(upper_bound > 0) {
        bits.set(0);
      } else upper_bound = -1;
    }
  public:
    void set_invalid() { upper_bound = -1; }
    size_t current_size() const { return bits.size(); }
    bool is_valid() const { return upper_bound != -1; }
    explicit operator bool() const { return is_valid(); }

    template<class T>
    BoundedSubsetIterator(const _Container& _c, const mstd::linear_interval<T> bounds):
      BoundedSubsetIterator(_c, bounds.low(), bounds.high())
    {}
    BoundedSubsetIterator(const _Container& _c, const ssize_t low = -1):
      BoundedSubsetIterator(_c, low, low)
    {}

    BoundedSubsetIterator(const _Container& _c, const ssize_t low, const ssize_t high):
      Parent(_c), upper_bound{std::min(high, static_cast<ssize_t>(c->size()))}
    {
      DEBUG5(std::cout << "iterating size-["<<low<<", "<<high<<"] subsets of "<<_c<<"\n");
      assert(low <= high);
      if((low >= 0) && (low <= upper_bound)) {
        // for initialization, set the first 'low' bits
        bits.flip_lowest_k(low);
      } else set_invalid(); // if low is out of bounds, mark the iterator invalid
    }

    BoundedSubsetIterator() = delete;

    auto& operator++() { next_set(); return *this; }
    auto operator++(int) { BoundedSubsetIterator result = *this; next_set(); return result; }
    
    auto& operator--() = delete;
    auto operator--(int) = delete;
  };

  static_assert(HasIterTraits<SubsetIterator<int*>>);


  template<class _Container, class _OutputContainer = _Container>
  using SubsetFactory = IterFactory<_Container, SubsetBeginEndIters<_Container, _OutputContainer>>;

  template<class _Container, class _OutputContainer = _Container>
  using BoundedSubsetFactory = IterFactory<BoundedSubsetIterator<_Container, _OutputContainer>>;


}// namespace
