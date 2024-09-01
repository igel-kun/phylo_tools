
#pragma once

#include<cassert>
#include<climits>
#include<cstring>

#include<memory>
#include<sstream>
#include<deque>
#include<variant> // for cout << std::variant
#include<vector> // appending to vectors
#include<stack> // deal with container-adaptors not being iterable...
#include<type_traits> // deal with STL's missing type checks
#include<functional>
#include<algorithm> // deal with STL's sort problems

#if __clang__ && (CLANG_VERSION < 150000)
#   include<cstdlib> // for strtof
#else
#   include<charconv> // for from_chars
#endif


#include "hash_utils.hpp"
#include "stl_concepts.hpp"

namespace mstd{

  // --------------------- FUNDAMENTALS -------------------------------------
  template<int width> struct _fixed_width_uint {};
  template<> struct _fixed_width_uint<8> { using type = uint8_t; };
  template<> struct _fixed_width_uint<16> { using type = uint16_t; };
  template<> struct _fixed_width_uint<32> { using type = uint32_t; };
  template<> struct _fixed_width_uint<64> { using type = uint64_t; };
  template<int width> using fixed_width_uint = typename _fixed_width_uint<width>::type;

  template<int width>
  constexpr auto _fixed_width_float_helper() {
    if constexpr (width == sizeof(float)) return float{};
    if constexpr (width == sizeof(double)) return double{};
    if constexpr (width == sizeof(long double)) return static_cast<long double>(0);
    assert("no floating point of given width available on this platform" && false);
  }
  template<int width>
  using _fixed_width_float = decltype(_fixed_width_float_helper<width>());

  using floatptr_t = _fixed_width_float<sizeof(char*)>;

  // interpret pointer as fixed-width array
  template<size_t dim, class T>
  auto& cast_to_array(T* t) { return *static_cast<T(*)[dim]>(static_cast<void*>(t)); }

  template<class T> constexpr bool is_pair = false;
  template<class X, class Y> constexpr bool is_pair<std::pair<X,Y>> = true;

  // first index of a type in a variadic template, or number types if the type does not occur
  template<class T, class First, class... Others>
  constexpr size_t var_type_index() {
    if constexpr (std::is_same_v<T, First>) {
      return 0;
    } else if constexpr (sizeof...(Others) != 0) {
      return 1 + var_type_index<T, Others...>();
    } else return 1;
  }

  template<class T, class... Ts>
  constexpr bool is_in = (var_type_index<T, Ts...>() < sizeof...(Ts));

  template<class T, class... Ts, template<class...> class Var>
  constexpr bool occurs_in(const Var<Ts...>& v) { return is_in<T, Ts...>; }


  template<class T, class... Ts>
  auto& get_by_type(std::variant<Ts...>& v) { return std::get<var_type_index<T, Ts...>>(v); }
  template<class T, class... Ts>
  const auto& get_by_type(const std::variant<Ts...>& v) { return std::get<var_type_index<T, Ts...>>(v); }


  //NOTE: std::add_rvalue_reference<A&> = A& that's not very intuitive...
  template<class T>
  using make_rvalue_reference = std::add_rvalue_reference_t<std::remove_reference_t<T>>;

  // don't add const/volatile to void...
  template<class T> using add_const_t = std::conditional_t<std::is_void_v<T>, void, std::add_const_t<T>>;
  template<class T> using add_volatile_t = std::conditional_t<std::is_void_v<T>, void, std::add_volatile_t<T>>;
  template<class T> constexpr bool is_const_ref = std::is_const_v<std::remove_reference_t<T>>;

  // turn a reference into const reference or rvalue into const rvalue
  template<class T> struct _const_reference { using type = add_const_t<T>; };
  template<class T> struct _const_reference<T&> { using type = add_const_t<T>&; };
  template<class T> struct _const_reference<T&&> { using type = add_const_t<T>&&; };
  template<class T> using const_reference_t = typename _const_reference<T>::type;

  // std::conditional_t for unary templates
  template<bool condition, template<class> class X, template<class> class Y>
  struct conditional_template { template<class Z> using type = X<Z>; };
  template<template<class> class X, template<class> class Y>
  struct conditional_template<false, X, Y> { template<class Z> using type = Y<Z>; };

  // get the first type in a parameter pack, or void if pack size is 0
  template<class... Args>
  struct _FirstTypeOf { using type = void; };
  template<class First, class... Args>
  struct _FirstTypeOf<First, Args...> { using type = First; };
  template<class... Args>
  using FirstTypeOf = typename _FirstTypeOf<Args...>::type;

  // ---------------- conditional invocation ---------------------------

#warning "TODO: unify this using proper type-/value- extraction from parameter packs"
  // apply a function or pass through first argument if Function cannot be invoked with first and other_args
  template<class Function, class FirstArg, class... OtherArgs>
  decltype(auto) apply_or_pass1(Function&& f, FirstArg&& first, OtherArgs&&... other_args) {
    if constexpr (std::invocable<Function, FirstArg, OtherArgs...>)
      return f(std::forward<FirstArg>(first), std::forward<OtherArgs>(other_args)...);
    else return std::forward<FirstArg>(first);
  }
  // apply a function or pass through second argument if Function cannot be invoked with first and second and other_args
  template<class Function, class FirstArg, class SecondArg, class... OtherArgs>
  decltype(auto) apply_or_pass2(Function&& f, FirstArg&& first, SecondArg&& second, OtherArgs&&... other_args) {
    if constexpr (std::invocable<Function, FirstArg, SecondArg, OtherArgs...>)
      return f(std::forward<FirstArg>(first), std::forward<SecondArg>(second), std::forward<OtherArgs>(other_args)...);
    else return std::forward<SecondArg>(second);
  }

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


  // ---------------- reference_of and value_type_of -----------------
  template<class T> concept HasDeref = requires (T t) { *t; };
  template<class T> concept HasBegin = requires (T t) { t.begin(); };

  template<class T> struct reference_of {};
  template<class T> requires has_reference<T>
  struct reference_of<T> {
    using BareT = std::remove_reference_t<T>;
    // some containers do not allow modifying their contents via iterators (such as std::unordered_set)
    // Thus, we copy constness of the iterator's reference onto the container to copy it onto the reference later on
    using Traits = std::iterator_traits<iterator_of_t<BareT>>;
    using TraitsConstness = std::remove_reference_t<typename Traits::reference>;
    using correctT = copy_cv_t<TraitsConstness, BareT>;
    // copy constness of the container onto the reference
    using Ref = typename BareT::reference;
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

  template<class T> using reference_of_t  = typename reference_of<T>::type;
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


  // ------------------ ITERATORS -----------------------------------

  // for reasons that escape me, std::iterator_traits depend on satisfaction of the followign concepts, but it's not defined by the STL...
  // however it is indispensible for debugging to know why std::iterator_traits will not work for a self-defined iterator...
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

  // a lightweight end-iterator dummy that can be returned by calls to end() and compared to by other iterators
  template<class> class _GenericEndIterator;

  template<>
  struct _GenericEndIterator<void> {
    static bool is_valid() { return false; }
    bool operator==(const _GenericEndIterator&) const { return true; }
    template<class Other>
    bool operator==(const Other& x) const { return (x.operator==(*this)); }
  };

  template<class Sentinel>
  struct _GenericEndIterator: public _GenericEndIterator<void> {
    Sentinel s;
    template<class Other>
    bool operator==(const Other* x) const { assert(x != nullptr); return *x == s; }
  };
  template<class T>
  using GenericEndIteratorS = _GenericEndIterator<T>;
  using GenericEndIterator = _GenericEndIterator<void>;

  template<iter_verifyable Iter, class T>
  bool operator==(const Iter& other, const GenericEndIteratorS<T>&) { return !other.is_valid(); }

  // wrap a pointer in an iterator shell that has all the required types and can be inherited from
  template<class Ptr>
  struct PointerIterWrapper: public std::iterator_traits<Ptr> {
    using Tptr = Ptr;
    using TptrRef = Tptr&;
    using TptrConstRef = const Tptr&;
    using T = decltype(*Ptr());
    using Traits = std::iterator_traits<Ptr>;
    using typename Traits::difference_type;

    Tptr data = nullptr;

    PointerIterWrapper() = default;
    PointerIterWrapper(const Tptr x): data(x) {}

    auto& operator++() { ++data; return *this; }
    auto operator++(int) { PointerIterWrapper result{data}; ++data; return result; }
    auto& operator--() { --data; return *this; }
    auto operator--(int) { PointerIterWrapper result{data}; --data; return result; }
    auto& operator+=(const int x) { data += x; return *this; }
    auto operator+(const difference_type x) const { return PointerIterWrapper{data + x}; }
    auto& operator-=(const int x) { data -= x; return *this; }
    auto operator-(const int x) const { return PointerIterWrapper{data - x}; }
    difference_type operator-(const PointerIterWrapper& other) const { return PointerIterWrapper{data - other.data}; }

    auto& operator[](const int x) const { return data[x]; }

    bool operator==(const PointerIterWrapper& other) const { return other.data == data; }
    bool operator==(const Tptr other) const { return other == data; }

    bool operator<=>(const PointerIterWrapper& other) const { return other.data <=> data; }
    bool operator<=>(const Tptr other) const { return other <=> data; }
    
    T& operator*() const { return *data; }
    Tptr operator->() const { return data; }
    
    explicit operator TptrRef() { return data; }
    explicit operator TptrConstRef() const { return data; }
  };
  template<class T>
  PointerIterWrapper<T> operator+(const long int x, const PointerIterWrapper<T>& it) { return it + x; }

  using _Out = PointerIterWrapper<int*>;
  static_assert(std::weakly_incrementable<_Out>);
  static_assert(std::random_access_iterator<_Out>);

  template<class T>
  using InheritableIter = std::conditional_t<std::is_pointer_v<T>, PointerIterWrapper<T>, T>;

  template<class Iterator>
  using CorrespondingEndIter = std::conditional_t<iter_verifyable<Iterator>, void, Iterator>;


  // convenience class for dereference
  // NOTE: for some oscure reason, lambda's do not return 'decltype(auto)' by default, but only 'auto'
  //      so, if you want your lambda to return by reference (which is basically _ALWAYS_ what you want), then you'd need to explicitly tell it so
  //      this deref-class here exists so that I don't accidentally forget that...
  struct default_deref {
    template<class T> requires mstd::HasDeref<T>
    decltype(auto) operator()(T&& t) const { return *t; }
  };

  // a class that returns itself on dereference 
  // useful for iterators returning rvalues instead of lvalue references
  template<class T>
  struct self_deref {
    T t;
    template<class... Args>
    self_deref(Args&&... args): t(std::forward<Args>(args)...) {}
    T& operator*() { return t; }
    const T& operator*() const { return t; }
    T* operator->() { return &t; }
    const T* operator->() const { return &t; }
  };
  template<class R> // if the given reference is not a reference but an rvalue, then a pointer to it is modeled via self_deref
  using pointer_from_reference = std::conditional_t<std::is_reference_v<R>, std::add_pointer_t<std::remove_reference_t<R>>, self_deref<R>>;

  // if you want to build a class with some template T in it, but T is a reference, the class will not be assignable; thus it is preferred to use pointers
  template<class R>
  using prefer_pointer = std::conditional_t<std::is_reference_v<R>, std::add_pointer_t<std::remove_reference_t<R>>, R>;

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

  template<MapType M> using key_type_of_t = typename std::remove_reference_t<M>::key_type;
  template<MapType M> using mapped_type_of_t = typename std::remove_reference_t<M>::mapped_type;
  
  template<class T> struct mapped_or_value_type_of { using type = value_type_of_t<T>; };
  template<MapType M> struct mapped_or_value_type_of<M> { using type = mapped_type_of_t<M>; };
  template<class T>
  using mapped_or_value_type_of_t = typename mapped_or_value_type_of<T>::type;


  template<class T, class... Qs> struct _invoke_or_lookup_result { };
  template<MapType T, class... Qs> struct _invoke_or_lookup_result<T, Qs...> { using type = mstd::mapped_type_of_t<T>; };
  template<class T, class... Qs> requires (std::is_invocable_v<T, Qs...>)
  struct _invoke_or_lookup_result<T, Qs...> { using type = std::invoke_result_t<T, Qs...>; };
  template<class T, class... Qs> using invoke_or_lookup_result = typename _invoke_or_lookup_result<T, Qs...>::type;

  /*
  template<class _Iterator>
  constexpr bool is_forward_iterator = std::is_same_v<typename iterator_traits<_Iterator>::iterator_category, std::forward_iterator_tag>;
  template<class _Iterator>
  constexpr bool is_bidirectional_iterator = std::is_same_v<typename iterator_traits<_Iterator>::iterator_category, std::bidirectional_iterator_tag>;
  template<class _Iterator>
  constexpr bool is_random_access_iterator = std::is_same_v<typename iterator_traits<_Iterator>::iterator_category, std::random_access_iterator_tag>;
  */

  // compare iterators with their reverse versions
  template<typename T>
  bool operator==(const T& i1, const std::reverse_iterator<T>& i2) {  return (next(i1) == i2.base()); }
  template<typename T>
  bool operator==(const std::reverse_iterator<T>& i2, const T& i1) {  return operator==(i1, i2); }

  // ----------------------- store references in classes without losing operator= ------------------------------
  // this replaces references with std::reference_wrappers
  template<class T>
  using NoRef = std::conditional_t<std::is_reference_v<T>, std::reference_wrapper<std::remove_reference_t<T>>, T>;

  // ----------------------- container to array (first 'elements' elements)  ----------------------------------
  template<size_t elements, ContainerType Container>
  auto to_array(const Container& c) {
    using Val = value_type_of_t<Container>;
    std::array<Val, elements> result;
    std::copy_n(std::begin(c), elements, result.begin());
    return result;
  }
  

  // ----------------------- lookup ----------------------------------

  template<class T> struct _findable_type { using type = value_type_of_t<T>; };
  template<MapType M> struct _findable_type<M> { using type = key_type_of_t<M>; };
  template<class T> using findable_type = typename _findable_type<std::remove_cvref_t<T>>::type;
  template<class T, class C> concept FindableType = requires(T t, findable_type<C> other) {
    { t == other } -> std::convertible_to<bool>;
    { t != other } -> std::convertible_to<bool>;
  };

  // a map lookup with default
  template<MapType Map, typename Key, typename Ref = mapped_type_of_t<Map>>
  Ref map_lookup(Map&& m, const Key& key, Ref&& default_val = Ref()) {
    const auto iter = m.find(key);
    return (iter == m.end()) ? default_val : iter->second;
  }

  template<size_t get_num>
  struct selector {
    template<class Tuple> auto& operator()(Tuple& p) { return std::get<get_num>(p); }
    template<class Tuple> auto& operator()(const Tuple& p) { return std::get<get_num>(p); }
  };


  // --------------------------- sort and merge -------------------------------------

  // facepalm-time: the STL can only sort 2 things: random-access containers & std::list, that's it. So this mergesort can sort with bidirectional iters
  // based on a post othx to @TemplateRex: https://stackoverflow.com/questions/24650626/how-to-implement-classic-sorting-algorithms-in-modern-c

  // advance iter num_steps steps and return the size of the sorted prefix (number of elements in order following (including) iter)
  //NOTE: is_sorted_until() *almost* does what we want, but not quite
  template<class FwdIt, class Compare = std::less<>>
  size_t sorted_prefix(FwdIt& iter, ssize_t num_steps, Compare cmp = Compare{})
  {
    if(!num_steps) return 0;
    size_t result = 1; // 1 element is always sorted
    FwdIt last = iter;
    for(++iter, --num_steps; num_steps; ++iter, --num_steps) {
      if(std::is_sorted(last, iter, cmp)){
        ++result;
        last = iter;
      } else break;
    }
    while(num_steps--) ++iter;
    return result;
  }

  template<class FwdIt, class Compare = std::less<>>
  void inplace_merge_fwd(FwdIt first, FwdIt second, const FwdIt last, Compare cmp = Compare{})
  {
#warning write me
  }

  template<class FwdIt, class Compare = std::less<>>
  void merge_sort_fwd(FwdIt first, const FwdIt last, const typename iterator_traits<FwdIt>::difference_type N, Compare cmp = Compare{})
  {
    if (N <= 1) return;
    FwdIt middle = first;
    const size_t prefix = sorted_prefix(middle, N / 2);
    
    if(prefix < static_cast<size_t>(N/2)) merge_sort_fwd(first, middle, N/2, cmp);
    assert(std::is_sorted(first, middle, cmp));
    
    merge_sort_fwd(middle, last, N - N/2, cmp);
    assert(std::is_sorted(middle, last, cmp));
    
    if(std::forward_iterator<FwdIt>){
      //inplace_merge_fwd(first, middle, last, cmp);
      assert(false && "not implemented");
    } else std::inplace_merge(first, middle, last, cmp);
    assert(std::is_sorted(first, last, cmp));
  }
  // N log N sort, no matter what kind of iterator we get...
  template<class Iter, class Compare = std::less<>>
  void flexible_sort(Iter first, const auto& last, Compare cmp = Compare{}) {
    if constexpr (std::random_access_iterator<Iter>)
      std::sort(first, last, cmp);
    else merge_sort_fwd(first, last, std::distance(first, last), cmp);
  }

}
namespace std {
  // ------------------------- HASHING -------------------------------------------
  template<typename T1, typename T2>
  struct hash<std::pair<T1, T2> >{
    size_t operator()(const std::pair<T1,T2>& p) const{
      const std::hash<T1> hasher1;
      const std::hash<T2> hasher2;
      return hash_combine(hasher1(p.first), hasher2(p.second));
    }
  };
  template<mstd::ContainerType C>
  struct hash<C>{
    size_t operator()(const C& c) const{
      const std::hash<std::remove_cvref_t<mstd::value_type_of_t<C>>> hasher;
      size_t result = 0;
      for(const auto& x: c) {
        if constexpr (mstd::UnorderedContainerType<C>)
          result = hash_combine_symmetric(result, hasher(x));
        else result = hash_combine(result, hasher(x));
      }
      return result;
    }
  };
  template<typename T>
  struct hash<std::reference_wrapper<T>>: public std::hash<T> {
    size_t operator()(const std::reference_wrapper<T>& p) const {
      return this->operator()(static_cast<const T&>(p));
    }
  };
}
namespace mstd {


  // ------------------------ FUNCTIONS -------------------------------------------
  // deferred function call for emplacements, thx @ Arthur O'Dwyer
  // emplace(f(x)) = construct + move (assuming f does copy elision)
  // emplace(deferred_call(f(x))) = (in-place) construct (assuming f does copy elision)
  template<class F>
  struct deferred_call_t {
    using T = std::invoke_result_t<F>;
    const F f;

    explicit deferred_call_t(F&& _f): f(std::forward<F>(_f)) {}
    operator T() { return f(); }
  };
  template<typename F>
  inline auto deferred_call(F&& f) { return deferred_call_t<F>(std::forward<F>(f)); }

//NOTE: GCCs optimizations will break the IdentityFunction for reasons beyond my understanding
#pragma GCC push_options
#pragma GCC optimize ("O2")
  // a functional that ignores everything (and hopefully gets optimized out)
  template<class ReturnType = void>
  struct IgnoreFunction {
    template<class... Args> IgnoreFunction(Args&&... args) {}

    template<class... Args>
    constexpr ReturnType operator()(Args&&... args) const { if constexpr (!std::is_void_v<ReturnType>) return ReturnType{}; };
  };
  // a functional that just returns its argument (and hopefully gets optimized out)
  template<class T = void>
  struct IdentityFunction {
    template<class Q> requires std::is_convertible_v<Q&&, T>
    constexpr T operator()(Q&& x) const { return x; };
  };
  template<>
  struct IdentityFunction<void> {
    template<class Arg>
    constexpr decltype(auto) operator()(Arg&& x) const { return std::forward<Arg>(x); };
  };
#pragma GCC pop_options


  // --------------------- MODIFIED DATA STRUCTURES ------------------------

  template<class T, ContainerType C = std::deque<T>>
  class iterable_stack: public std::stack<T, C> {
    using std::stack<T, C>::c;
  public:
    using iterator = typename C::iterator;
    using const_iterator = typename C::const_iterator;

    iterator begin() { return c.begin(); }
    iterator end() { return c.end(); }
    const_iterator begin() const { return c.begin(); }
    const_iterator end() const { return c.end(); }
  };


  // a deleter that will or will not delete, depeding on its argument upon construction (for shared_ptr's)
  template<class T>
  struct SelectiveDeleter {
    const bool del;
    SelectiveDeleter(const bool _del): del(_del) {}
    inline void operator()(T* p) const { if(del) delete p; }
  };
  using NoDeleter = IgnoreFunction<>;


  //! decrease a value in a map, pointed to by an iterator; return true if the value was decreased and false if the item was removed
  template<MapType Map, long threshold = 1>
  inline bool decrease_or_remove(Map& m, const iterator_of_t<Map>& it) {
    if(it->second == threshold) {
      m.erase(it);
      return false;
    } else {
      --(it->second);
      return true;
    }
  }


  template<VectorType V>
  void vector_shrink_to_size(V&& vec, const size_t new_size) {
    vec.erase(vec.begin() + new_size, vec.end());
  }

  // --------------------- PAIR OPERATIONS -------------------------------

  //! add two pairs of things
  template <typename A, typename B>
  std::pair<A,B> operator+(const std::pair<A,B>& l, const std::pair<A,B>& r)
  {
    return {l.first + r.first, l.second + r.second};
  }

  //! reverse a pair of things, that is, turn (x,y) into (y,x)
  template<typename A, typename B>
  inline std::pair<B,A> reverse(const std::pair<A,B>& p) { return {p.second, p.first}; }

  // read tuples from Stringlike
  template<size_t index, class... Ts>
  void read_tuple(const std::string_view s, std::tuple<Ts...>& t, const char delim = ' ') {
    if constexpr (index < std::tuple_size_v<std::tuple<Ts...>>) {
      if(!s.empty()) {
        const size_t pos = s.find(delim);
        std::string_view tmp{s.substr(0, pos)};
        std::istringstream{std::string{tmp}} >> std::get<index>(t);
#warning "TODO: in C++23, use ispanstream, which can be constructed with a string_view"
        if(pos != std::string::npos)
          read_tuple<index + 1>(s.substr(pos + 1), t, delim );
      }
    }
  }
  template<class... Ts>
  auto read_tuple(const std::string_view s, const char delim = ' ') {
    std::tuple<Ts...> t;
    read_tuple<0, Ts...>(s, t, delim);
    return t;
  }

}

namespace std {
  // ----------------------- OUTPUT ---------------------------------------
  template<class First, class... Ts>
  std::ostream& operator<<(std::ostream& os, const std::variant<First, Ts...>& var) {
    std::visit([&os](const auto& v) { os << v; }, var);
    return os;
  }

  template<mstd::IterableType C> requires (!mstd::is_stringlike_v<C>)
  std::ostream& _print_iterable(std::ostream& os, const C& objs, const char delim = ' ', const char print_empty = true) {
    if(print_empty || (begin(objs) != end(objs))) {
      os << '[';
      bool first = true;
      for(const auto& obj : objs) {
        if(first) first = false; else os << delim;
        using Item = std::remove_cvref_t<decltype(obj)>;
        if constexpr (mstd::PointerType<Item>) {
          os << hex << obj;
        } else if constexpr (mstd::ArithmeticType<Item>) {
          os << +obj;
        } else os << obj;
      }
      return os << ']';
    } else return os;
  }

  template<mstd::IterableType C> requires (!mstd::is_stringlike_v<C>)
  inline std::ostream& operator<<(std::ostream& os, const C& objs) {
    return _print_iterable(os, objs, ' ');
  }

  template<mstd::IterableType C> requires (!mstd::is_stringlike_v<C>)
  struct Linewise: public C {
    char delimeter = '\n';
    bool print_empty = true;
    Linewise(const C& c, const bool _print_empty = true, const char _delimeter = '\n'):
      C(c), delimeter{_delimeter}, print_empty{_print_empty} {}

    friend ostream& operator<<(ostream& os, const Linewise& L) {
      return _print_iterable(os, static_cast<const C&>(L), L.delimeter, L.print_empty);
    }
  };

  template<class T> std::string to_string(const T& x) { std::ostringstream out; out << x; return std::move(out).str(); }
  template<class T> long to_int(const T& x) { return strtol(to_string(x), nullptr, 10); }

  template <typename A, typename B>
  std::ostream& operator<<(std::ostream& os, const std::pair<A,B>& p) { return os << '('<<p.first<<','<<p.second<<')'; }
  template <typename A>
  std::ostream& operator<<(std::ostream& os, const std::reference_wrapper<A>& r) { return os << r.get(); }


  // --------------------- STRINGS & STRING_VIEW ------------------------------
  size_t length(const std::string& s) { return s.size(); }
  size_t length(const std::string_view& s) { return s.size(); }
  size_t length(const char* s) { return std::strlen(s); }
  size_t length(const char s) { return 1; }

  template<mstd::StringlikeOrChar STR1, mstd::StringlikeOrChar STR2>
  std::string operator+(const STR1& s1, const STR2& s2) {
    std::string result;
    result.reserve(length(s1) + length(s2) + 1);
    if constexpr (sizeof(STR1) == 1)
      result.push_back(s1);
    else result.append(s1);
    if constexpr (sizeof(STR2) == 1)
      result.push_back(s2);
    else result.append(s2);
    return result;
  }


#if __APPLE__ || (__clang__ && (CLANG_VERSION < 130000))
  // clang before version 13 doesn't have from_chars
  // also, apple is, shall we say, less than optimal

  // note: a string_view is not guaranteed to be zero-terminated and, if it's not, we _have_to_ copy it :(
  template<class Converter>
  auto _stoX(const std::string_view s, size_t& first_unconverted, Converter&& convert = Converter()) {
    const char* const c_str = s.data();
    if(c_str[s.size()] != 0) {
      const std::string my_s(s);
      return convert(my_s.c_str(), &first_unconverted);
    } else return convert(c_str, &first_unconverted);
  }
  template<mstd::ArithmeticType T>
  T stoX(const std::string_view s, size_t& first_unconverted) {
    if constexpr (std::is_integral_v<T>) {
      if constexpr (is_signed_v<T>) {
        return static_cast<T>(_stoX(s, first_unconverted, std::stoll));
      } else return static_cast<T>(_stoX(s, first_unconverted, std::stoull));
    } else if constexpr (sizeof(T) == sizeof(float)) {
      return static_cast<T>(_stoX(s, first_unconverted, std::stof));
    } else if constexpr (sizeof(T) == sizeof(double)) {
      return static_cast<T>(_stoX(s, first_unconverted, std::stod));
    } else return static_cast<T>(_stoX(s, first_unconverted, std::stold));
  }

#else
  // std::string_view conversion
  template<mstd::ArithmeticType T>
  T stoX(const std::string_view sv, size_t& first_unconverted) {
    T result;
    first_unconverted = std::from_chars(sv.data(), sv.data() + sv.size(), result).ptr - sv.data();
    return result;
  }

  //int    stoi(const std::string_view sv) { int result = 0; std::from_chars(sv.data(), sv.data() + sv.size(), result); return result; }
  //long   stol(const std::string_view sv) { long result = 0; std::from_chars(sv.data(), sv.data() + sv.size(), result); return result; }
  //float  stof(const std::string_view sv) { float result = 0.0; std::from_chars(sv.data(), sv.data() + sv.size(), result); return result; }
  //double stod(const std::string_view sv) { double result = 0.0; std::from_chars(sv.data(), sv.data() + sv.size(), result); return result; }
#endif

  template<mstd::ArithmeticType T>
  T stoX(const std::string_view s) {
    size_t ignore_me;
    return stoX<T>(s, ignore_me);
  }

  int    stoi(const std::string_view s) { return stoX<int>(s); }
  long   stol(const std::string_view s) { return stoX<long>(s); }
  float  stof(const std::string_view s) { return stoX<float>(s); }
  double stod(const std::string_view s) { return stoX<double>(s); }

}

namespace mstd {
  // --------------------- splitting prefixes off string_views ------------------------------
  
  auto split_prefix_from(std::string_view& s, const size_t pos) {
    std::string_view result = s.substr(0, pos);
    s.remove_prefix(pos + 1);
    return result;
  }
  bool split_prefix_at_next(std::string_view& s, auto& prefix, const auto& delims, bool invert = false) {
    const size_t pos = invert ? s.find_first_not_of(delims) : s.find_first_of(delims);
    if(pos != std::string::npos) {
      prefix = split_prefix_from(s, pos);
      return true;
    } else return false;
  }

  // -------------------- ranges --------------------------------
  // this is the biggest sillyness yet: a c++-ranges const filtered-view cannot be iterated-over... why? why? WHY!!!!
  template<class T> requires (!std::ranges::view<T>)
  decltype(auto) make_usable(T&& t) { return std::forward<T>(t); }
  template<class T> requires (std::ranges::view<T>)
  decltype(auto) make_usable(const T& t) { return const_cast<T&>(t); }


  // --------------------- MISC ---------------------------------------------
  template<class T, class Q>
  concept CompatibleValueTypes = std::is_same_v<value_type_of_t<T>, value_type_of_t<Q>>;
  template<class T, class Q>
  concept ConvertibleValueTypes = std::convertible_to<value_type_of_t<Q>, value_type_of_t<T>>;

  // Often times I want the template parameter to decide whether an object owns something or not.
  // In previous versions, I just passed either a type (like 'NodeTranslation') or a reference ('NodeTranslation&') to distinguish the two.
  // However, this may give problems with costness, so we might actually want to use owning-/non-owning pointers instead!
  // Then, however, we get heap-allocations :/
  template<class T>
  using AutoOwningPtr = std::conditional_t<std::is_reference_v<T>,
                                            std::add_pointer_t<std::remove_reference_t<T>>,
                                            std::unique_ptr<std::remove_reference_t<T>>>;



  // functions returning void are treated differently from functions returning anything, even if that anything is then ignored; we unify the two here
  template<class P, template<class> class Q> struct _UnlessVoid { using type = Q<P>; };
  template<template<class> class Q> struct _UnlessVoid<void, Q> { using type = void; };
  template<class P, template<class> class Q> using UnlessVoid = typename _UnlessVoid<P,Q>::type;

  template<class... T> struct _FirstNonVoid {};
  template<class T, class... Else> requires (!std::is_void_v<T>) struct _FirstNonVoid<T, Else...> { using type = T; };
  template<class T, class... Else> requires (std::is_void_v<T>) struct _FirstNonVoid<T, Else...>: public _FirstNonVoid<Else...> {};
  template<class... T> using FirstNonVoid = typename _FirstNonVoid<T...>::type;


  template<class T, class Else = uint_fast8_t> using ReturnableType = FirstNonVoid<T, Else>;

  // an operator that appends anything to a given container
  template<ContainerType C>
  struct appender {
    C& target;
    appender(C& _target): target(_target) {}

    template<class... Args>
    void operator()(Args&&... args) { append(target, std::forward<Args>(args)...); }
  };

  // an operator that stores something and returns it every time it is called
  template<class T>
  struct dispenser {
    T data;
    template<class... Args> dispenser(Args&&... args): data(std::forward<Args>(args)...) {}
    template<class... Args> T& operator()(Args&&... args) { return data; }
    template<class... Args> const T& operator()(Args&&... args) const { return data; }
  };



  template<class T = int>
  struct minus_one { static constexpr T value = -1; };

  // specialize to your hearts desire
  template<class T> struct default_invalid {
    using type = std::conditional_t<is_basically_arithmetic_v<T>, minus_one<T>, void>;
  };
  template<class T> using default_invalid_t = typename default_invalid<T>::type;



  // -------------------- variants --------------------------------

  template<class... Args>
  std::ostream& operator<<(std::ostream& os, const std::variant<Args...>& var) {
    return os << std::visit([](const auto& x) { std::cout << x; }, var);
  }
}



