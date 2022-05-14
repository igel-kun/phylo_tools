
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "trans_iter.hpp"

namespace std {
/*
  // spew out all first/second in a container of pairs
  template<class _TupleContainer>
  class TupleIterator
  {
  protected:
    using _Iterator = iterator_of_t<_TupleContainer>;

    _Iterator pair_it;
  public:
    using value_type = typename my_iterator_traits<_Iterator>::value_type;
    using reference = typename my_iterator_traits<_Iterator>::reference;
    //using const_reference = typename my_iterator_traits<_Iterator>::const_reference; // why oh why, STL, do you not define that for pointers???
    using const_reference = const_reference_t<reference>;

    TupleIterator(const _Iterator& _it): pair_it(_it)
    {}

    TupleIterator(_TupleContainer& c): pair_it(c.begin())
    {}

    //! increment operator
    TupleIterator& operator++()
    {
      ++pair_it;
      return *this;
    }

    //! post-increment
    TupleIterator operator++(int)
    {
      TupleIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    bool operator==(const TupleIterator& it) const { return pair_it == it.pair_it; }
    bool operator!=(const TupleIterator& it) const { return !operator==(it); }

    bool operator==(const _Iterator& it) const { return pair_it == it; }
    bool operator!=(const _Iterator& it) const { return !operator==(it); }

  };
*/

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

