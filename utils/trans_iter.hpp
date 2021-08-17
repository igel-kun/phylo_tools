
#pragma once

#include "stl_utils.hpp"
#include "iter_factory.hpp"

namespace std {
  // this is an iterator that transforms items of a range on the fly
  // **IMPORTANT NOTE**: dereferencing such an iterator will generate and return an rvalue, not an lvalue reference as people are used to!
  //                     Thus, users must avoid drawing non-const references from the result of de-referencing this iterator (otherwise: ref to temporary)
  template<class Iter,
           class target_value_type,
           class Transformation>
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

    // construct from an iterator alone, default-construct the transformation
    template<class T> requires (!is_same_v<remove_cvref_t<T>, _transforming_iterator>)
    _transforming_iterator(T&& iter): it(forward<T>(iter)), trans() {}
    // construct from iter and transformaiton
    template<class T, class F>
    _transforming_iterator(T&& iter, F&& f): it(forward<T>(iter)), trans(forward<F>(f)) {}


    // piecewise construction of the iter and the transformation
    template<class IterTuple, class TransTuple>
    constexpr _transforming_iterator(const piecewise_construct_t, IterTuple&& iter_init, TransTuple&& trans_init):
      it(make_from_tuple<Iter>(forward<IterTuple>(iter_init))),
      trans(make_from_tuple<Transformation>(forward<TransTuple>(trans_init)))
    {}


    // we have operator= and operator== for our iterator type (Iter) setting only the iterator, but not the transformation function
    _transforming_iterator& operator=(const _transforming_iterator&) = default;
    _transforming_iterator& operator=(_transforming_iterator&&) = default;
    _transforming_iterator& operator=(const Iter& other) { it = other; }
    _transforming_iterator& operator=(Iter&& other) { it = other; }

    bool operator==(const _transforming_iterator& other) const { return it == other.it; }
    bool operator==(const Iter& other) const { return it == other.it; }
  
    template<class T>
    bool operator!=(const T& other) const { return !operator==(other); }

    // NOTE: do not attempt to call ++ on the end-iterator lest you see segfaults
    _transforming_iterator& operator++() { ++it; return *this; }
    _transforming_iterator& operator++(int) { _transforming_iterator result(*this); ++this; return result; }

    reference operator*() const { return trans(*it); }
    pointer operator->() const { return operator*(); }
  };
  
  // if the second template argument ("target_value_type") is invocable, then we interpret it as Transformation function
  //    and we derive the correct target_value_type as the return type of it
  template<class Iter, class target_value_type, class Transformation = std::function<target_value_type(value_type_of_t<Iter>)>>
  struct __transforming_iterator {
    using type = _transforming_iterator<Iter, target_value_type, Transformation>;
  };  
  template<class Iter, invocable<reference_of_t<Iter>> Transformation>
  struct __transforming_iterator<Iter, Transformation, std::function<Transformation(value_type_of_t<Iter>)>> {
    using type = _transforming_iterator<Iter, invoke_result_t<Transformation, reference_of_t<Iter>>, Transformation>;
  };


  template<class Iter,
           class target_value_type,
           class Transformation = std::function<target_value_type(typename Iter::reference)>>
  using transforming_iterator = typename __transforming_iterator<Iter, target_value_type, Transformation>::type;

  // factories
  template<class Iter,
           class target_value_type,
           class Transformation = std::function<target_value_type(typename Iter::reference)>,
           class BeginEndTransformation = void>
  using TransformingIterFactory = IterFactory<transforming_iterator<Iter, target_value_type, Transformation>, BeginEndTransformation>;

}
