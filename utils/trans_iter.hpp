
#pragma once

#include "stl_utils.hpp"

namespace std {
  // this is an iterator that transforms items of a range on the fly
  // **IMPORTANT NOTE**: dereferencing such an iterator will generate and return an rvalue, not an lvalue reference as people are used to!
  //                     Thus, users must avoid drawing non-const references from the result of de-referencing this iterator (otherwise: ref to temporary)
  template<class Iter,
           class target_value_type,
           invocable<typename Iter::reference> Transformation = std::function<target_value_type(typename Iter::reference)>>
  class _transforming_iterator {
    Iter it;
    Transformation trans;
    using TransArg = typename Iter::reference;
  public:
    using difference_type = ptrdiff_t;
    using value_type      = target_value_type;
    using pointer         = self_deref<target_value_type>; // NOTE: the self-deref class returns itself on de-reference
    using const_pointer   = self_deref<const target_value_type>;
    using reference       = target_value_type;
    using const_reference = const target_value_type;
    using iterator_category = typename my_iterator_traits<Iter>::iterator_category;

    _transforming_iterator() = default;
    _transforming_iterator(_transforming_iterator&&) = default;
    _transforming_iterator(const _transforming_iterator&) = default;

    _transforming_iterator& operator=(const _transforming_iterator&) = default;
    _transforming_iterator& operator=(_transforming_iterator&&) = default;

    bool operator==(const _transforming_iterator& other) { return it == other.it; }
    bool operator!=(const _transforming_iterator& other) { return !operator==(other); }


    // construct from only one given container
    template<invocable<TransArg> F>
    _transforming_iterator(F&& f): trans{forward<F>(f)} {}
    template<class T, class F>
    _transforming_iterator(F&& f, T&& iter): it{forward<T>(iter)}, trans{forward<F>(f)} {}


    // NOTE: do not attempt to call ++ on the end-iterator lest you see segfaults
    _transforming_iterator& operator++() { ++it; }
    _transforming_iterator& operator++(int) { _transforming_iterator result(*this); ++this; return result; }

    reference operator*() const { return *it; }
    pointer operator->() const { return it.operator->(); }
  };
  
  // if the second template argument ("target_value_type") is invocable, then we interpret it as Transformation function
  //    and we derive the correct target_value_type as the return type of it
  template<class Iter, class target_value_type, class Transformation = std::function<target_value_type(typename Iter::value_type)>>
  struct __transforming_iterator { using type = _transforming_iterator<Iter, target_value_type, Transformation>; };
  template<class Iter, invocable<typename Iter::reference> Transformation, invocable<typename Iter::reference> IgnoreMe>
  struct __transforming_iterator<Iter, Transformation, IgnoreMe> { using type = _transforming_iterator<Iter, invoke_result_t<Transformation, typename Iter::reference>>; };

  template<class Iter,
           class target_value_type,
           invocable<typename Iter::reference> Transformation = std::function<target_value_type(typename Iter::reference)>>
  using transforming_iterator = typename __transforming_iterator<Iter, target_value_type, Transformation>::type;

}
