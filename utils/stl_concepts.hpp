
#include <vector>
#include "utils.hpp"

namespace mstd {

  // NOTE: my concepts do not differentiate between T and T&. For example ContainerType<T> is true for T = std::vector<int>&. This is so you can write "ContainerType C" and use C as universal reference

  // std::is_arithmetic is false for pointers.... why?
  template<class T> constexpr bool is_really_arithmetic_v = std::is_arithmetic_v<T> || std::is_pointer_v<T>;
  // anything that can be converted from and to int is considered "basically arithmetic"
  template<class T> constexpr bool is_basically_arithmetic_v = std::is_convertible_v<int, std::remove_cvref_t<T>> && std::is_convertible_v<std::remove_cvref_t<T>, int>;



  // ever needed to get an iterator if T was non-const and a const_iterator if T was const? Try this:
  template<class T> requires requires { typename T::iterator; typename T::const_iterator; } struct _iterator_of {
    using type = std::conditional_t<std::is_const_v<T>, typename T::const_iterator, typename T::iterator>;
  };
  template<class T> struct _iterator_of<T*> { using type = T*; };
  template<class T, std::size_t N> struct _iterator_of<T (&)[N]> { using type = T*; };
  template<class T>
  using _iterator_of_t = typename _iterator_of<std::remove_reference_t<T>>::type;

  template<class T> concept ArithmeticType =  is_really_arithmetic_v<T>;
  template<class T> concept PointerType = std::is_pointer_v<std::remove_cvref_t<T>>;

  template<class T>
  concept VectorType = std::is_convertible_v<std::remove_cvref_t<T>, std::vector<typename std::remove_reference_t<T>::value_type, typename std::remove_reference_t<T>::allocator_type>>;
  template<class T>
  concept StrictVectorType = (VectorType<T> && !std::is_reference_v<T>);

  template<class T>
  concept VectorOrStringType = VectorType<T> || std::is_same_v<std::remove_cvref_t<T>, std::string>;
  template<class T>
  concept StrictVectorOrStringType = (VectorOrStringType<T> && !std::is_reference_v<T>);


  template<class T>
  concept HasIterTraits = requires { typename std::iterator_traits<T>; };

  template<class Iter, class T>
  concept dereferencable_to = requires(Iter it) {
    { *it } -> std::convertible_to<T>;
  };

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

  template<class T> struct iterator_of { using type = T; };
  template<IterableType T> struct iterator_of<T>: public _iterator_of<T> {};
  template<class T> using iterator_of_t = typename iterator_of<std::remove_reference_t<T>>::type;
  template<class T> using reverse_iterator_of_t = std::reverse_iterator<iterator_of_t<T>>;
  template<class T> using const_iterator_of_t = typename iterator_of<const std::remove_reference_t<T>>::type;

  template<IterableType T> using BeginType = decltype(std::begin(std::declval<T>()));
  template<IterableType T> using EndType = decltype(std::end(std::declval<T>()));
  // a concept for iterable types in which begin() and end() have the same type (this is apparently needed for some STL stuff like std::vector::insert)
  template<class T>
  concept IterableTypeWithSameIterators = IterableType<T> && std::is_same_v<BeginType<T>, EndType<T>>;

  // NOTE: 
  // we do not need to store the end-iterator if the iterator type has "bool is_valid() const"
  // (for example, the _auto_iter itself -- imagine an _auto_iter of _auto_iters)
  template<class Iter>
  concept iter_verifyable = requires(const Iter i) {
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


  template<class Iter, class C>
  concept StrictIteratorTypeOf = ContainerType<C> && !ContainerType<Iter> &&
                                  std::is_same_v<typename std::iterator_traits<Iter>::value_type, C::value_type>;
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
	concept MapType = requires(T a, typename std::remove_reference_t<T>::key_type key) {
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

  template<class T>
  concept TupleType = requires(T t) {
    typename std::tuple_size<T>::type;
    requires std::derived_from<std::tuple_size<T>, std::integral_constant<size_t, std::tuple_size_v<T>>>;
  };

}
