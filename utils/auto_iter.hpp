
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
  class _auto_iter: public my_iterator_traits<Iterator>
  {
    static_assert(!iter_verifyable<Iterator>);
    using Traits = my_iterator_traits<Iterator>;
    Iterator it;
    EndIterator end_it;
   public:
    using typename Traits::reference;
    using typename Traits::pointer;

    // when default-constructed, end_it == *this, so is_valid() will be false
    _auto_iter(): it(), end_it(it) {}

    // construct from two iterators (begin and end)
    template<class _Iterator, class _EndIterator>
      requires (is_convertible_v<remove_cvref_t<_Iterator>,Iterator> && is_convertible_v<remove_cvref_t<_EndIterator>, EndIterator>)
    constexpr _auto_iter(_Iterator&& _it, _EndIterator&& _end): it(forward<_Iterator>(_it)), end_it(forward<_EndIterator>(_end)) {}
    // construct from a container
    template<IterableType Container> requires (is_convertible_v<iterator_of_t<Container>, Iterator>)
    _auto_iter(Container&& c): _auto_iter(begin(c), end(c)) {}
    // piecewise construct
    template<class IterTuple, class EndTuple>
    _auto_iter(const piecewise_construct_t, IterTuple&& iter_init, EndTuple&& end_init):
      it(make_from_tuple<Iterator>(forward<IterTuple>(iter_init))),
      end_it(make_from_tuple<EndIterator>(forward<EndTuple>(end_init)))
    {}

    // copy and move-construction & assignment are default
    _auto_iter(const _auto_iter&) = default;
    _auto_iter(_auto_iter&&) = default;
    _auto_iter& operator=(const _auto_iter&) = default;
    _auto_iter& operator=(_auto_iter&&) = default;

    bool operator==(const _auto_iter& other) const { return is_valid() ? (other.it == it) : !(other.is_valid()); }
    template<class T>
    bool operator==(const T& other) const { return is_valid() ? (other == it) : (other != end_it); }
    template<class T>
    bool operator!=(const T& other) const { return !(*this == other); }

    _auto_iter& operator++() { ++it; return *this; }
    _auto_iter  operator++(int) { _auto_iter result(*this); ++it; return result; }
    _auto_iter& operator--() { ++it; return *this; }
    _auto_iter  operator--(int) { _auto_iter result(*this); --it; return result; }

    reference operator*() const { return *it; }
    pointer operator->() const { return &(*it); }

    constexpr bool is_valid() const { return it != end_it; }
    constexpr bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }
    Iterator get_iter() const { return it; }
    EndIterator get_end() const { return end_it; }
  };



  template<class Iterator>
  class _auto_iter<Iterator, void>: public my_iterator_traits<Iterator>
  {
    using Traits = my_iterator_traits<Iterator>;
    Iterator it;
  public:
    using typename Traits::reference;
    using typename Traits::pointer;

    MAKE_CONSTRUCTIBLE_FROM_TUPLE(_auto_iter)

    template<class... Args>
    _auto_iter(Args&&... args): it(forward<Args>(args)...) {}

    // copy and move-construction are default
    _auto_iter(const _auto_iter&) = default;
    _auto_iter(_auto_iter&&) = default;

    template<IterableType Container> requires (is_convertible_v<Container, Iterator>)
    _auto_iter(Container&& c): it(forward<Container>(c)) {}

    bool operator==(const _auto_iter& other) const { return is_valid() ? (other.it == it) : !(other.is_valid()); }
    template<class T>
    bool operator==(const T& other) const { return is_valid() ? (other == it) : !(other.is_valid()); }
    template<class T>
    bool operator!=(const T& other) const { return !(*this == other); }

    _auto_iter& operator++() { ++it; return *this; }
    _auto_iter  operator++(int) { _auto_iter result(*this); ++it; return result; }
    _auto_iter& operator--() { --it; return *this; }
    _auto_iter  operator--(int) { _auto_iter result(*this); --it; return result; }

    reference operator*() const { return *it; }
    pointer operator->() const { return &(*it); }

    constexpr bool is_valid() const { return it.is_valid(); }
    constexpr bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }
    // get the end-iterator
    // NOTE: default-constructed Iterator should be equivalent to the end-iterator (compare == with the end-iterator), this is the user's responsability!
    constexpr auto get_end() const { return it.get_end(); }
    Iterator get_iter() const { return it; }
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
