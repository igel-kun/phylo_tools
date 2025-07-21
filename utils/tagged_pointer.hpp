
#pragma once

#include "utils.hpp"

namespace mstd {

  // ========== TaggedPoiner ==========
  // a tagged pointer is a pointer whose lowest bits are repurposed to fit some bits of a uint8_t

  // ------- TaggedPoiner: helpers ---------
  
  // if Ptr is an incomplete type, because we're trying to declare a pointer to a class inside that class,
  // then we may pass 'alignment' manually since the compiler cannot figure out the alignment of incomplete classes
  template<class Ptr, uint8_t tag_bits, size_t alignment>
  struct _well_aligned { static constexpr bool value = (alignment >= (1ul << tag_bits)); };
  template<class Ptr, uint8_t tag_bits>
  struct _well_aligned<Ptr, tag_bits, 0> { static constexpr bool value = (NUM_TRAILING_ZEROSL(alignof(std::remove_pointer_t<Ptr>)) >= tag_bits); };
  template<class Ptr, uint8_t tag_bits, size_t alignment>
  constexpr bool is_well_aligned = _well_aligned<Ptr, tag_bits, alignment>::value;

  // ------- TaggedPoiner: main class ---------
  template<class Ptr, uint8_t tag_bits, size_t alignment = 0>
    requires (std::is_pointer_v<Ptr> and
        (tag_bits < sizeof(uintptr_t) * 8) and
        is_well_aligned<Ptr, tag_bits, alignment>)
  struct TaggedPtr {
    // ------- static stuff --------
    using Pointee = std::remove_pointer_t<Ptr>;

    static constexpr size_t pointer_bits = sizeof(uintptr_t) * 8;
    //static constexpr uintptr_t mask = (uintptr_t{1} << tag_bits) - 1;
    static constexpr uintptr_t mask = (~uintptr_t{0}) >> (pointer_bits - tag_bits);
    static_assert(NUM_ONES_IN(mask) == tag_bits);

    // ------- members --------
  protected:
    uintptr_t value = 0;

    // ------- construction & desctruction ---------
  public:
    TaggedPtr() = default;

    TaggedPtr(Ptr ptr, const uint8_t tag = 0):
      value{0}
    {
      set(ptr, tag);
    }

    // ------- operators --------
    bool operator==(const nullptr_t other) const { return get_pointer() == other; }
    bool operator==(const Ptr other) const { return get_pointer() == other; }
    //operator==(const TaggedPtr other) const = default;
  
    // allow handling just like Ptr
    Pointee& operator*() const { return *(get_pointer()); }
    Pointee* operator->() const { return get_pointer(); }
    TaggedPtr& operator+=(const int64_t i) { value += alignof(Pointee) * i; return *this; }
    TaggedPtr& operator-=(const int64_t i) { value -= alignof(Pointee) * i; return *this; }
    TaggedPtr operator+(const int64_t i) const { TaggedPtr result(*this); result += i; return result; }
    TaggedPtr operator-(const int64_t i) const { TaggedPtr result(*this); result -= i; return result; }
    TaggedPtr& operator=(const Ptr x) { set_pointer(x); return *this; }
    TaggedPtr& operator=(const nullptr_t x) { set_pointer(x); return *this; }

    // allow casting seemlessly to pointer
    operator Ptr() const { return get_pointer(); }

    // ------- methods: initialization --------
    // ------- methods: modification --------
    void clear_tag() { value &= ~mask; }
    
    void set_tag(const uint8_t tag) {
      assert(tag <= mask && "Tag exceeds range");
      value = (value & ~mask) | (tag & mask);
    }
    
    void set_pointer(const Ptr ptr) {
      const uintptr_t raw_ptr = std::bit_cast<uintptr_t>(ptr);
      assert((raw_ptr & mask) == 0 && "Pointer is not sufficiently aligned at runtime");
      value = (value & mask) | raw_ptr;
    }
    void set_pointer(const nullptr_t ptr) { set_pointer(std::bit_cast<Ptr>(ptr)); }
    
    void set(const Ptr ptr, const uint8_t tag) {
      const uintptr_t raw_ptr = std::bit_cast<uintptr_t>(ptr);
      assert(tag <= mask && "Tag exceeds range");
      assert((raw_ptr & mask) == 0 && "Pointer is not sufficiently aligned at runtime");
      value = raw_ptr | (tag & mask);
    }

    // ------- methods: query --------
    Ptr get_pointer() const { return std::bit_cast<Ptr>(value & ~mask); }
    uint8_t get_tag() const { return static_cast<uint8_t>(value & mask); }
  };
  // ------- TaggedPoiner: factories ---------
  // ------- TaggedPoiner: concepts ---------
  // ------- TaggedPoiner: deduction guides ---------
  // ------- TaggedPoiner: defaults ---------

}
