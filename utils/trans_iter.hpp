
#pragma once

#include <functional>
#include "stl_utils.hpp"
#include "iter_factory.hpp"

namespace std {
  // this is an iterator that transforms items of a range on the fly
  // **IMPORTANT NOTE**: dereferencing such an iterator may (depending on the transformation) generate and return an rvalue, not an lvalue reference
  //                     Thus, users must avoid drawing non-const references from the result of de-referencing such iterators (otherwise: ref to temporary)
  template<class Iter, class Transformation>
  class _transforming_iterator {
    Iter it;
    Transformation trans;
  public:
    using difference_type = ptrdiff_t;
    using reference       = decltype(trans(*it));
    using const_reference = decltype(trans(as_const(*it)));
    using value_type      = remove_reference_t<reference>;
    using pointer         = pointer_from_reference<reference>;
    using const_pointer   = pointer_from_reference<const_reference>;
    using iterator_category = typename my_iterator_traits<Iter>::iterator_category;
    static constexpr bool reference_is_rvalue = !is_reference_v<reference>;

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

    _transforming_iterator& operator+=(int diff) { it += diff; return *this; }
    _transforming_iterator& operator-=(int diff) { it -= diff; return *this; }
    size_t operator-(const _transforming_iterator& other) const { return it - other.it; }
   

    reference operator*() { return trans(*it); }
    reference operator*() const { return trans(*it); }
    pointer operator->() { if constexpr (reference_is_rvalue) return operator*(); else return &(trans(*it)); }
    pointer operator->() const { if constexpr (reference_is_rvalue) return operator*(); else return &(trans(*it)); }
  };
  
  // the second argument is either a transformation function or, if the second template argument ("T") is not invocable,
  // then we interpret it as target_value_type and the transformation function will be std::function<target_value_type(Iter::reference)>
  template<class Iter, class Result>
  struct __transforming_iterator {
    using F = std::function<Result(std::reference_of_t<Iter>)>;
    using type = _transforming_iterator<Iter, F>;
  };  
  template<class Iter, invocable<reference_of_t<Iter>> Transformation>
  struct __transforming_iterator<Iter, Transformation> {
    using type = _transforming_iterator<Iter, Transformation>;
  };


  template<class Iter, class T>
  using transforming_iterator = typename __transforming_iterator<Iter, T>::type;

  // factories
  template<class Iter,
           class T,
           class BeginEndTransformation = void>
  using TransformingIterFactory = IterFactory<transforming_iterator<Iter, T>, BeginEndTransformation>;

}
