
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

  // a predicate returning true/or false depending on whether the query is in a given set
  template<mstd::IterableType Container, bool invert = false>
  struct ContainmentPredicate {
    const Container& c;
    constexpr ContainmentPredicate(const Container& _c): c(_c) {}
    template<class Item> constexpr bool operator()(const Item& x) const { return test(c, x) != invert; }
    template<class Item> constexpr bool operator()(const Item& x) { return test(c, x) != invert; }
  };

  // if P is iterable, get its containment predicate
  template<class P> struct _AsContainmentPred { using type = P; };
  template<mstd::IterableType P> struct _AsContainmentPred<P> { using type = ContainmentPredicate<P>; };
  template<class P> using AsContainmentPred = typename _AsContainmentPred<P>::type;

}
