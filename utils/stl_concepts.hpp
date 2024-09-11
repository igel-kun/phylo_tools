
#pragma once

#include <concepts>
#include <vector>
#include "utils.hpp"
#include "runes.hpp"

namespace mstd {

  // std::conditional_t for unary templates
  template<bool condition, template<class> class X, template<class> class Y>
  struct conditional_template { template<class Z> using type = X<Z>; };
  template<template<class> class X, template<class> class Y>
  struct conditional_template<false, X, Y> { template<class Z> using type = Y<Z>; };

  template<class T> constexpr bool is_pair = false;
  template<class X, class Y> constexpr bool is_pair<std::pair<X,Y>> = true;



  template<class T, TypeRune rune, class ... U> concept IsAnyOfR = (mstd::is_same_v<T, U, rune> || ...);
  template<class T, class ... U> concept IsAnyOf = IsAnyOfR<T, TR_ConstRefOK, U...>;

  // std::is_arithmetic is false for pointers.... why?
  template<class T, TypeRune rune = TR_Strict> // NOTE: strict by default
  constexpr bool is_really_arithmetic_v = mstd::is_arithmetic_v<T, rune> || mstd::is_pointer_v<T, rune>;

  // anything that can be converted from and to int is considered "basically arithmetic"
  template<class T> constexpr bool is_basically_arithmetic_v = std::is_convertible_v<int, std::remove_cvref_t<T>> && std::is_convertible_v<std::remove_cvref_t<T>, int>;
  // std::weakly_incrementable has a whole sack full of other iterator-related requirements like default-constructibility and difference_type...
  template<class T> concept really_pre_incrementable = requires(T t){++t;};
  template<class T> concept really_pre_decrementable = requires(T t){--t;};
  template<class T> concept really_post_incrementable = requires(T t){t++;};
  template<class T> concept really_post_decrementable = requires(T t){t--;};
  template<class T> concept really_int_incrementable = requires(T t, int x){t += x;};

  // --------------- Stringlike -------------------
  // containers can be output to std::cout in the form [a b c ], unless they are strings or char* or string_view or....
	template<class T> constexpr bool is_stringlike_v = false;
	template<class... T> constexpr bool is_stringlike_v<std::basic_string<T...>> = true;
	template<> constexpr bool is_stringlike_v<std::string_view> = true;
	template<> constexpr bool is_stringlike_v<char*> = true;
	template<> constexpr bool is_stringlike_v<char[]> = true;
	template<int i> constexpr bool is_stringlike_v<char[i]> = true;
	template<> constexpr bool is_stringlike_v<const char*> = true;
	template<> constexpr bool is_stringlike_v<const char[]> = true;
	template<int i> constexpr bool is_stringlike_v<const char[i]> = true;
	
	template<class T, TypeRune rune = TR_ConstRefOK> concept Stringlike = apply_rune_v<T, rune> || is_stringlike_v<apply_rune_t<T, rune>>;
	template<class T, TypeRune rune = TR_ConstRefOK> concept StringlikeOrChar = Stringlike<T, rune> || mstd::is_same_v<T, char, rune>;

  template<class T> concept has_iterator = requires { typename T::iterator;};
  // for reasons, C++20's std::span has no const_iterator yet (added in C++23)
  template<class T> concept has_const_iterator = requires { typename T::const_iterator;};
  template<class T> concept has_reference = requires { typename std::remove_reference_t<T>::reference; };


  // ---------------- copy CV or & qualifiers from a type to the next -------------------
  // this is useful as what we're getting from a "const std::vector<int>" should be "const int", not "int" (the actualy value_type)

  template<typename T,typename U>
  struct copy_cv {
    using R =    std::remove_reference_t<T>;
    using U1 =   std::conditional_t<std::is_const_v<R>, std::add_const_t<U>, U>;
    using type = std::conditional_t<std::is_volatile_v<R>, std::add_volatile_t<U1>, U1>;
  };
  template<typename T,typename U> using copy_cv_t = typename copy_cv<T,U>::type;

  template<typename T,typename U>
  struct copy_ref {
    using URR = std::add_rvalue_reference_t<U>;
    using ULL = std::conditional_t<std::is_lvalue_reference_v<T>, std::add_lvalue_reference_t<U>, U>;
    using type = std::conditional_t<std::is_rvalue_reference_v<T>, URR, ULL>;
  };
  template<typename T,typename U> using copy_ref_t = typename copy_ref<T,U>::type;
  // NOTE: it is important to copy the const before copying the ref!
  template<typename T,typename U> using copy_cvref_t = copy_ref_t<T, copy_cv_t<T, U>>;

  // a class that returns itself on dereference 
  // useful for iterators returning rvalues instead of lvalue references
  template<class T>
  struct self_deref {
    T t;
   // template<class... Args> self_deref(Args&&... args): t(std::forward<Args>(args)...) {}
    T& operator*() { return t; }
    T* operator->() { return &t; }
    const T& operator*() const { return t; }
    const T* operator->() const { return &t; }
  };
  template<class R> // if the given reference is not a reference but an rvalue, then a pointer to it is modeled via self_deref
  using pointer_from_reference = std::conditional_t<std::is_reference_v<R>, std::add_pointer_t<std::remove_reference_t<R>>, self_deref<R>>;



  // ---------------- getting iterators of containers  -------------------
  template<class T> concept has_iter_category = requires { typename std::iterator_traits<T>::iterator_category; };
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept HasIterCategory = apply_rune_v<T, rune> || has_iter_category<apply_rune_t<T, rune>>;

  // ever needed to get an iterator if T was non-const and a const_iterator if T was const? Try this:
  template<class T> struct _iterator_of { using type = T; };
  template<class T> requires std::ranges::range<T> 
  struct _iterator_of<T> { using type = decltype(std::ranges::begin(std::declval<T&>())); };

  template<class T> requires (!std::ranges::range<T> && std::is_const_v<T> && has_const_iterator<T>)
  struct _iterator_of<T> { using type = typename T::const_iterator; };

  template<class T> requires (!std::ranges::range<T> && has_iterator<T> && (!std::is_const_v<T> || !has_const_iterator<T>))
  struct _iterator_of<T> { using type = typename T::iterator; };

  template<class T> struct _iterator_of<T*> { using type = T*; };
  template<class T, std::size_t N> struct _iterator_of<T (&)[N]> { using type = T*; };

  template<class T> using iterator_of_t = std::conditional_t<HasIterCategory<T>, T, typename _iterator_of<std::remove_reference_t<T>>::type>;

  template<class T> using reverse_iterator_of_t = std::reverse_iterator<iterator_of_t<T>>;
  template<class T> using const_iterator_of_t = iterator_of_t<const std::remove_reference_t<T>>;


  // ---------------- reference_of and value_type_of -----------------
  template<class T> concept HasDeref = requires (T t) { *t; };
  template<class T> concept HasBegin = requires (T t) { t.begin(); };

  template<class T> struct reference_of {};
  template<class T> requires (has_reference<T>)
  struct reference_of<T> {
    // some containers do not allow modifying their contents via iterators (such as std::unordered_set)
    // Thus, we copy constness of the iterator's reference onto the container to copy it onto the reference later on
    using Traits = std::iterator_traits<iterator_of_t<T>>;
    using TraitsConstness = std::remove_reference_t<typename Traits::reference>;
    using correctT = copy_cv_t<TraitsConstness, T>;
    // copy constness of the container onto the reference
    using Ref = typename T::reference;
    static constexpr bool returning_rvalue = !std::is_reference_v<Ref>;
    using value_type = copy_cv_t<correctT, std::remove_reference_t<Ref>>;
    using type = std::conditional_t<returning_rvalue, value_type, std::add_lvalue_reference_t<value_type>>;
  };

  template<class T> requires ((not has_reference<T>) && std::ranges::range<std::remove_const_t<T>>)
  struct reference_of<T> {
    using _type = std::ranges::range_reference_t<std::remove_const_t<T>>;
    using type = copy_cv_t<T, _type>;
  };
  template<class T> requires (not (has_reference<T> || std::ranges::range<std::remove_const_t<T>>) && HasBegin<T>)
  struct reference_of<T> { using type = decltype(std::begin(std::declval<T>())); };
  template<class T> requires (not (has_reference<T> || std::ranges::range<std::remove_const_t<T>> || HasBegin<T>) && HasDeref<T>)
  struct reference_of<T> { using type = decltype(*std::declval<T>()); };

  template<class T> using reference_of_t  = typename reference_of<std::remove_reference_t<T>>::type;
  template<class T> using value_type_of_t = std::remove_reference_t<reference_of_t<T>>;


  // ----------------- const_pointer and const_reference ---------------------
  // if a container has a const_pointer type, then return this type, otherwise return a pointer to const value_type
  template<class N, class T = void> struct get_const_ptr { using type = std::add_pointer_t<std::add_const_t<value_type_of_t<N>>>; };
  template<class N> struct get_const_ptr<N, std::void_t<typename N::const_pointer>> { using type = typename N::const_pointer; };
  template<class N> using const_pointer_of_t = typename get_const_ptr<std::remove_reference_t<N>>::type;

  // if a container has a pointer type, then return this type, otherwise return a pointer to value_type
  template<class N, class T = void> struct get_ptr { using type = std::add_pointer_t<value_type_of_t<N>>; };
  template<class N> struct get_ptr<N, std::void_t<typename N::pointer>> { using type = typename N::pointer; };
  template<class N> using pointer_of_t = typename get_ptr<std::remove_reference_t<N>>::type;

  // if a container has a const_reference type, then return this type, otherwise return an lvalue reference to const value_type
  template<class N, class T = void> struct get_const_ref { using type = std::add_lvalue_reference_t<std::add_const_t<typename std::iterator_traits<N>::value_type>>; };
  template<class N> struct get_const_ref<N, std::void_t<typename N::const_reference>> { using type = typename N::const_reference; };
  template<class N> using const_reference_of_t = typename get_const_ref<std::remove_reference_t<N>>::type;



  // ---------------- iterator traits  -----------------
  template<class T> concept has_iter_traits = requires { typename std::iterator_traits<iterator_of_t<T>>::reference; };
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept HasIterTraits = apply_rune_v<T, rune> || has_iter_traits<apply_rune_t<T, rune>>;

  // not all std::iterator_traits of the STL provide "const_pointer" and "const_reference", so I'll do that for them
  template<typename T> requires HasIterTraits<T>
  struct _iterator_traits: public std::iterator_traits<T> {
    // since the ::reference correctly gives "const T&", we'll just std::remove_reference_t from it
    using value_type = std::conditional_t<std::is_pointer_v<T>,
                          std::remove_reference_t<typename std::iterator_traits<T>::reference>,
                          typename std::iterator_traits<T>::value_type>;
    using const_reference = const_reference_of_t<T>;
    using const_pointer   = const_pointer_of_t<T>;
    using iterator = T;
  };
  template<typename T>
  using iterator_traits = _iterator_traits<iterator_of_t<T>>;

  template<class Ref, class IteratorTag = std::forward_iterator_tag>
  struct iter_traits_from_reference {
    static constexpr bool returning_rvalue = not std::is_reference_v<Ref>;
    using reference = Ref;
    using value_type = std::remove_reference_t<reference>;
    using const_reference = std::conditional_t<returning_rvalue, const value_type, const value_type&>;
    using pointer = pointer_from_reference<reference>;
    using const_pointer = pointer_from_reference<const_reference>;
    using difference_type = ptrdiff_t;
    using size_type = size_t;
    using iterator_category = IteratorTag; // by default we're a std::forward_iterator, overwrite this if you do bidirectional or random access
  };





  template<class T, TypeRune rune = TR_ConstRefOK> concept ArithmeticType = is_really_arithmetic_v<T, rune>;
  template<class T, TypeRune rune = TR_ConstRefOK> concept PointerType = mstd::is_pointer_v<T, rune>;

  template<class T> concept HasAllocator = requires (T t) { typename T::allocator_type; };

  template<class T> constexpr bool is_vector_v = false;
  template<class T, class A> constexpr bool is_vector_v<std::vector<T, A>> = true;

  template<class T, TypeRune rune = TR_ConstRefOK> concept VectorType = apply_rune_v<T, rune> || is_vector_v<apply_rune_t<T, rune>>;
  template<class T> concept StrictVectorType = is_vector_v<T, TR_Strict>;

  template<class T, TypeRune rune = TR_ConstRefOK> concept VectorOrStringType = VectorType<T, rune> || mstd::Stringlike<T, rune>;
  template<class T> concept StrictVectorOrStringType = VectorOrStringType<T, TR_Strict>;

  template<class T, class I = size_t>
  concept is_indexible = requires (T& t, const I& i) { {t[i]}; };
  template<class T, class I = size_t, TypeRune rune = TR_ConstRefOK>
  concept IndexibleType = apply_rune_v<T, rune> || is_indexible<apply_rune_t<T, rune>, I>;
  template<class T, class I = size_t>
  concept StrictIndexibleType = IndexibleType<T, I, TR_Strict>;

  template<class T> 
  concept is_iterable = not std::is_void_v<iterator_of_t<T>> &&  requires(T a) {
    std::begin(a);
    std::end(a);
	};

  template<class T, TypeRune rune = TR_ConstRefOK>
  concept IterableType = apply_rune_v<T, rune> || is_iterable<apply_rune_t<T, rune>>;
  template<class T> concept StrictIterableType = IterableType<T, TR_Strict>;
  template<class T> concept OptionalIterableType = IterableType<T, TR_ConstRefOK + TR_VoidOK>;

  // if we need T to support reporting its size via T::size() 
  template <class T> 
  concept is_iterable_with_size = is_iterable<T> && requires(T a) {
    { a.size() }    -> std::same_as<typename T::size_type>;
    { a.empty() }   -> std::same_as<bool>;
	};
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept IterableTypeWithSize = apply_rune_v<T, rune> || is_iterable_with_size<apply_rune_t<T, rune>>;
  template <class T> concept StrictIterableTypeWithSize = IterableTypeWithSize<T, TR_Strict>;
  template <class T> concept OptionalIterableTypeWithSize = IterableTypeWithSize<T, TR_ConstRefVoidOK>;

  template<IterableType T> using BeginType = decltype(std::begin(std::declval<T>()));
  template<IterableType T> using EndType = decltype(std::end(std::declval<T>()));
  // a concept for iterable types in which begin() and end() have the same type (this is apparently needed for some STL stuff like std::vector::insert)
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept IterableTypeWithSameIterators = IterableType<T, rune> && std::is_same_v<BeginType<T>, EndType<T>>;

  template<class Iter, class T>
  concept is_dereferencable_to = requires(Iter it) { { *it } -> std::convertible_to<T>; };
  template<class Iter, class T, TypeRune rune = TR_ConstRefOK>
  concept DereferencableTo = apply_rune_v<Iter, rune> || is_dereferencable_to<apply_rune_t<Iter, rune>, T>;


  // NOTE: 
  // we do not need to store the end-iterator if the iterator type has "bool is_valid() const"
  // (for example, the _auto_iter itself -- imagine an _auto_iter of _auto_iters)
  template<class Iter>
  concept is_verifyable_iter = HasIterTraits<Iter> && requires(const Iter i) {
    { i.is_valid() } -> std::convertible_to<bool>;
  };
  template<class Iter, TypeRune rune = TR_ConstRefOK>
  concept VerifyableIter = apply_rune_v<Iter, rune> || is_verifyable_iter<apply_rune_t<Iter, rune>>;


  // concept checking for STL-style container (thanks to https://stackoverflow.com/questions/60449592 )
  template <class T> 
  concept is_container = IterableTypeWithSize<T, TR_Strict> && requires(T a) {
    requires std::destructible<typename std::remove_cvref_t<T>::value_type>;
    //requires std::same_as<typename std::remove_cvref_t<T>::reference, typename std::remove_cvref_t<T>::value_type &>;
    //requires std::same_as<typename std::remove_cvref_t<T>::const_reference, const typename std::remove_cvref_t<T>::value_type &>;
    requires std::signed_integral<typename std::remove_cvref_t<T>::difference_type>;
    requires std::same_as<typename std::remove_cvref_t<T>::difference_type, typename std::iterator_traits<typename std::remove_cvref_t<T>::iterator>::difference_type>;
    requires std::same_as<typename std::remove_cvref_t<T>::difference_type, typename std::iterator_traits<typename std::remove_cvref_t<T>::const_iterator>::difference_type>;
  };

  template<class C, TypeRune rune = TR_ConstRefOK>
  concept ContainerType = apply_rune_v<C, rune> || is_container<apply_rune_t<C, rune>>;  
  template<class T> concept StrictContainerType = ContainerType<T, TR_Strict>;
  template<class T> concept OptionalContainerType = ContainerType<T, TR_ConstRefOK + TR_VoidOK>;

  template<class T> concept has_hasher = requires { typename T::hasher; };
  
  template<class C, TypeRune rune = TR_ConstRefOK>
  concept UnorderedContainerType = ContainerType<C, rune> && has_hasher<apply_rune_t<C, rune>>;
  template<class C> concept OptionalUnorderedContainerType = UnorderedContainerType<C, TR_ConstRefVoidOK>;

  template<class C, class Val, TypeRune rune = TR_ConstRefOK>
  concept ContainerOfType = ContainerType<C, rune> && std::is_same_v<std::remove_cvref_t<Val>, value_type_of_t<C>>;

	// a set is a container that supports count()
	template<class T>
	concept is_setlike_v = ContainerType<T, TR_Strict> && requires(T a, typename T::value_type v) {
		{ a.count(v) } -> std::convertible_to<size_t>;
		{ a.emplace(v).second } -> std::convertible_to<bool>;
	};
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept SetType = apply_rune_v<T, rune> || is_setlike_v<apply_rune_t<T, rune>>;
  template<class T> concept StrictSetType = SetType<T, TR_Strict>;
  template<class T> concept OptionalSetType = SetType<T, TR_ConstRefVoidOK>;


  template<class T>
	concept is_multisetlike_v = ContainerType<T, TR_Strict> && requires(T a, typename T::value_type v) {
		{ a.count(v) } -> std::convertible_to<size_t>;
		{ *(a.emplace(v)) } -> std::convertible_to<typename T::value_type>;
	};
  template<class T, TypeRune rune = TR_ConstRefOK>
  concept MultiSetType = apply_rune_v<T, rune> || is_multisetlike_v<apply_rune_t<T, rune>>;
  template<class T> concept StrictMultiSetType = MultiSetType<T, TR_Strict>;
  template<class T> concept OptionalMultiSetType = MultiSetType<T, TR_ConstRefVoidOK>;

  // a map is something mapping key_type to value_type with operator[]
  template<class T>
  concept maps_to_key = requires(T a, typename T::key_type key) {
		{ a[key] } -> std::same_as<typename T::mapped_type&>;
	};
  template<class T, TypeRune rune = TR_ConstRefOK>
	concept MapType = ContainerType<T, rune> && maps_to_key<apply_rune_t<T, rune>>;
  template<class T> concept StrictMapType = MapType<T, TR_Strict>;
  template<class T> concept OptionalMapType = MapType<T, TR_ConstRefVoidOK>;

  template<MapType M> using key_type_of_t = typename std::remove_reference_t<M>::key_type;
  template<MapType M> using mapped_type_of_t = typename std::remove_reference_t<M>::mapped_type;
  
  template<class T> struct mapped_or_value_type_of { using type = value_type_of_t<T>; };
  template<MapType M> struct mapped_or_value_type_of<M> { using type = mapped_type_of_t<M>; };
  template<class T>
  using mapped_or_value_type_of_t = typename mapped_or_value_type_of<T>::type;



  // a queue is something that has push and pop
	template<class T>
	concept QueueType = requires(T a, typename std::remove_cvref_t<T>::value_type v) {
		a.push(v);
		a.pop();
	};
  //template<class T>
  //concept QueueType = StrictQueueType<std::remove_cvref_t<T>>;
  template<class T>
  concept OptionalQueueType = std::is_void_v<T> || QueueType<T>;

  template<typename T>
  concept Printable = requires(T t) {
      { std::cout << t } -> std::same_as<std::ostream&>;
  };

  template <typename> struct is_tuple: std::false_type {};
  template <typename ...T> struct is_tuple<std::tuple<T...>>: std::true_type {};
  template<class T>
  constexpr bool is_tuple_v = is_tuple<T>::value;

  template<class T> concept StrictTupleType = is_tuple_v<T>;
  template<class T> concept TupleType = StrictTupleType<std::remove_cvref_t<T>>;

  // ----------------------- deriving from and basing on templates  ----------------------------------
  template <template <class...> class C, class...Ts>
  std::true_type is_derived_from_template_impl(const C<Ts...>*);

  template <template <class...> class C>
  std::false_type is_derived_from_template_impl(...);

  template <class T, template <class...> class C>
  using is_derived_from_template = decltype(is_derived_from_template_impl<C>(std::declval<T*>()));
  
  template <class T, template <class...> class C>
  static constexpr bool is_derived_from_template_v = is_derived_from_template<T, C>::value;


  // ---------------- Iterators ----------------------------
  // for reasons that escape me, std::iterator_traits depend on satisfaction of the followign concepts, but it's not defined by the STL...
  // however it is indispensible for debugging to know why std::iterator_traits will not work for a self-defined iterator...
  // for example, you make a new iterator, but std::iterator_traits<Iter> is empty... now try to find out why, I dare you, without using the following concepts
  template<class T>
  concept __Referenceable = requires { typename std::type_identity_t<T&>; };

  template<class I>
  concept __LegacyIterator = requires(I i) {
      {   *i } -> __Referenceable;
      {  ++i } -> std::same_as<I&>;
      { *i++ } -> __Referenceable;
  } && std::copyable<I>;

  template<class I>
  concept __LegacyInputIterator = __LegacyIterator<I> && std::equality_comparable<I> && requires(I i) {
    typename std::incrementable_traits<I>::difference_type;
    typename std::indirectly_readable_traits<I>::value_type;
    typename std::common_reference_t<std::iter_reference_t<I>&&, typename std::indirectly_readable_traits<I>::value_type&>;
    *i++;
    typename std::common_reference_t<decltype(*i++)&&, typename std::indirectly_readable_traits<I>::value_type&>;
    requires std::signed_integral<typename std::incrementable_traits<I>::difference_type>;
  };
}
