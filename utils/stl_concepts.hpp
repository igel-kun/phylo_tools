
#include <vector>
#include "utils.hpp"

namespace std {

  // NOTE: my concepts do not differentiate between T and T&. For example ContainerType<T> is true for T = vector<int>&. This is so you can write "ContainerType C" and use C as universal reference

  // std::is_arithmetic is false for pointers.... why?
  template<class T> constexpr bool is_really_arithmetic_v = is_arithmetic_v<T> || is_pointer_v<T>;
  // anything that can be converted from and to int is considered "basically arithmetic"
  template<class T> constexpr bool is_basically_arithmetic_v = is_convertible_v<int, remove_cvref_t<T>> && is_convertible_v<remove_cvref_t<T>, int>;


  // ever needed to get an interator if T was non-const and a const_iterator if T was const? Try this:
  template<class T> struct iterator_of {
    using type = conditional_t<is_const_v<T>, typename T::const_iterator, typename T::iterator>;
  };
  template<class T> struct iterator_of<T*> { using type = T*; };
  template<class T, std::size_t N> struct iterator_of<T (&)[N]> { using type = T*; };
  template<class T> using iterator_of_t = typename iterator_of<remove_reference_t<T>>::type;
  template<class T> using reverse_iterator_of_t = reverse_iterator<iterator_of_t<T>>;

  template<class T> concept ArithmeticType =  is_really_arithmetic_v<T>;
  template<class T> concept PointerType = is_pointer_v<remove_cvref_t<T>>;

  template<class T>
  concept VectorType = is_convertible_v<remove_cvref_t<T>, vector<typename T::value_type, typename T::allocator_type>>;

  template <class T> 
  concept IterableType = requires(remove_cvref_t<T> a, const T& b) {
    // NOTE: it seems forward_iterator concept cannot be satisfied by the proxy iterator of raw_vector_map :/
    //requires forward_iterator<typename T::iterator>;
    //requires forward_iterator<typename T::const_iterator>;
    { a.begin() }   -> convertible_to<iterator_of_t<T>>;
    { a.end() }     -> convertible_to<iterator_of_t<T>>;
    { b.begin() }   -> convertible_to<iterator_of_t<const T>>;
    { b.end() }     -> convertible_to<iterator_of_t<const T>>;
	};
  //template<class T>
  //concept IterableType = StrictIterableType<remove_cvref_t<T>>;
  template<class T>
  concept OptionalIterableType = is_void_v<T> || IterableType<T>;

  // concept checking for STL-style container (thanks to https://stackoverflow.com/questions/60449592 )
  template <class T> 
  concept ContainerType = requires(T a) {
    requires regular<remove_cvref_t<T>>;
    requires swappable<remove_cvref_t<T>>;
		requires IterableType<remove_cvref_t<T>>;
    requires destructible<typename remove_cvref_t<T>::value_type>;
    //requires same_as<typename remove_cvref_t<T>::reference, typename remove_cvref_t<T>::value_type &>;
    //requires same_as<typename remove_cvref_t<T>::const_reference, const typename remove_cvref_t<T>::value_type &>;
    requires signed_integral<typename remove_cvref_t<T>::difference_type>;
    requires same_as<typename remove_cvref_t<T>::difference_type, typename iterator_traits<typename remove_cvref_t<T>::iterator>::difference_type>;
    requires same_as<typename remove_cvref_t<T>::difference_type, typename iterator_traits<typename remove_cvref_t<T>::const_iterator>::difference_type>;
    { a.size() }    -> same_as<typename remove_cvref_t<T>::size_type>;
    { a.empty() }   -> same_as<bool>;
  };
  // for some reason, the idea to have a StrictContainerType + a ContainerType like this doesn't work (GCC wrongly claims "concept depends on itself")
  //template<class T>
  //concept ContainerType = StrictContainerType<remove_cvref_t<T>>; 
  
  template<class T>
  concept OptionalContainerType = is_void_v<T> || ContainerType<T>;

	// a set is a container that supports count()
	template<class T>
	concept SetType = requires(T a, typename remove_cvref_t<T>::value_type v) {
		requires ContainerType<T>;
		{ a.count(v) } -> same_as<size_t>;
	};
  //template<class T>
  //concept SetType = StrictSetType<remove_cvref_t<T>>;
  template<class T>
  concept OptionalSetType = is_void_v<T> || SetType<T>;


  // a map is something mapping key_type to value_type with operator[]
	template<class T>
	concept MapType = requires(T a, typename remove_cvref_t<T>::key_type key) {
		requires ContainerType<T>;
		{ a[key] } -> same_as<typename T::mapped_type&>;
	};
  //template<class T>
  //concept MapType = StrictMapType<remove_cvref_t<T>>;
  template<class T>
  concept OptionalMapType = is_void_v<T> || MapType<T>;

  // a queue is something that has push and pop
	template<class T>
	concept QueueType = requires(T a, typename remove_cvref_t<T>::value_type v) {
		a.push(v);
		a.pop();
	};
  //template<class T>
  //concept QueueType = StrictQueueType<remove_cvref_t<T>>;
  template<class T>
  concept OptionalQueueType = is_void_v<T> || QueueType<T>;

  template<typename T>
  concept Printable = requires(T t) {
      { std::cout << t } -> std::same_as<std::ostream&>;
  };

}
