
#pragma once

#include <functional>

namespace pred {
  // Predicates

  template<class T, class... Args>
  concept PredicateType = (std::is_invocable_v<T, Args...> && std::is_same_v<std::invoke_result_t<T, Args...>, bool>);

  struct TruePredicate {
    template<class... Args> constexpr bool operator()(Args&&...) const { return true; }
  };

  template<class Predicate>
  struct NotPredicate: public Predicate {
    using Predicate::Predicate;
    template<class... Args>
    constexpr bool operator()(Args&&... args) const { return !Predicate::operator()(args...); }
    template<class... Args>
    constexpr bool operator()(Args&&... args) { return !Predicate::operator()(args...); }
  };


  using FalsePredicate = NotPredicate<TruePredicate>;
  using BinaryEqualPredicate = std::equal_to<void>;
  using BinaryUnequalPredicate = NotPredicate<BinaryEqualPredicate>;

  template<class T, T t>
  struct UnaryEqualPredicate {
    constexpr bool operator()(const T& x) const { return t == x; }
    constexpr bool operator()(const T& x) { return t == x; }
  };

  template<class T, T t>
  using UnaryUnequalPredicate = NotPredicate<UnaryEqualPredicate<T, t>>;

  template<class PredicateA, class PredicateB, class Connector>
  struct ChainPredicate {
    PredicateA predA;
    PredicateB predB;
    Connector conn;

    template<class C>
    constexpr ChainPredicate(C&& c):
      conn(std::forward<C>(c))
    {}

    template<class A, class B, class C>
    constexpr ChainPredicate(A&& a, B&& b):
      predA(std::forward<A>(a)), predB(std::forward<B>(b))
    {}
   
    template<class A, class B, class C>
    constexpr ChainPredicate(A&& a, B&& b, C&& c):
      predA(std::forward<A>(a)), predB(std::forward<B>(b)), conn(std::forward<C>(c))
    {}

    template<class... Args>
    constexpr bool operator()(const Args&... args) const { return Connector(predA(args...), predB(args...)); }
    
    template<class... Args>
    constexpr bool operator()(const Args&... args) { return Connector(predA(args...), predB(args...)); }
  };


  template<class PredicateA, class PredicateB>
  using AndPredicate = ChainPredicate<PredicateA, PredicateB, std::logical_and<bool>>;
  template<class PredicateA, class PredicateB>
  using OrPredicate = ChainPredicate<PredicateA, PredicateB, std::logical_or<bool>>;

  template<class ItemPredicate, size_t get_num>
  struct SelectingPredicate: public ItemPredicate {
    using ItemPredicate::ItemPredicate;
    template<class Tuple> constexpr auto& operator()(const Tuple& p) const { return ItemPredicate::operator()(std::get<get_num>(p)); }
    template<class Tuple> constexpr auto& operator()(const Tuple& p) { return ItemPredicate::operator()(std::get<get_num>(p)); }
  };

  template<class _ItemPredicate>
  using MapKeyPredicate = SelectingPredicate<_ItemPredicate, 0>;
  template<class _ItemPredicate>
  using MapValuePredicate = SelectingPredicate<_ItemPredicate, 1>;

  template<class Compare, class CmpTarget = typename Compare::second_argument_type>
  struct ComparePredicate {
    Compare cmp;
    CmpTarget cmp_target;

    constexpr ComparePredicate() = default;

    template<class T, class... Args> requires std::is_constructible_v<Compare, T>
    constexpr ComparePredicate(T&& t, Args&&... args):
      cmp(std::forward<T>(t)), cmp_target(std::forward<Args>(args)...)
    {}

    template<class X> constexpr bool operator()(const X& x) const { return cmp(x, cmp_target); }
    template<class X> constexpr bool operator()(const X& x) { return cmp(x, cmp_target); }
  };

  template<class Compare, class CmpTarget = typename Compare::second_argument_type>
  using MapValueComparePredicate = MapValuePredicate<ComparePredicate<Compare, CmpTarget>>;

  // predicate for containers of sets, returning whether the given set is empty
  struct EmptyPredicate {
    template<std::IterableType _Set> constexpr bool operator()(const _Set& x) const { return x.empty();} 
    template<std::IterableType _Set> constexpr bool operator()(const _Set& x) { return x.empty();} 
  };
  using NonEmptyPredicate = NotPredicate<EmptyPredicate>;

  // a predicate returning true/or false depending on whether the query is in a given set
  template<std::IterableType Container, bool is_in = true>
  struct ContainmentPredicate {
    const Container& c;
    constexpr ContainmentPredicate(const Container& _c): c(_c) {}
    template<class Item> constexpr bool operator()(const Item& x) const { return test(c, x) == is_in; }
    template<class Item> constexpr bool operator()(const Item& x) { return test(c, x) == is_in; }
  };

  // if P is iterable, get its containment predicate
  template<class P> struct _AsContainmentPred { using type = P; };
  template<std::IterableType P> struct _AsContainmentPred<P> { using type = ContainmentPredicate<P>; };
  template<class P> using AsContainmentPred = typename _AsContainmentPred<P>::type;


#warning "TODO: replace all this by lambdas"
#warning "TODO: implement static predicates correctly"
  /* THIS DOESN'T WORK YET... ULTIMATELY, I WANT TWO DIFFERENT filter_iterators DEPENDING ON WHETHER THE predicate IS STATIC
  // we'll assume that a class with a static member ::value(...) is a static predicate
  template <typename T, typename X = int> // if value(cont X&) is templated, check for value<int>(...) by default
  struct has_static_value_method {
    template <class, class> class checker;
    template <typename C> static std::true_type test(checker<C, decltype(C::value)> *);
    template <typename C> static std::true_type test(checker<C, decltype(C::template value<X>)> *);
    template <typename C> static std::false_type test(...);
    typedef decltype(test<T>(nullptr)) type;
    static const bool value = std::is_same<std::true_type, decltype(test<T>(nullptr))>::value;
	};
	template<class T> constexpr bool has_static_value_method_v = has_static_value_method<T>::value;
	template<class T> constexpr bool is_static_predicate_v = has_static_value_method_v<T>;
  */
}
