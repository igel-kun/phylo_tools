
#pragma once

#include <array>

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
      update_lo(other.at(0));
      update_hi(other.at(1));
    }
    void intersect(const linear_interval& other) {
      at(0) = std::max(at(0), other.at(0));
      at(1) = std::min(at(1), other.at(1));
    }
    void update_lo(const T& lo) { at(0) = std::min(at(0), lo); }
    void update_hi(const T& hi) { at(1) = std::max(at(1), hi); }
    void update(const T& x) { update_lo(x); update_hi(x); }

    bool contains(const linear_interval& other) const { return (at(0) <= other.at(0)) && (at(1) >= other.at(1)); }
    bool contains(const T& val) const { return (at(0) <= val) && (at(1) >= val); }
    bool overlaps(const linear_interval& other) const { return (at(0) >= other.at(0)) ? (at(0) <= other.at(1)) : (at(1) >= other.at(0)); }
    bool contained_in(const linear_interval& other) const { return other.contains(*this); }
    bool left_of(const T& val) { return at(1) <= val; }
    bool strictly_left_of(const T& val) { return at(1) < val; }
    bool right_of(const T& val) { return val <= at(0); }
    bool strictly_right_of(const T& val) { return val < at(0); }

    friend std::ostream& operator<<(std::ostream& os, const linear_interval& i) { return os << '[' << i.at(0) << ',' << i.at(1) << ']'; }
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
