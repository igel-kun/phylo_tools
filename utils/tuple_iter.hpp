
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "trans_iter.hpp"

namespace std {

  template<class T, size_t get_num> struct __selecting_iterator { using type = transforming_iterator<T, std::selector<get_num>>; };
  template<IterableType T, size_t get_num> struct __selecting_iterator<T,get_num> { using type = transforming_iterator<iterator_of_t<T>, std::selector<get_num>>; };
  template<class T, size_t get_num> using selecting_iterator = typename __selecting_iterator<T, get_num>::type;

  template<class T> struct _firsts_iterator { using type = selecting_iterator<T, 0>; };
  template<class T> struct _seconds_iterator { using type = selecting_iterator<T, 1>; };
  template<IterableType T> struct _firsts_iterator<T> { using type = selecting_iterator<iterator_of_t<T>, 0>; };
  template<IterableType T> struct _seconds_iterator<T> { using type = selecting_iterator<iterator_of_t<T>, 1>; };
  template<class T> using firsts_iterator = typename _firsts_iterator<remove_reference_t<T>>::type;
  template<class T> using seconds_iterator = typename _seconds_iterator<remove_reference_t<T>>::type;


  template<class T, size_t get_num, class BeginEndTransformation = void>
  using TupleItemIterFactory = IterFactory<selecting_iterator<T, get_num>, BeginEndTransformation>;

  // convenience classes for selecting the first and second items of tuples/pairs
  template<class T> using FirstsFactory = TupleItemIterFactory<T, 0>;
  template<class T> using SecondsFactory = TupleItemIterFactory<T, 1>;

  // convenience functions to construct selecting iterator factories for returning the first and second items of tuples/pairs
  template<class TupleContainer>
  constexpr FirstsFactory<remove_reference_t<TupleContainer>> firsts(TupleContainer&& c) { return forward<TupleContainer>(c); }
  template<class TupleContainer>
  constexpr SecondsFactory<remove_reference_t<TupleContainer>> seconds(TupleContainer&& c) { return forward<TupleContainer>(c); }


}

