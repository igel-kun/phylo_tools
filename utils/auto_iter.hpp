
#pragma once

#include "stl_utils.hpp"

namespace std{

  // NOTE: 
  // we do not need to store the end-iterator if the iterator type has "bool is_valid() const"
  // (for example, the _auto_iter itself -- imagine an _auto_iter of _auto_iters)
  template<class Iter>
  concept iter_verifyable = requires(const Iter i) {
    { i.is_valid() } -> convertible_to<bool>;
  };

  // a forward iterator that knows the end of the container & converts to false if it's at the end
  //NOTE: this also supports that the end iterator has a different type than the iterator, as long as they can be compared with "!="
  template<class Iterator, class EndIterator = conditional_t<iter_verifyable<Iterator>, void, Iterator>>
  class _auto_iter: public Iterator
  {
    static_assert(!iter_verifyable<Iterator>);
    using Parent = Iterator;
    EndIterator end_it;
   public:
    using Traits = my_iterator_traits<Iterator>;
    using Parent::Parent;

    // when default-constructed, end_it == *this, so is_valid() will be false
    _auto_iter(): Parent(), end_it(*this) {}
    // copy and move-construction are default
    _auto_iter(const _auto_iter&) = default;
    _auto_iter(_auto_iter&&) = default;

    // construct from two iterators (begin and end)
    template<class _Iterator, class _EndIterator>
      requires (is_convertible_v<remove_cvref_t<_Iterator>,Iterator> && is_convertible_v<remove_cvref_t<_EndIterator>, EndIterator>)
    constexpr _auto_iter(_Iterator&& it, _EndIterator&& _end): Parent(forward<_Iterator>(it)), end_it(forward<_EndIterator>(_end)) {}
    // construct from a container
    template<IterableType Container> requires (is_convertible_v<iterator_of_t<Container>, Iterator>)
    _auto_iter(Container&& c): _auto_iter(begin(c), end(c)) {}

    _auto_iter& operator=(const _auto_iter&) = default;
    _auto_iter& operator=(_auto_iter&&) = default;

    bool operator==(const _auto_iter& other) const {
      return is_valid() ? Parent::operator==(static_cast<const Iterator>(other)) : !(other.is_valid());
    }
    template<class T>
    bool operator==(T&& other) const {
      return is_valid() ? Parent::operator==(forward<T>(other)) : (other != end_it);
    }
    template<class T>
    bool operator!=(T&& other) const { return !operator==(forward<T>(other)); }

    constexpr bool is_valid() const { return static_cast<const Parent>(*this) != end_it; }
    constexpr bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }
    EndIterator get_end() const { return end_it; }
  };

  template<class Iterator>
  struct _auto_iter<Iterator, void>: public Iterator
  {
    using Parent = Iterator;

    MAKE_CONSTRUCTIBLE_FROM_TUPLE(_auto_iter)

    template<class... Args>
    _auto_iter(Args&&... args): Parent(forward<Args>(args)...) {}

    // copy and move-construction are default
    _auto_iter(const _auto_iter&) = default;
    _auto_iter(_auto_iter&&) = default;

    template<IterableType Container> requires (is_convertible_v<iterator_of_t<Container>, Iterator>)
    _auto_iter(Container&& c): Parent(begin(c)) {}

    template<class _Iterator, class T> requires is_convertible_v<remove_cvref_t<_Iterator>,Iterator>
    constexpr _auto_iter(_Iterator&& it, T&&): Parent(forward<_Iterator>(it)) {}

    bool operator==(const _auto_iter& other) const {
      return is_valid() ? Parent::operator==(static_cast<const Iterator>(other)) : !(other.is_valid());
    }
    // any non-auto_iter is just given to the Parent for comparison
    template<class T>
    bool operator==(T&& other) const {
      return is_valid() ? Parent::operator==(forward<T>(other)) : !(other.is_valid());
    }
    template<class T>
    bool operator!=(T&& other) const { return !operator==(forward<T>(other)); }

    constexpr bool is_valid() const { return Parent::is_valid(); }
    constexpr bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }
    // get the end-iterator
    // NOTE: default-constructed Iterator should be equivalent to the end-iterator (compare == with the end-iterator), this is the user's responsability!
    constexpr auto get_end() const { return Iterator::get_end(); }
  };


  template<class T, class EndIterator = conditional_t<iter_verifyable<iterator_of_t<T>>, void, iterator_of_t<T>>>
  using auto_iter = _auto_iter<iterator_of_t<T>, EndIterator>;


  template<IterableType Container,
           class Iterator = iterator_of_t<Container>,
           class EndIterator = Iterator>
  auto_iter<Iterator, EndIterator> get_auto_iter(Container&& c) { return c; }

  template<IterableType Container,
           class KeyType,
           class Iterator = iterator_of_t<Container>,
           class EndIterator = Iterator>
  auto_iter<Iterator, EndIterator> auto_find(Container&& c, const KeyType& key) { return {c.find(key), end(c)}; }
 
} //namespace
