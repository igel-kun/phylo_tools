
#pragma once

#include "stl_utils.hpp"

namespace std{

  // a forward iterator that knows the end of the container & converts to false if it's at the end
  //NOTE: this also supports that the end iterator has a different type than the iterator, as long as they can be compared with "!="
  template<class _Iterator, class _EndIterator = _Iterator>
  struct fwd_auto_iter: public pair<_Iterator, _EndIterator>, public my_iterator_traits<_Iterator>
  {
    using Parent = pair<_Iterator, _EndIterator>;
    using Traits = my_iterator_traits<_Iterator>;
    using Parent::Parent;
    using Parent::first;
    using Parent::second;
    using typename Traits::reference;
    using typename Traits::const_reference;
    using typename Traits::pointer;
    using typename Traits::const_pointer;

    template<class Container, class = enable_if_t<is_container_v<Container>>>
    fwd_auto_iter(Container&& c): Parent(begin(c), end(c)) {}

    fwd_auto_iter& operator++() { ++first; return *this; }
    fwd_auto_iter operator++(int) { fwd_auto_iter it(*this); ++(*this); return it; }
    reference operator*() const { return *first;}
    pointer operator->() const { return first.operator->();}

    bool is_valid() const { return first != second; }
    bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }
    operator _Iterator() { return first; }
    operator const _Iterator() const { return first; }
    _Iterator get_iter() const { return first; }
  };

  template<class _Iterator, class _EndIterator = _Iterator>
  struct bd_auto_iter: public fwd_auto_iter<_Iterator, _EndIterator>
  {
    using Parent = fwd_auto_iter<_Iterator, _EndIterator>;
    using Parent::Parent;

    bd_auto_iter& operator--() { --Parent::first; return *this; }
    bd_auto_iter operator--(int) { bd_auto_iter it(*this); --(*this); return it; }
  };

  template<class _Iterator, class _EndIterator = _Iterator>
  struct ra_auto_iter: public bd_auto_iter<_Iterator, _EndIterator>
  {
    using Parent = bd_auto_iter<_Iterator, _EndIterator>;
    using Parent::Parent;
    using Parent::first;
    using typename Parent::difference_type;
    using typename Parent::reference;
    using typename Parent::const_reference;

    ra_auto_iter& operator+=(const difference_type n) { first += n; return *this; }
    ra_auto_iter operator+(const difference_type n) { ra_auto_iter it(*this); it += n; return it; }

    ra_auto_iter& operator-=(const difference_type n) { first -= n; return *this; }
    ra_auto_iter operator-(const difference_type n) { ra_auto_iter it(*this); it -= n; return it; }

    difference_type operator-(const ra_auto_iter& it) const { return first - it.first; }
    reference operator[](const difference_type n) { return *(first + n); }
    const_reference operator[](const difference_type n) const { return *(first + n); }

    bool operator<(const ra_auto_iter& it)  const { return first < it.first; }
    bool operator<=(const ra_auto_iter& it) const { return first <= it.first; }
    bool operator>(const ra_auto_iter& it)  const { return it < *this; }
    bool operator>=(const ra_auto_iter& it) const { return it <= *this; }
  };

  // now, depending on the iterator_category of _Iterator, choose the right auto_iter
  template<class _Iterator, class _EndIterator, class = typename my_iterator_traits<_Iterator>::iterator_category>
  struct _auto_iter { using type = fwd_auto_iter<_Iterator, _EndIterator>; };
  template<class _Iterator, class _EndIterator>
  struct _auto_iter<_Iterator, _EndIterator, bidirectional_iterator_tag> { using type = bd_auto_iter<_Iterator, _EndIterator>; };
  template<class _Iterator, class _EndIterator>
  struct _auto_iter<_Iterator, _EndIterator, random_access_iterator_tag> { using type = ra_auto_iter<_Iterator, _EndIterator>; };
  
  template<class _Iterator, class _EndIterator = _Iterator>
  using auto_iter = typename _auto_iter<_Iterator, _EndIterator>::type;

  template<class Container,
           class _Iterator = iterator_of_t<Container>,
           class _EndIterator = _Iterator,
           class = enable_if_t<is_container_v<Container>>>
  auto_iter<_Iterator, _EndIterator> get_auto_iter(Container&& c) { return c; }

  template<class Container,
           class KeyType,
           class _Iterator = iterator_of_t<Container>,
           class _EndIterator = _Iterator,
           class = enable_if_t<is_container_v<Container>>>
  auto_iter<_Iterator, _EndIterator> auto_find(Container&& c, const KeyType& key) { return {c.find(key), end(c)}; }
 


} //namespace
