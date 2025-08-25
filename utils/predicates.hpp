
#pragma once

#include <functional>

namespace pred {
  // Predicates

  template<class T, class... Args>
  concept PredicateType = (std::is_invocable_v<T, Args...> && std::is_same_v<std::invoke_result_t<T, Args...>, bool>);

  struct TruePredicate { template<class... Args> constexpr bool operator()(Args&&...) const { return true; } };

  template<class Predicate>
  struct NotPredicate: public Predicate {
    using Predicate::Predicate;
    template<class... Args> constexpr bool operator()(Args&&... args) const { return !Predicate::operator()(args...); }
    template<class... Args> constexpr bool operator()(Args&&... args) { return !Predicate::operator()(args...); }
  };


  using FalsePredicate = NotPredicate<TruePredicate>;
  using BinaryEqualPredicate = std::equal_to<void>;
  using BinaryUnequalPredicate = NotPredicate<BinaryEqualPredicate>;

  // a predicate returning true/or false depending on whether the query is in a given set
  template<mstd::IterableType<mstd::TR_PtrOK> Container_, bool invert = false>
  struct ContainmentPredicate {
    // replace references by pointers
    using Container = Container_;
    using Storage = std::remove_pointer_t<Container>;
    static constexpr bool is_indirect = std::is_pointer_v<Container>;

    Container c;
    constexpr ContainmentPredicate(const Container& _c): c(_c) {}
    constexpr ContainmentPredicate(Container&& _c): c(std::move(_c)) {}
    constexpr ContainmentPredicate(const Storage& _c) requires (is_indirect): c(&_c) {}
    
    template<class Item> requires mstd::is_testable<Storage, const Item&>
    constexpr bool operator()(const Item& x) const {
      return mstd::test(mstd::access(c), x) != invert;
    }
  };

  // if P is iterable, get its containment predicate
  template<class P> struct AsContainmentPred_ { using type = P; };
  template<mstd::IterableType P> struct AsContainmentPred_<P> { using type = ContainmentPredicate<P>; };
  template<class P> using AsContainmentPred = typename AsContainmentPred_<P>::type;

}
