
#pragma once

#include <array>
#include <ranges>
#include <cmath>

#include "stl_concepts.hpp"
#include "stl_utils.hpp"

namespace mstd {

  // ========== linear_interval ==========
  // cheapo linear interval class - can merge, intersect, add scalars or intervals

  // ------- linear_interval: helpers ---------
  // ------- linear_interval: main class ---------
  
  template<class T = uint32_t>
  struct linear_interval:
    public std::array<T, 2>
  {
    // ------- static stuff --------
    using Parent = std::array<T,2>;

    // ------- members --------
    // ------- construction & desctruction ---------
    //INHERIT_ALL_CONSTRUCTORS(linear_interval, Parent)
    /* let's try and keep this as an aggregate
     *
    constexpr linear_interval(const T& init_lo, const T& init_hi): Parent{init_lo, init_hi} { assert(init_lo <= init_hi); }
    explicit constexpr linear_interval(const T& init): linear_interval(init, init) {}

    template<class Q> requires (not mstd::is_same_v<Q,T>)
    explicit constexpr linear_interval(const linear_interval<Q>& other):
      Parent{static_cast<const T&>(other.low()), static_cast<const T&>(other.high())}
    {}
    */

    // ------- operators --------
    //INHERIT_ASSIGNMENT(linear_interval, Parent)
    
    constexpr bool operator()(const T& val) const { return contains(val); }

    // Note: we won't allow adding two linear intervals since it's ambiguous with 'merge' and 'intersect'
    template<class Q = uint32_t> requires (not mstd::is_derived_from_template_v<Q, mstd::linear_interval> or mstd::is_same_v<Q, T>)
    constexpr void operator+=(const Q& value) {
      high() += value;
      low() += value;
    }
    template<class Q = uint32_t> requires (not mstd::is_derived_from_template_v<Q, mstd::linear_interval> or mstd::is_same_v<Q, T>)
    constexpr void operator-=(const Q& value) {
      high() -= value;
      low() -= value;
    }
    template<class Q = uint32_t> requires (not mstd::is_derived_from_template_v<Q, mstd::linear_interval> or mstd::is_same_v<Q, T>)
    constexpr auto operator+(const Q& value) const {
      const auto hi = high() + value;
      const auto lo = low() + value;
      using Z = std::remove_const_t<decltype(hi)>;
      return linear_interval<Z>{lo, hi};
    }
    template<class Q = uint32_t> requires (not mstd::is_derived_from_template_v<Q, mstd::linear_interval> or mstd::is_same_v<Q, T>)
    constexpr auto operator-(const Q& value) const {
      const auto hi = high() - value;
      const auto lo = low() - value;
      using Z = std::remove_const_t<decltype(hi)>;
      return linear_interval<Z>{lo, hi};
    }


    template<class Q = uint32_t> requires (not mstd::is_derived_from_template_v<Q, mstd::linear_interval> or mstd::is_same_v<Q, T>)
    constexpr auto operator*(const Q& value) const {
      const auto hi = high() * value;
      const auto lo = low() * value;
      using Z = std::remove_const_t<decltype(hi)>;
      linear_interval<Z> result{lo, hi};
      if(value < Q{0}) std::swap(result.low(), result.high());
      assert(result.low() <= result.high());
      return result;
    }
    template<class Q = uint32_t>
    constexpr auto operator*(const linear_interval<Q>& other) const {
      linear_interval<T> result = other * low();
      result.merge(other * high());
      return result;
    }

    template<class Q = uint32_t> requires (not mstd::is_derived_from_template_v<Q, mstd::linear_interval> or mstd::is_same_v<Q, T>)
    constexpr auto operator/(const Q& value) const {
      assert(value != 0);
      const auto hi = high() / value;
      const auto lo = low() / value;
      using Z = std::remove_const_t<decltype(hi)>;
      linear_interval<Z> result{lo, hi};
      if(value < Q{0}) std::swap(result.low(), result.high());
      assert(result.low() <= result.high());
      return result;
    }
    template<class Q = uint32_t>
    constexpr auto operator/(const linear_interval<Q>& other) const {
      linear_interval<T> result(*this / other.low());
      linear_interval<T> tmp(*this / other.high());
      result.merge(std::move(tmp));
      return result;
    }

    template<class Q = uint32_t> requires (not std::is_same_v<Q, T>)
    constexpr auto& operator=(const linear_interval<Q>& other) {
      low() = other.low();
      high() = other.high();
      return *this;
    }
    template<class Q = uint32_t>
    constexpr auto& operator=(const std::pair<Q,Q>& other) {
      low() = other.first;
      high() = other.second;
      return *this;
    }
    constexpr auto& operator=(const std::initializer_list<T>& lst) {
      assert(lst.size() == 2);
      low() = lst.begin()[0];
      high() = lst.begin()[1];
      return *this;
    }
    constexpr auto& operator=(const T& x) {
      low() = high() = x;
      return *this;
    }


    // ------- methods: initialization --------
    // ------- methods: query --------
    using Parent::at;

    constexpr T& low() { return (*this)[0]; }
    constexpr T& high() { return (*this)[1]; }
    constexpr const T& low() const { return (*this)[0]; }
    constexpr const T& high() const { return (*this)[1]; }

    constexpr bool empty() { return low() > high(); }

    constexpr T size() const { return high() - low(); }
    constexpr T length() const { return size(); }
    constexpr T average() const { return (high() + low()) / 2; }
    constexpr bool contains(const linear_interval& other) const { return (low() <= other.low()) and (high() >= other.high()); }
    constexpr bool contains(const T& val) const { return (low() <= val) and (val <= high()); }
    constexpr bool overlaps(const linear_interval& other) const { return (low() >= other.low()) ? (low() <= other.high()) : (high() >= other.low()); }
    constexpr bool contained_in(const linear_interval& other) const { return other.contains(*this); }
    constexpr bool left_of(const T& val) { return high() <= val; }
    constexpr bool strictly_left_of(const T& val) { return high() < val; }
    constexpr bool right_of(const T& val) { return val <= low(); }
    constexpr bool strictly_right_of(const T& val) { return val < low(); }

    // we just use iota_view's begin()/end() since iota_view is a burrowed range, these iterators survive its destruction
    constexpr decltype(auto) begin() const requires std::is_integral_v<T> { return std::ranges::iota_view<T,T>{low(), high() + 1}.begin(); }
    constexpr decltype(auto) end() const requires std::is_integral_v<T> { return std::ranges::iota_view<T,T>{low(), high() + 1}.end(); }


    // ------- methods: modification -------- 
    // remove all values below x from the interval
    constexpr auto& remove_below(const T& x) { low() = std::max(low(), x); return *this; }
    // remove all values above x from the interval
    constexpr auto& remove_above(const T& x) { high() = std::min(high(), x); return *this; }

    constexpr auto& update_lo(const T& lo) { low() = std::min(low(), lo); return *this; }
    constexpr auto& update_hi(const T& hi) { high() = std::max(high(), hi); return *this; }
    constexpr auto& update(const T& x) { update_lo(x); update_hi(x); return *this; }

    // NOTE: merge may not do what you think! If you merge [0,1] and [99,100], you will get [0,100].
    constexpr auto& merge(const linear_interval& other) {
      update_lo(other.low());
      update_hi(other.high());
      return *this;
    }
    constexpr auto& intersect(const linear_interval& other) {
      remove_below(other.low());
      remove_above(other.high());
      return *this;
    }
    




    constexpr auto& ceil() { low() = std::ceil(low()); high() = std::ceil(high()); return *this; }
    constexpr auto& floor() { low() = std::floor(low()); high() = std::floor(high()); return *this; }
    constexpr auto& round() { low() = std::round(low()); high() = std::round(high()); return *this; }
    constexpr auto& shrink_to_int() {
      low() = std::ceil(low());
      high() = std::floor(high());
      if(low() > high()) throw std::logic_error("no int in linear interval ["+std::to_string(low())+","+std::to_string(high())+"]");
      return *this;
    }
    template<class Q> requires std::is_integral_v<Q>
    constexpr auto shrink_to() const {
      const Q _low = std::ceil(low());
      const Q _high = std::floor(high());
      if(_low > _high) throw std::logic_error("no int in linear interval ["+std::to_string(low())+","+std::to_string(high())+"]");
      return linear_interval<Q>{_low, _high};
    }
    constexpr int64_t unique_int() const {
      const auto l = std::ceil(low());
      if(l == std::floor(high())) {
        return l;
      } else throw std::logic_error("linear interval does not contain a unique int");
    }

    friend std::ostream& operator<<(std::ostream& os, const linear_interval& i) { return os << '[' << i.low() << ',' << i.high() << ']'; }
  };
  

  // ------- linear_interval: external operators ---------
  // as a linear_interval is iterable, we'll have to forbid printing it by its "members"
  template<class T> struct blacklist_iterable_printing<linear_interval<T>>: public std:: true_type {};

  // an interval is "bigger than" a value if it lies entirely on the right of that value
  template<class T = uint32_t>
  constexpr bool operator<(const T& value, const linear_interval<T>& interval) { return interval.strictly_right_of(value); }
  template<class T = uint32_t>
  constexpr bool operator>(const T& value, const linear_interval<T>& interval) { return interval.strictly_left_of(value); }
  template<class T = uint32_t>
  constexpr bool operator<=(const T& value, const linear_interval<T>& interval) { return interval.right_of(value); }
  template<class T = uint32_t>
  constexpr bool operator>=(const T& value, const linear_interval<T>& interval) { return interval.left_of(value); }

  template<class T = uint32_t, class Q = uint32_t>
  constexpr auto operator*(const T& value, const linear_interval<Q>& interval) { return interval * value; }

  template<class T = uint32_t, class Q = uint32_t>
  constexpr auto operator/(const Q& value, const linear_interval<T>& interval) {
    linear_interval<Q> result{value / interval.high(), value / interval.low()};
    if(value < static_cast<Q>(0)) std::swap(result.low(), result.high());
    assert(result.low() <= result.high());
    return result;
  }

  // ------- linear_interval: factories ---------
  
  // ------- linear_interval: concepts ---------
  
  // ------- linear_interval: deduction guides ---------
  
  // ------- linear_interval: defaults ---------

}
