
#pragma once

#include "stl_utils.hpp"

namespace std{


  template<class Ptr>
  struct PointerIterWrapper: public std::iterator_traits<Ptr> {
    using Tptr = Ptr;
    using TptrRef = Tptr&;
    using TptrConstRef = const Tptr&;

    Tptr data = nullptr;

    operator TptrRef() { return data; }
    operator TptrConstRef() const { return data; }

    PointerIterWrapper(const Tptr& _data): data(_data) {}
  };

  template<class T>
  using InheritableIter = std::conditional_t<std::is_pointer_v<T>, PointerIterWrapper<T>, T>;


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
  class _auto_iter: public InheritableIter<Iterator> {
    static_assert(!iter_verifyable<Iterator>);
    EndIterator end_it;
   public:
    using Parent = InheritableIter<Iterator>;
    using iterator = Iterator;
    using const_iterator = Iterator;

    // when default-constructed, end_it == *this, so is_valid() will be false
    _auto_iter(): Parent(), end_it(begin()) {}

    // construct from two iterators (begin and end)
    template<class _Iterator, class _EndIterator>
      requires (is_convertible_v<remove_cvref_t<_Iterator>,Iterator> && is_convertible_v<remove_cvref_t<_EndIterator>, EndIterator>)
    constexpr _auto_iter(_Iterator&& _it, _EndIterator&& _end): Parent(forward<_Iterator>(_it)), end_it(forward<_EndIterator>(_end)) {}
    // construct from a container
    template<IterableType Container> requires (is_convertible_v<iterator_of_t<Container>, Iterator>)
    _auto_iter(Container&& c): _auto_iter(std::begin(c), std::end(c)) {}
    // piecewise construct
    template<class IterTuple, class EndTuple>
    _auto_iter(const piecewise_construct_t, IterTuple&& iter_init, EndTuple&& end_init):
      Parent(make_from_tuple<Iterator>(forward<IterTuple>(iter_init))),
      end_it(make_from_tuple<EndIterator>(forward<EndTuple>(end_init)))
    {}

    // copy and move-construction & assignment are default
    _auto_iter(const _auto_iter&) = default;
    _auto_iter(_auto_iter&&) = default;
    _auto_iter& operator=(const _auto_iter&) = default;
    _auto_iter& operator=(_auto_iter&&) = default;

    template<class T>
    bool operator==(const T& other) const { return is_valid() ? Iterator::operator==(other.it) : !(other.is_valid()); }
    template<class T>
    bool operator!=(const T& other) const { return !(other == *this); }

    bool is_valid() const { return end_it != static_cast<const Iterator&>(*this); }
    
    const Iterator& begin() const & { return *this; }
    Iterator&& begin() && { return *this; }
    Iterator& begin() & { return *this; }

    const EndIterator& end() const & { return end_it; }
    EndIterator&  end() & { return end_it; }
    EndIterator&& end() && { return move(end_it); }
  };


  template<class Iterator> requires (iter_verifyable<Iterator>)
  struct _auto_iter<Iterator, void>: public Iterator {
    using iterator = Iterator;
    using const_iterator = Iterator;

    // this is a more thorough form of inheriting Iterator's constructors that also catches copy/move constructing the _auto_iter from an Iterator
    template<class... Args>
    _auto_iter(Args&&... args): Iterator(forward<Args>(args)...) {}

    const Iterator& begin() const & { return *this; }
    Iterator& begin()      & { return *this; }
    Iterator&& begin()    && { return move(*this); }
    static auto end() { return GenericEndIterator(); }
  };


  // add some convenience functions to _auto_iters
  template<class T, class EndIterator = conditional_t<iter_verifyable<iterator_of_t<T>>, void, iterator_of_t<T>>>
  class auto_iter: public _auto_iter<iterator_of_t<T>, EndIterator> {
    using Parent = _auto_iter<iterator_of_t<T>, EndIterator>;
  public:
    using typename Parent::iterator;
    using Parent::Parent;
    using Parent::is_valid;

    size_t size() const { return distance(static_cast<const iterator&>(*this), Parent::end()); }
    bool empty() const { return !is_valid(); }
    bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }

    bool operator==(const GenericEndIterator&) const { return is_valid(); }
    bool operator!=(const GenericEndIterator&) const { return !is_valid(); }
    auto_iter& operator++() { ++static_cast<Parent&>(*this); return *this; }
    auto_iter operator++(int) { _auto_iter result = *this; ++(*this); return result; }

    // copy the elements in the traversal to a container using the 'append()'-function
    template<class _Container>
    _Container& append_to(_Container& c) const { append(c, *this); }
    template<class _Container = vector<remove_cvref_t<value_type_of_t<T>>>>
    _Container to_container() const { _Container result; append(result, *this); return result; }
  };

  template<class T>
  struct auto_iter<auto_iter<T>>: public auto_iter<T> {
    using auto_iter<T>::auto_iter;
  };


  template<IterableType Container,
           class Iterator = iterator_of_t<Container>>
  auto_iter<Iterator> get_auto_iter(Container&& c) { return c; }

  template<IterableType Container,
           class KeyType,
           class Iterator = iterator_of_t<Container>>
  auto_iter<Iterator> auto_find(Container&& c, const KeyType& key) { return {c.find(key), end(c)}; }
 
} //namespace
