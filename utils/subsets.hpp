
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

  template<IterableType<TR_PtrOK> Container_, bool store_iters_ = false>
  struct SetWithSubset {
    using Container = std::remove_pointer_t<Container_>;

    static constexpr bool ground_set_indexed = (VectorType<Container> or SetType<Container>);
    static constexpr bool is_indirect = std::is_pointer_v<Container_>;
    static constexpr bool store_iters = store_iters_ or not ground_set_indexed;
    
    using ContainerIter = std::conditional_t<std::is_const_v<Container>, const_iterator_of_t<Container>, iterator_of_t<Container>>;
    using SubsetMask = std::conditional_t<store_iters, std::vector<ContainerIter>, ordered_bitset>;

    // change bits into the next bitset that contains the same number of ones,
    //    unless that would set a bit outside the range of the bitset, in which case set bits to contain the first |bits|+1 bits,
    //    unless that's over upper_bound, in which case return false
    static bool advance(ordered_bitset& bits, const uint32_t upper_bound) {
      DEBUG5(std::cout << "advancing (bitset mode) "; bits.print(std::cout); std::cout << "\n");
      if(not bits.empty()) {
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
    static bool advance(Container& c, std::vector<ContainerIter>& bits, const uint32_t upper_bound) {
      DEBUG5(std::cout << "advancing (vector mode)\n");
      if(upper_bound > 0) {
        auto c_it = std::begin(c);
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

    Container_ ground_set;
    SubsetMask subset;

    SetWithSubset() = default;
    SetWithSubset(Container_&& gs): ground_set{std::move(gs)} {}
    SetWithSubset(const Container_& gs): ground_set{gs} {}

    decltype(auto) get_item(const auto& i) {
      if constexpr (store_iters) {
        // if the ground set cannot be indexed, then the subset is a vector of iterators into the ground set
        return *i;
      } else return access(ground_set)[i]; // if the ground set is a vector/set then the subset is a bitset of indices into the ground set
    }
    decltype(auto) get_item(const auto& i) const {
      if constexpr (store_iters) {
        return *i;
      } else return access(ground_set)[i];
    }

    template<StrictContainerType T>
    decltype(auto) get_subset() const {
      if constexpr (not std::is_same_v<T, SubsetMask>) {
        static_assert(ConvertibleValueTypes<T, Container>);
        T result;
        if constexpr (HasReserve<T>)
          result.reserve(subset.size());
        for(const auto i: subset)
          append(result, get_item(i));
        return result;
      } else return subset;
    }

    const Container& get_ground_set() const { return access(ground_set); }

    bool advance(const uint32_t upper_bound = std::numeric_limits<uint32_t>::max()) {
      if constexpr (store_iters) {
        return advance(access(ground_set), subset, upper_bound);
      } else return advance(subset, upper_bound);
    }

    friend std::ostream& operator<<(std::ostream& os, const SetWithSubset& s) {
      os << '(';
      for(const auto i: s.subset)
        os << s.get_item(i) << ',';
      return os << ')';
    }
  };


  // ------- Subset Iteration: main class ---------
  template<IterableType<TR_ConstOK> Container_,
           bool partial_ = false,
           StrictContainerType OutputContainer_ = std::remove_const_t<Container_>>
  struct SubsetIterator:
    public SetWithSubset<Container_*, is_any_of<value_type_of_t<OutputContainer_>, iterator_of_t<Container_>, const_iterator_of_t<Container_>>>,
    public iter_traits_from_reference<OutputContainer_>
  {
    static_assert(not std::is_const_v<OutputContainer_>); // OutputContainer_ is a STRICT ContainerType, so no const should be allowed
    using Traits = iter_traits_from_reference<OutputContainer_>;
    using typename Traits::reference;
    using typename Traits::pointer;
    using OutputContainer = OutputContainer_;
    using OutVal = value_type_of_t<OutputContainer>;
    using Container = Container_;

    static constexpr bool user_wants_iters = is_any_of<OutVal, iterator_of_t<Container>, const_iterator_of_t<Container>>;
    using Parent = SetWithSubset<Container*, user_wants_iters>;
    using Parent::advance;
    
    using Parent::ground_set;
    using Parent::subset;

    static constexpr bool partial = partial_;
    static constexpr bool store_iters = Parent::store_iters;

    [[ no_unique_address ]] std::conditional_t<partial, uint32_t, mstd::monostate> upper_bound;

    SubsetIterator(Container& c) requires (not partial):
      Parent(&c)
    {
      subset.reserve(access(ground_set).size());
    }

    SubsetIterator(Container& c, uint32_t low, uint32_t high) requires(partial):
      Parent(&c)
    {
      DEBUG4(std::cout << "constructing SubsetIterator for partial subsets of sizes "<<low<<" -- "<<high<<'\n');
      DEBUG6(std::cout << "input container: "<<type_name<Container>() <<'\n');
      DEBUG6(std::cout << "output container: "<<type_name<OutputContainer>() <<'\n');
      DEBUG6(std::cout << "SubsetState: "<<type_name<Parent>() << " (storing iters: "<<store_iters<<")\n");
      const auto& gs = access(ground_set);
      const uint32_t ground_size = gs.size();

      if(low > high) std::swap(low, high);
      if(low <= ground_size) {
        if(high > ground_size) high = ground_size;
        upper_bound = high;
        
        // NOTE: if the subset is a vector of iters, we need upper_bound many entries
        //       if the subset is an ordered_bitset, then we need groundset.size() many entries
        if constexpr (store_iters) {
          subset.reserve(upper_bound);
        } else subset.reserve(gs.size());

        assert(low <= gs.size());
        if constexpr (store_iters) {
          for(auto it = std::begin(gs); low-- != 0; ++it)
            append(subset, it);
        } else subset.flip_lowest_k(low);
      } else ground_set = nullptr; // if low > c.size() then no subset falls within the boundaries
    }

    template<class T> requires (partial)
    SubsetIterator(Container& c, const linear_interval<T> bounds):
      SubsetIterator(c, bounds.low(), bounds.high())
    {}
    template<class InContainer, class T> requires (partial)
    SubsetIterator(Container& c, const uint32_t bnd):
      SubsetIterator(c, bnd, bnd)
    {}

    template<class... Args>
    SubsetIterator(Container* c, Args&&... args):
      SubsetIterator(*c, std::forward<Args>(args)...)
    {}

    bool is_valid() const { return ground_set != nullptr; }
    size_t current_size() const { return subset.size(); }
    
    uint32_t get_upper_bound() const {
      if constexpr (partial)
        return upper_bound;
      else return std::numeric_limits<uint32_t>::max();
    }
    
    void next_state() {
      if(is_valid()) {
        if(not advance(get_upper_bound()))
          ground_set = nullptr;
      }
    }

    //! increment operator
    auto& operator++() { next_state(); return *this; }
    auto operator++(int) { SubsetIterator result = *this; ++(*this); return result; }
    auto& operator--() = delete;
    auto operator--(int) = delete;

    SubsetIterator& operator=(const SubsetIterator&) = default;

    bool operator==(const SubsetIterator& other) const {
      if(is_valid()) {
        if(ground_set == other.ground_set) {
          return subset == other.subset;
        } else return false;
      } else return not other.is_valid();
    }

    // dereference
    reference deref() const { return Parent::template get_subset<OutputContainer>(); }
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
  template<IterableType<TR_ConstOK> Container_> SubsetIterator(Container_&) -> SubsetIterator<Container_, false>;
  template<IterableType<TR_ConstOK> Container_> SubsetIterator(Container_&, uint32_t) -> SubsetIterator<Container_, true>;
  template<IterableType<TR_ConstOK> Container_> SubsetIterator(Container_&, uint32_t, uint32_t) -> SubsetIterator<Container_, true>;

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
