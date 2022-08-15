
#pragma once

#include <functional>
#include "stl_utils.hpp"
#include "iter_factory.hpp"

namespace mstd {
  // this is an iterator that transforms items of a range on the fly
  // NOTE: you can choose to pass the iterator to the transformation instead of its dereference
  // **IMPORTANT NOTE**: dereferencing such an iterator may (depending on the transformation) generate and return an rvalue, not an lvalue reference
  //                     Thus, users must avoid drawing non-const references from the result of de-referencing such iterators (otherwise: ref to temporary)
  template<class _Iter, class _Transformation, bool pass_iterator = false>
    requires (std::is_invocable_v<_Transformation, std::conditional_t<pass_iterator, _Iter, reference_of_t<_Iter>>>)
  class proto_transforming_iterator: public InheritableIter<_Iter> {
    using Parent = _Iter;
    
    mstd::mutableT<_Transformation> trans;
    
    decltype(auto) deref() const {
      const Parent* p = static_cast<const Parent*>(this);
		  if constexpr (pass_iterator) return *p; else return **p;
    }

    void increment_trans() {
      if constexpr (mstd::really_pre_incrementable<Transformation>)
        ++(trans.value);
    }
  public:
    using Transformation = _Transformation;
    using Iterator = _Iter;
    using UnderlyingIterator = _Iter;
    using ParentDeref = typename std::iterator_traits<Parent>::reference;
    using TransInput = std::conditional_t<pass_iterator, proto_transforming_iterator, ParentDeref>;
    // this will not compile if pass_iter == true, since proto_transforming_iterator is not fully defined yet
    //static_assert(std::is_invocable_v<Transformation, TransInput>); 
    //using TransOutput = std::invoke_result_t<Transformation, TransInput>;
    //using ConstTransOutput = std::invoke_result_t<Transformation, const TransInput>;
    using TransOutput = decltype(std::declval<Transformation>()(std::declval<TransInput>()));
    using ConstTransOutput = decltype(std::declval<Transformation>()(std::declval<const TransInput>()));

    using difference_type = ptrdiff_t;
    using reference       = TransOutput;
    using const_reference = ConstTransOutput;
    using value_type      = std::remove_reference_t<reference>;
    using pointer         = pointer_from_reference<reference>;
    using const_pointer   = pointer_from_reference<const_reference>;
    using iterator_category = typename mstd::iterator_traits<Iterator>::iterator_category;
    static constexpr bool reference_is_rvalue = !std::is_reference_v<reference>;

    proto_transforming_iterator() = default;
    proto_transforming_iterator(proto_transforming_iterator&&) = default;
    proto_transforming_iterator(const proto_transforming_iterator&) = default;

    // construct from an iterator alone, default-construct the transformation
    template<class T> requires ((!std::is_same_v<std::remove_cvref_t<T>, proto_transforming_iterator>) &&
        std::is_default_constructible_v<Transformation> && std::is_constructible_v<Parent, T&&>)
    proto_transforming_iterator(T&& iter): Parent(std::forward<T>(iter)), trans() {}
    // construct from iter and transformaiton
    template<class T, class F> requires (std::is_constructible_v<Parent, T&&> && std::is_constructible_v<Transformation, F&&>)
    proto_transforming_iterator(T&& iter, F&& f): Parent(std::forward<T>(iter)), trans{std::forward<F>(f)} {}


    // std::piecewise construction of the iter and the transformation
    template<class IterTuple, class TransTuple>
    constexpr proto_transforming_iterator(const std::piecewise_construct_t, IterTuple&& iter_init, TransTuple&& trans_init):
      Parent(make_from_tuple<Parent>(std::forward<IterTuple>(iter_init))),
      trans{make_from_tuple<Transformation>(std::forward<TransTuple>(trans_init))}
    {}


    // we have operator= and operator== for our iterator type (Iter) setting only the iterator, but not the transformation function
    proto_transforming_iterator& operator=(const proto_transforming_iterator&) = default;
    proto_transforming_iterator& operator=(proto_transforming_iterator&&) = default;
    proto_transforming_iterator& operator=(const Iterator& other) { Parent::operator=(other); }
    proto_transforming_iterator& operator=(Iterator&& other) { Parent::operator=(std::move(other)); }

    proto_transforming_iterator& operator++() { Parent::operator++(); increment_trans(); return *this; }
    proto_transforming_iterator operator++(int) { proto_transforming_iterator result(*this); ++(*this); return result; }

    decltype(auto) operator*() & { return trans.value(deref()); }
    decltype(auto) operator*() const & { return trans.value(deref()); }
    decltype(auto) operator*() && { return std::move(trans.value)(deref()); }
    decltype(auto) operator->() & {
      decltype(auto) retval = operator*();
      if constexpr (std::is_reference_v<decltype(retval)>)
        return &(trans.value(deref()));
      else return retval;
    }
    decltype(auto) operator->() const & {
      decltype(auto) retval = operator*();
      if constexpr (std::is_reference_v<decltype(retval)>)
        return &(trans.value(deref()));
      else return retval;
    }
    decltype(auto) operator->() && {
      decltype(auto) retval = operator*();
      if constexpr (std::is_reference_v<decltype(retval)>)
        return &(trans.value(deref()));
      else return retval;
    }

  };

  /*
  // the second argument is either a transformation function or, if the second template argument ("T") is not std::invocable,
  // then we interpret it as target_value_type and the transformation function will be std::function<target_value_type(Iter::reference)>
  template<class Iter, class Result, bool pass_iterator>
  struct _transforming_iterator {
    using F = std::function<Result(mstd::reference_of_t<Iter>)>;
    using type = proto_transforming_iterator<Iter, F, pass_iterator>;
  };  
  template<class Iter, class Transformation, bool pass_iterator>
    requires (std::is_invocable_v<Transformation, std::conditional_t<pass_iterator, Iter, reference_of_t<Iter>>>)
  struct _transforming_iterator<Iter, Transformation, pass_iterator> {
    using type = proto_transforming_iterator<Iter, Transformation, pass_iterator>;
  };
  template<class Iter, class T, bool pass_iterator = false>
  using transforming_iterator = typename _transforming_iterator<iterator_of_t<Iter>, T, pass_iterator>::type;
  */
  template<class Iter, class T, bool pass_iterator = false>
  using transforming_iterator = proto_transforming_iterator<Iter, T, pass_iterator>;

  // factories
  template<class Iter, class T, bool pass_iterator = false, class BeginEndTransformation = void, class EndIterator = iterator_of_t<Iter>>
  using TransformingIterFactory = IterFactory<transforming_iterator<Iter, T, pass_iterator>, BeginEndTransformation, EndIterator>;

  template<IterableType Container, class Trans, bool pass_iterator = false>
  auto get_transforming(Container&& c, Trans&& trans) {
    return TransformingIterFactory<iterator_of_t<Container>, Trans, pass_iterator, void>(std::forward<Container>(c), std::forward<Trans>(trans));
  }


  // ----------- special case: selecting first or second element from a pair ---------------

  template<class T, size_t get_num> struct _selecting_iterator { using type = transforming_iterator<T, selector<get_num>>; };
  template<IterableType T, size_t get_num> struct _selecting_iterator<T,get_num> { using type = transforming_iterator<iterator_of_t<T>, selector<get_num>>; };
  template<class T, size_t get_num> using selecting_iterator = typename _selecting_iterator<T, get_num>::type;

  template<class T> using firsts_iterator = selecting_iterator<T, 0>;
  template<class T> using seconds_iterator = selecting_iterator<T, 1>;

  template<class T, size_t get_num, class BeginEndTransformation = void, class EndIterator = iterator_of_t<T>>
  using TupleItemIterFactory = IterFactory<selecting_iterator<T, get_num>, BeginEndTransformation, EndIterator>;

  // convenience classes for selecting the first and second items of tuples/pairs
  template<class T> using FirstsFactory = TupleItemIterFactory<T, 0>;
  template<class T> using SecondsFactory = TupleItemIterFactory<T, 1>;

  // convenience functions to construct selecting iterator factories for returning the first and second items of tuples/pairs
  template<class TupleContainer>
  constexpr auto firsts(TupleContainer&& c) { return FirstsFactory<TupleContainer>(std::forward<TupleContainer>(c)); }
  template<class TupleContainer>
  constexpr auto seconds(TupleContainer&& c) { return SecondsFactory<TupleContainer>(std::forward<TupleContainer>(c)); }


}
