
#pragma once

#include <array>
#include <ranges>

namespace mstd {

  // cheapo linear interval class - can merge and intersect
  template<class T = uint32_t>
  struct linear_interval: public std::array<T, 2> {
    using Parent = std::array<T,2>;
    using Parent::at;

    T& low() { return (*this)[0]; }
    T& high() { return (*this)[1]; }
    const T& low() const { return (*this)[0]; }
    const T& high() const { return (*this)[1]; }

    linear_interval(const T& init_lo, const T& init_hi): Parent{init_lo, init_hi} {}
    linear_interval(const T& init): linear_interval(init, init) {}
    
    void merge(const linear_interval& other) {
      update_lo(other.low());
      update_hi(other.high());
    }
    void intersect(const linear_interval& other) {
      low() = std::max(low(), other.low());
      high() = std::min(high(), other.high());
    }
    void update_lo(const T& lo) { low() = std::min(low(), lo); }
    void update_hi(const T& hi) { high() = std::max(high(), hi); }
    void update(const T& x) { update_lo(x); update_hi(x); }

    bool contains(const linear_interval& other) const { return (low() <= other.low()) && (high() >= other.high()); }
    bool contains(const T& val) const { return (low() <= val) && (val <= high()); }
    bool overlaps(const linear_interval& other) const { return (low() >= other.low()) ? (low() <= other.high()) : (high() >= other.low()); }
    bool contained_in(const linear_interval& other) const { return other.contains(*this); }
    bool left_of(const T& val) { return high() <= val; }
    bool strictly_left_of(const T& val) { return high() < val; }
    bool right_of(const T& val) { return val <= low(); }
    bool strictly_right_of(const T& val) { return val < low(); }
    bool operator()(const T& val) const { return contains(val); }

    // we just use iota_view's begin()/end() since iota_view is a burrowed range, these iterators survive its destruction
    decltype(auto) begin() const { return std::ranges::iota_view{low(), high()}.begin(); }
    decltype(auto) end() const { return std::ranges::iota_view{low(), high()}.end(); }

    friend std::ostream& operator<<(std::ostream& os, const linear_interval& i) { return os << '[' << i.low() << ',' << i.high() << ']'; }
  };

  // an interval is "bigger than" a value if it lies entirely on the right of that value
  template<class T = uint32_t>
  bool operator<(const T& value, const linear_interval<T>& interval) { return interval.strictly_right_of(value); }
  template<class T = uint32_t>
  bool operator>(const T& value, const linear_interval<T>& interval) { return interval.strictly_left_of(value); }
  template<class T = uint32_t>
  bool operator<=(const T& value, const linear_interval<T>& interval) { return interval.right_of(value); }
  template<class T = uint32_t>
  bool operator>=(const T& value, const linear_interval<T>& interval) { return interval.left_of(value); }

}
