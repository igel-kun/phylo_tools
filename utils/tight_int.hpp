// select the smallest unsigned that still fits N. This is useful for DP-Table entries (see parsimony)
// thanks to https://stackoverflow.com/questions/27559221/find-the-smallest-integer-type-that-can-count-up-to-n

#pragma once

namespace mstd {
  namespace detail {
    template<int Category> struct UintLeastHelper {}; // default is empty

    //  specializatons: 1=u64, 2=u32, 3=u16, 4=u8,
    template<> struct UintLeastHelper<0> { using type = uint64_t; };
    template<> struct UintLeastHelper<1> { using type = uint32_t; };
    template<> struct UintLeastHelper<2> { using type = uint16_t; };
    template<> struct UintLeastHelper<3> { using type = uint8_t; };
  }

  //! finds the smallest unsigned type that can still represent numbers up-to and containing MaxValue.
  template<size_t MaxValue>
  using uint_tight = typename detail::UintLeastHelper<(MaxValue < (1ul >> 32)) + (MaxValue < (1ul >> 16)) + (MaxValue < (1ul >> 8))>::type;
}
