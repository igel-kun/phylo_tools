
#pragma once

#include <concepts>
#include <vector>
#include "utils.hpp"

namespace mstd {

  // NOTE: my concepts do not differentiate between T and T&. For example ContainerType<T> is true for T = std::vector<int>&.
  //        This is so you can write "ContainerType C" and use C as universal reference

  template<class T, class ... U> concept IsAnyOf = (std::same_as<T, U> || ...);

  // std::is_arithmetic is false for pointers.... why?
  template<class T> constexpr bool is_really_arithmetic_v = std::is_arithmetic_v<T> || std::is_pointer_v<T>;
  // anything that can be converted from and to int is considered "basically arithmetic"
  template<class T> constexpr bool is_basically_arithmetic_v = std::is_convertible_v<int, std::remove_cvref_t<T>> && std::is_convertible_v<std::remove_cvref_t<T>, int>;
  // std::weakly_incrementable has a whole sack full of other iterator-related requirements like default-constructibility and difference_type...
  template<class T> concept really_pre_incrementable = requires(T t){++t;};
  template<class T> concept really_post_incrementable = requires(T t){t++;};
  template<class T> concept really_int_incrementable = requires(T t, int x){t += x;};

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
	
  template<class T> concept StrictStringlike = is_stringlike_v<std::remove_const_t<T>>;
	template<class T> concept Stringlike = StrictStringlike<std::remove_cvref_t<T>>;

  template<class T> concept StrictStringlikeOrChar = StrictStringlike<T> || std::is_same_v<std::remove_const_t<T>, char>;
	template<class T> concept StringlikeOrChar = StrictStringlikeOrChar<std::remove_cvref_t<T>>;

  template<class T> concept has_iterator = requires { typename T::iterator;};
  // for reasons, C++20's std::span has no const_iterator yet (added in C++23)
  template<class T> concept has_const_iterator = requires { typename T::const_iterator;};
  template<class T> concept has_reference = requires { typename std::remove_reference_t<T>::reference; };

  // ever needed to get an iterator if T was non-const and a const_iterator if T was const? Try this:
  template<class T> struct _iterator_of {};
  template<class T> requires std::ranges::range<T>  struct _iterator_of<T> {
    using type = decltype(std::ranges::begin(std::declval<T&>()));
  };
  template<class T>
    requires (!std::ranges::range<T> && std::is_const_v<T> && has_const_iterator<T>)
  struct _iterator_of<T> { using type = typename T::const_iterator; };
  template<class T>
    requires (!std::ranges::range<T> && has_iterator<T> && (!std::is_const_v<T> || !has_const_iterator<T>))
  struct _iterator_of<T> { using type = typename T::iterator; };
  template<class T> struct _iterator_of<T*> { using type = T*; };
  template<class T, std::size_t N> struct _iterator_of<T (&)[N]> { using type = T*; };

  template<class T>
  using _iterator_of_t = typename _iterator_of<std::remove_reference_t<T>>::type;

  template<class T> concept ArithmeticType =  is_really_arithmetic_v<T>;
  template<class T> concept PointerType = std::is_pointer_v<std::remove_cvref_t<T>>;

  template<class T>
  concept HasAllocator = requires (T t) { typename T::allocator_type; };

  template<class T>
  constexpr bool is_vector_v = std::is_convertible_v<T, std::vector<typename T::value_type, typename T::allocator_type>>;
  template<class T> requires (!HasAllocator<T>)
  constexpr bool is_vector_v<T> = false;


  template<class T>
  concept StrictVectorType = is_vector_v<T>;
  template<class T>
  concept VectorType = StrictVectorType<std::remove_cvref_t<T>>;

  template<class T>
  concept VectorOrStringType = VectorType<T> || std::is_same_v<std::remove_cvref_t<T>, std::string>;
  template<class T>
  concept StrictVectorOrStringType = (VectorOrStringType<T> && !std::is_reference_v<T>);

  template<class T, class I = size_t>
  concept is_indexible_v = requires (T& t, const I& i) { {t[i]}; };
  template<class T, class I = size_t>
  concept IndexibleType = is_indexible_v<T, I>;
  template<class T, class I = size_t>
  concept StrictIndexibleType = IndexibleType<T, I>  && !std::is_reference_v<T>;


  template<class T> 
  concept IterableType = requires(T a) {
    typename _iterator_of_t<T>;
    typename _iterator_of_t<const T>;
    // NOTE: it seems forward_iterator concept cannot be satisfied by the proxy iterator of raw_std::vector_map :/
    //requires forward_iterator<typename T::iterator>;
    //requires forward_iterator<typename T::const_iterator>;
    std::begin(a);
    std::end(a);
	};

  template<class T>
  concept StrictIterableType = (IterableType<T> && !std::is_reference_v<T>);
  template<class T>
  concept OptionalIterableType = std::is_void_v<T> || IterableType<T>;

  // if we need T to support reporting its size via T::size() 
  template <class T> 
  concept IterableTypeWithSize = IterableType<T> && requires(T a) {
    { a.size() }    -> std::same_as<typename std::remove_cvref_t<T>::size_type>;
    { a.empty() }   -> std::same_as<bool>;
	};

  template<IterableType T> using BeginType = decltype(std::begin(std::declval<T>()));
  template<IterableType T> using EndType = decltype(std::end(std::declval<T>()));
  // a concept for iterable types in which begin() and end() have the same type (this is apparently needed for some STL stuff like std::vector::insert)
  template<class T>
  concept IterableTypeWithSameIterators = IterableType<T> && std::is_same_v<BeginType<T>, EndType<T>>;


  template<class T> struct iterator_of { using type = T; };
  template<IterableType T> struct iterator_of<T>: public _iterator_of<T> {};
  template<class T> using iterator_of_t = typename iterator_of<std::remove_reference_t<T>>::type;
  template<class T> using reverse_iterator_of_t = std::reverse_iterator<iterator_of_t<T>>;
  template<class T> using const_iterator_of_t = iterator_of_t<const std::remove_reference_t<T>>;


  template<class T>
  concept HasIterTraits = requires { typename std::iterator_traits<iterator_of_t<T>>::reference; };

  template<class Iter, class T>
  concept dereferencable_to = requires(Iter it) {
    { *it } -> std::convertible_to<T>;
  };


  // NOTE: 
  // we do not need to store the end-iterator if the iterator type has "bool is_valid() const"
  // (for example, the _auto_iter itself -- imagine an _auto_iter of _auto_iters)
  template<class Iter>
  concept iter_verifyable = HasIterTraits<std::remove_cvref_t<Iter>> && requires(const Iter i) {
    { i.is_valid() } -> std::convertible_to<bool>;
  };


  // concept checking for STL-style container (thanks to https://stackoverflow.com/questions/60449592 )
  template <class T> 
  concept ContainerType = IterableTypeWithSize<T> && requires(T a) {
    requires std::destructible<typename std::remove_cvref_t<T>::value_type>;
    //requires std::same_as<typename std::remove_cvref_t<T>::reference, typename std::remove_cvref_t<T>::value_type &>;
    //requires std::same_as<typename std::remove_cvref_t<T>::const_reference, const typename std::remove_cvref_t<T>::value_type &>;
    requires std::signed_integral<typename std::remove_cvref_t<T>::difference_type>;
    requires std::same_as<typename std::remove_cvref_t<T>::difference_type, typename std::iterator_traits<typename std::remove_cvref_t<T>::iterator>::difference_type>;
    requires std::same_as<typename std::remove_cvref_t<T>::difference_type, typename std::iterator_traits<typename std::remove_cvref_t<T>::const_iterator>::difference_type>;
  };
  
  template<class T>
  concept OptionalContainerType = std::is_void_v<T> || ContainerType<T>;

  template <class T> 
  concept UnorderedContainerType = ContainerType<T> && requires { typename T::hasher; };
  template<class T>
  concept OptionalUnorderedContainerType = std::is_void_v<T> || UnorderedContainerType<T>;
  template<class C, class V>
  concept ContainerOfType = ContainerType<C> &&
    std::is_same_v<std::remove_cvref_t<V>, std::remove_cvref_t<typename std::iterator_traits<_iterator_of_t<C>>::value_type>>;

  template<class Iter, class C>
  concept StrictIteratorTypeOf = ContainerType<C> && !ContainerType<Iter> &&
                                  std::is_same_v<typename std::iterator_traits<Iter>::value_type, typename C::value_type>;
  template<class Iter, class C>
  concept IteratorTypeOf = StrictIteratorTypeOf<std::remove_reference_t<Iter>, std::remove_reference_t<C>>;


	// a set is a container that supports count()
	template<class T>
	concept StrictSetType = requires(T a, typename T::value_type v) {
		requires ContainerType<T>;
		{ a.count(v) } -> std::convertible_to<size_t>;
		{ a.emplace(v).second } -> std::convertible_to<bool>;
	};
  template<class T>
  concept SetType = StrictSetType<std::remove_cvref_t<T>>;
  template<class T>
  concept OptionalSetType = std::is_void_v<T> || SetType<T>;

  template<class T>
	concept StrictMultiSetType = requires(T a, typename T::value_type v) {
		requires ContainerType<T>;
		{ a.count(v) } -> std::convertible_to<size_t>;
		{ *(a.emplace(v)) } -> std::convertible_to<typename T::value_type>;
	};
  template<class T>
  concept MultiSetType = StrictMultiSetType<std::remove_cvref_t<T>>;
  template<class T>
  concept OptionalMultiSetType = std::is_void_v<T> || MultiSetType<T>;


  // a map is something mapping key_type to value_type with operator[]
	template<class T>
	concept MapType = requires(std::remove_cvref_t<T> a, typename std::remove_reference_t<T>::key_type key) {
		requires ContainerType<T>;
		{ a[key] } -> std::same_as<typename std::remove_reference_t<T>::mapped_type&>;
	};

  //template<class T>
  //concept MapType = StrictMapType<std::remove_cvref_t<T>>;
  template<class T>
  concept OptionalMapType = std::is_void_v<T> || MapType<T>;

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

}
