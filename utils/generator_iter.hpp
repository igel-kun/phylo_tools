

#pragma once

namespace mstd {

  // NOTE: this iterator takes an item x and a number k and returns k instances of x
  // NOTE: usually, the class 'Item' will be a reference
  template<class Item, ArithmeticType IndexType = uint32_t>
  class generator_iter {
    using ItemPtr = pointer_from_reference<Item>;
    using ItemRef = reference_of_t<Item>;
    ItemPtr p;
    linear_interval<IndexType> index;
  public:
    using difference_type = ptrdiff_t;
    using reference       = ItemRef;
    using const_reference = ConstItemRef;
    using value_type      = std::remove_reference_t<reference>;
    using pointer         = ItemPtr;
    using iterator_category = std::random_access_iterator_tag;

    generator_iter(std::nullptr_t p = nullptr):x(0), index(0, 0) {}

    template<class T> requires (!std::is_pointer_v<T>)
    generator_iter(T&& _x, const IndexType _max = 1u):
      x(&_x), index(0, _max) {}

    template<class P> requires (std::is_pointer_v<P>)
    generator_iter(P _x, const IndexType _max = 1u):
      x(_x), index(0, _max) {}

    template<class P> requires (is_pair<P> && ArithmeticType<typename P::second_type>)
    generator_iter(P&& p):
      x(&(p.first())), index(0, p.second()) {}

    // take inverted pairs only if the second type is not arithmetic!
    template<class P> requires (is_pair<P> && !ArithmeticType<typename P::second_type> && ArithmeticType<typename P::first_type>)
    generator_iter(P&& p):
      x(&(p.second())), index(0, p.first()) {}


    bool is_valid() const { return index.lo() < index.hi(); }
    bool is_invalid() const { return !is_valid(); }

    ItemRef operator*() { return *x; }
    ConstItemRef operator*() const { return *x; }
    auto operator->() { return x; }
    auto operator->() const { return x; }

    generator_iter& operator++() { index.lo()++; return *this; }
    generator_iter& operator--() { index.lo()--; return *this; }
    generator_iter operator++(int) { generator_iter result(*this); ++(*this); return result; }
    generator_iter operator--(int) { generator_iter result(*this); --(*this); return result; }

    generator_iter& operator+=(const IndexType i) { index.lo() += i; return *this; }
    generator_iter& operator-=(const IndexType i) { index.lo() -= i; return *this; }
    generator_iter operator+(const IndexType i) { generator_iter result(*this); result += i; return result; }
    generator_iter operator-(const IndexType i) { generator_iter result(*this); result -= i; return result; }

    bool operator==(const generator_iter& other) const { return (x == other.x) && (index == other.index); }
    bool operator!=(const generator_iter& other) const { return !operator==(other); }
    bool operator==(const GenericEndIterator) const { return is_invalid(); }
    bool operator!=(const GenericEndIterator) const { return is_valid(); }

    generator_iter& operator=(const generator_iter& other) = default;
    generator_iter& operator=(generator_iter&& other) = default;
  };

}

