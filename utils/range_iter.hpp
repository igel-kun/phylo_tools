
#pragma once

// this is a range class, allowing iteration over ranges of things

namespace PT{

  template<class T = uint32_t>
  class RangeIter
  {
  private:
    T x;
  public:
    RangeIterator(T _x):
      x(_x)
    {
      static_assert(std::is_integral<T>::value, "can only range over integral types");
    }

    bool operator==(const RangeIter& it) const { return x == it.x; }
    bool operator!=(const RangeIter& it) const { return x != it.x; }
    T operator*() const { return x; }
    RangeIter& operator++() { ++x; return *this; }
  };

  template<class T>
  class Range
  {
  private:
    const T from;
    const T to;

  public:
    Range(T _from, T _to):
      from(_from), to(_to)
    { static_assert(std::is_integral<T>::value, "can only range over integral types"); }

    Range(T _to): Range(0, _to) {}

    RangeIter begin() const { return {from}; }
    RangeIter end() const { return {to}; }
  };

  template<typename T>
  Range<T> range(T from, T to){
    static_assert(std::is_integral<T>::value, "can only range over integral types");
    return { from, to };
  }
}// namespace
