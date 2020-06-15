
#pragma once

namespace std{
  // Predicates
#warning "TODO: automatically detect static and dynamic predicates"
  struct StaticPredicate { inline static constexpr bool is_static = true; };
  struct DynamicPredicate { inline static constexpr bool is_static = false; };
  // note: for an iterator it, use pred.value(it) to get the value of the predicate for the item at position it
  template<class Predicate>
  struct StaticNotPredicate: public Predicate
  { template<class X> static bool value(const X& x) { return !Predicate::value(x); } };
  template<class Predicate>
  struct DynamicNotPredicate: public Predicate
  {
    using Predicate::Predicate;
    template<class X> bool value(const X& x) const { return !Predicate::value(x); }
  };
  template<class Predicate>
  using NotPredicate = conditional_t<Predicate::is_static, StaticNotPredicate<Predicate>, DynamicNotPredicate<Predicate>>;

  struct TruePredicate: public StaticPredicate { template<class X> static bool value(const X& x) { return true; } };
  using FalsePredicate = NotPredicate<TruePredicate>;

  template<class T, T t>
  struct StaticEqualPredicate: public StaticPredicate
  { static bool value(const T& x) { return t == x; } };
  template<class T, T* t>
  struct StaticEqualPtrPredicate: public StaticPredicate
  { static bool value(const T& x) { return *t == x; } };

  template<class T>
  struct DynamicEqualPredicate: public DynamicPredicate
  {
    T t;
    template<class... Args>
    DynamicEqualPredicate(Args&&... args): t(forward<Args>(args)...) {}
    bool value(const T& x) const { return t == x; }
  };

  template<class _ItemPredicate, size_t get_num>
  struct StaticSelectingPredicate: public _ItemPredicate
  { template<class Pair> bool value(const Pair& p) const { return _ItemPredicate::value(std::get<get_num>(p)); } };

  template<class _ItemPredicate, size_t get_num>
  struct DynamicSelectingPredicate: public _ItemPredicate
  {
    using _ItemPredicate::_ItemPredicate;

    template<class Pair> 
    bool value(const Pair& p) const { return _ItemPredicate::value(std::get<get_num>(p)); }
  };
  template<class _ItemPred, size_t get_num>
  using SelectingPredicate = conditional_t<_ItemPred::is_static, StaticSelectingPredicate<_ItemPred, get_num>, DynamicSelectingPredicate<_ItemPred, get_num>>;

  template<class _ItemPredicate>
  using MapKeyPredicate = SelectingPredicate<_ItemPredicate, 0>;
  template<class _ItemPredicate>
  using MapValuePredicate = SelectingPredicate<_ItemPredicate, 1>;


  // predicate for containers of sets, returning whether the given set is empty
  struct EmptySetPredicate: public StaticPredicate
  { template<class _Set> static bool value(const _Set& x) {return x.empty();} };
  using NonEmptySetPredicate = NotPredicate<EmptySetPredicate>;

  // a predicate returning true/or false depending whether the query is in a given set
  template<class Container, bool is_in = true>
  struct ContainmentPredicate: public DynamicPredicate
  {
    using Item = typename Container::value_type;
    const Container* c;
    ContainmentPredicate(const Container& _c): c(&_c) {}
    template<class Item>
    bool value(const Item& x) const { return test(*c, x) == is_in; }
  };

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