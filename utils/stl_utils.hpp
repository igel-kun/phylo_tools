
#pragma once

#include<cassert>
#include<cstring>

#include<limits>
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

#include "config.hpp"
#include "hash_utils.hpp"
#include "stl_concepts.hpp"

namespace mstd{

  // --------------------- FUNDAMENTALS -------------------------------------
  struct monostate {};

  template<int width> struct _fixed_width_uint {};
  template<> struct _fixed_width_uint<8> { using type = uint8_t; };
  template<> struct _fixed_width_uint<16> { using type = uint16_t; };
  template<> struct _fixed_width_uint<32> { using type = uint32_t; };
  template<> struct _fixed_width_uint<64> { using type = uint64_t; };
  template<int width> using fixed_width_uint = typename _fixed_width_uint<width>::type;

  template<int width>
  constexpr auto _fixed_width_float_helper() {
    if constexpr (width == sizeof(float)) {
      return float{};
    } else if constexpr (width == sizeof(double)) {
      return double{};
    } else if constexpr (width == sizeof(long double)) {
      return static_cast<long double>(0);
    }
    assert("no floating point of given width available on this platform" && false);
  }
  template<int width>
  using _fixed_width_float = decltype(_fixed_width_float_helper<width>());

  using floatptr_t = _fixed_width_float<sizeof(char*)>;

  // interpret pointer as fixed-width array
  template<size_t dim, class T>
  auto& cast_to_array(T* t) { return *static_cast<T(*)[dim]>(static_cast<void*>(t)); }

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



  // ---------------- Tuples and Variadic Templates -------------------

  // Nth type in a list of types
  template <size_t Index, class... Ts> requires (Index < sizeof...(Ts))
  using NthType = std::tuple_element_t<Index, std::tuple<Ts...>>;

  // get the first type in a parameter pack, or void if pack size is 0
  template<class... Args>
  using FirstTypeOf = NthType<0, Args...>;

  // first index of a type in a variadic template, or number types if the type does not occur
  template<class T, class First, class... Others>
  constexpr size_t var_type_index() {
    if constexpr (std::is_same_v<T, First>) {
      return 0;
    } else if constexpr (sizeof...(Others) != 0) {
      return 1 + var_type_index<T, Others...>();
    } else return 1;
  }
  template<class T>
  constexpr size_t var_type_index() { return 1; }

  // convenience function to deduce the Ts
  template<class T, class... Ts, template<class...> class Var>
  constexpr bool occurs_in(const Var<Ts...>& v) { return is_any_of<T, Ts...>; }

  template<class T, class... Ts>
  auto& get_by_type(std::variant<Ts...>& v) { return std::get<var_type_index<T, Ts...>>(v); }
  template<class T, class... Ts>
  const auto& get_by_type(const std::variant<Ts...>& v) { return std::get<var_type_index<T, Ts...>>(v); }


  // apply a function to all items of a tuple and return the tuple of results
  template<typename Tuple, typename F, std::size_t... Is>
  constexpr auto tuple_transform_impl(Tuple&& tup, F&& f, std::index_sequence<Is...>) {
    return std::make_tuple(f(std::get<Is>(std::forward<Tuple>(tup)), Is)...);
  }

  template<typename Tuple, typename F>
  constexpr auto tuple_transform(Tuple&& tup, F&& f) {
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    return tuple_transform_impl(
      std::forward<Tuple>(tup),
      std::forward<F>(f),
      std::make_index_sequence<N>{}
    );
  }

  template<typename Tuple, typename F, std::size_t... Is>
  constexpr auto tuple_generate_impl(F&& f, std::index_sequence<Is...>) {
    return std::make_tuple(f.template operator()<std::tuple_element_t<Is, Tuple>>(Is)...);
  }

  template<typename Tuple, typename F>
  constexpr auto tuple_generate(F&& f) {
    constexpr size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    return tuple_generate_impl<Tuple>(
      std::forward<F>(f),
      std::make_index_sequence<N>{}
    );
  }



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



  // ------------------ ITERATORS -----------------------------------

  // a lightweight end-iterator dummy that can be returned by calls to end() and compared to by other iterators
  template<class> class _GenericEndIterator;

  template<>
  struct _GenericEndIterator<void> {
    static bool is_valid() { return false; }
    bool operator==(const _GenericEndIterator&) const { return true; }
    template<VerifyableIter Other> requires (not mstd::is_same_v<Other, _GenericEndIterator>)
    bool operator==(const Other& x) const { return not x.is_valid(); }
    template<class Other> requires (not VerifyableIter<Other> && not mstd::is_same_v<Other, _GenericEndIterator>)
    bool operator==(const Other& x) const { return x.operator==(*this); }
  };

  template<class Sentinel>
  struct _GenericEndIterator: public _GenericEndIterator<void> {
    Sentinel s;
    template<class Other>
    bool operator==(const Other* x) const { assert(x != nullptr); return *x == s; }
  };
  using GenericEndIterator = _GenericEndIterator<void>;

  template<VerifyableIter Iter, class T>
  bool operator!=(const Iter& other, const _GenericEndIterator<T>&) { return other.is_valid(); }

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

  static_assert(std::weakly_incrementable<PointerIterWrapper<int*>>);
  static_assert(std::random_access_iterator<PointerIterWrapper<int*>>);

  template<class T>
  using InheritableIter = std::conditional_t<std::is_pointer_v<T>, PointerIterWrapper<T>, T>;

  template<HasIterCategory Iterator>
  using CorrespondingEndIter = std::conditional_t<VerifyableIter<Iterator>, void, Iterator>;


  // convenience class for dereference
  // NOTE: for some oscure reason, lambdas do not return 'decltype(auto)' by default, but only 'auto'
  //      so, if you want your lambda to return by reference (which is basically _ALWAYS_ what you want), then you'd need to explicitly tell it so
  //      this deref-class here exists so that I don't accidentally forget that...
  struct default_deref {
    template<class T> 
    decltype(auto) operator()(T&& t) const {
      if constexpr (mstd::HasDeref<T>)
        return *(std::forward<T>(t));
      else return std::forward<T>(t); 
    }
  };


  // access a pointer or reference, returning it as reference
  template<class T>
  decltype(auto) access(T&& t) {
    if constexpr (HasDeref<T>)
      return *t;
    else return t;
  }

  // a function composition, allows using pointers to functions
  template<class F1, class F2>
  struct PointwiseCompose {
    [[no_unique_address]] F1 f1;
    [[no_unique_address]] F2 f2;

    template<class... Args>
    static constexpr bool F1invocable = (std::invocable<decltype(access(std::declval<F1&>())), Args> && ...);

    template<class... Args> requires F1invocable<Args...>
    decltype(auto) operator()(Args&&... args) {
      return access(f2)( access(f1)(std::forward<Args>(args))... );
    }
    template<class... Args> requires F1invocable<Args...>
    decltype(auto) operator()(Args&&... args) const {
      return access(f2)( access(f1)(std::forward<Args>(args))... );
    }

  };
  template<class F1, class F2>
  struct Compose {
    [[no_unique_address]] F1 f1;
    [[no_unique_address]] F2 f2;

    template<class... Args> requires std::invocable<decltype(access(f1)), Args...>
    decltype(auto) operator()(Args&&... args) { return access(f2)(access(f1)(std::forward<Args>(args)...)); }
  };


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
  // if you want to build a class with some templated member T, but T is instanciated as a reference,
  // then the class will not be assignable; thus it is preferred to use pointers
  template<class R>
  using NoRef = std::conditional_t<std::is_reference_v<R>, std::add_pointer_t<std::remove_reference_t<R>>, R>;


  // ----------------------- container to array (first 'elements' elements)  ----------------------------------
  template<size_t elements, ContainerType Container>
  auto to_array(const Container& c) {
    using Val = value_type_of_t<Container>;
    std::array<Val, elements> result;
    std::copy_n(std::begin(c), elements, result.begin());
    return result;
  }
  

  // ----------------------- lookup ----------------------------------
  // a map lookup with default
  template<MapType Map, typename Key, typename Ref = mapped_type_of_t<Map>>
  Ref map_lookup(Map&& m, const Key& key, Ref&& default_val = Ref()) {
    const auto iter = m.find(key);
    return (iter == m.end()) ? default_val : iter->second;
  }

  template<size_t get_num>
  struct selector {
    template<class Tuple> auto& operator()(Tuple& p) const { return std::get<get_num>(p); }
    template<class Tuple> auto& operator()(const Tuple& p) const { return std::get<get_num>(p); }
  };


  // --------------------------- sort and merge -------------------------------------

  // facepalm-time: the STL can only sort 2 things: random-access containers & std::list, that's it. So this mergesort can sort with bidirectional iters
  // based on a post of @TemplateRex: https://stackoverflow.com/questions/24650626/how-to-implement-classic-sorting-algorithms-in-modern-c

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
    exit(EXIT_FAILURE);
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

  // if you only have a '<'-comparison, but you need '==', then this class does it for you
  template<class Compare>
  struct EqualFromOrdering {
    [[ no_unique_address ]] Compare cmp;

    template<class T, class U>
    bool operator()(const T& a, const U& b) const { return (not cmp(a, b)) and  (not cmp(b, a)); }
    template<class T, class U>
    bool operator()(const T& a, const U& b) { return (not cmp(a, b)) and  (not cmp(b, a)); }
  };

  // in C++20, unordered_set::find() accepts values that can be compared to the keys, provided the comparator has a field called 'is_transparent'
  // why this would not just be standard is beyond me...
  // anyways, this comparator dereferences the arguments when they can be dereferenced, otherwise not, and then it compares the results
  struct DerefEqual {
    using is_transparent = void;

    template<class X, class Y>
    bool operator()(const X& x, const Y& y) const { return access(x) == access(y); }
  };
  // a hasher to hash various pointers (C-style pointers, smart pointers, ...)
  // can be used to index into a hash-table of pointers by different types of pointers
  struct PtrHash {
    using is_transparent = void;

    template<HasDeref T>
    auto operator()(const T& x) const { return std::hash<void*>{}(&(*x)); }
  };


  // ------------------------ FUNCTIONS -------------------------------------------
  // deferred function call for emplacements, thx @ Arthur O'Dwyer
  // emplace(f()) = construct + move (assuming f does copy elision)
  // emplace(deferred_call(f())) = (in-place) construct (assuming f does copy elision)
  template<class F>
  struct deferred_call_t {
    using T = std::invoke_result_t<F>;
    const F f;

    explicit deferred_call_t(F&& _f): f(std::forward<F>(_f)) {}
    operator T() { return f(); }
  };
  template<typename F>
  inline auto deferred_call(F&& f) { return deferred_call_t<F>(std::forward<F>(f)); }

  // a functional that ignores everything (and hopefully gets optimized out)
  template<class ReturnType = void>
  struct IgnoreFunction {
    template<class... Args>
    constexpr ReturnType operator()(Args&&... args) const { if constexpr (not std::is_void_v<ReturnType>) return ReturnType{}; };
  };
  template<auto RetVal = 0u>
  struct ConstFunction {
    template<class... Args>
    constexpr decltype(auto) operator()(Args&&... args) const { return RetVal; };
  };

  // a functional that just returns its argument (and hopefully gets optimized out)
  template<class T = void>
  struct IdentityFunction {
    template<class Q, class... Args> requires std::is_convertible_v<Q&&, T>
    constexpr T operator()(Q&& x, Args&&... args) const { return x; };
  };
  template<>
  struct IdentityFunction<void> {
    template<class Arg, class... Args>
    constexpr decltype(auto) operator()(Arg&& x, Args&&... args) const { return std::forward<Arg>(x); };
  };



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
        return static_cast<T>(_stoX(s, first_unconverted, [](auto&&... x) {return std::stoll(x...);} ));
      } else return static_cast<T>(_stoX(s, first_unconverted, [](auto&&... x){ return std::stoull(x...);}));
    } else if constexpr (sizeof(T) == sizeof(float)) {
      return static_cast<T>(_stoX(s, first_unconverted, [](auto&&... x){ return std::stof(x...);}));
    } else if constexpr (sizeof(T) == sizeof(double)) {
      return static_cast<T>(_stoX(s, first_unconverted, [](auto&&... x){ return std::stod(x...);}));
    } else return static_cast<T>(_stoX(s, first_unconverted, [](auto&&... x){ return std::stold(x...);}));
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

  template<class T> std::string to_string(const T& x) { std::ostringstream out; out << x; return std::move(out).str(); }
}

// ----------------------- OUTPUT ---------------------------------------
namespace mstd {

  template<class T>
  struct printable {
    const T* x;

    friend std::ostream& operator<<(std::ostream& os, const printable& p) {
      if constexpr (Printable<T>) {
        return os << *(p.x);
      } else return os << "((unprintable @"<<p.x<<"))";
    }
  };

  // a general parser from string_stream to anything
  template<class Default = size_t>
  struct AnythingFromString {
    struct viewbuf: std::streambuf {
      viewbuf(std::string_view sv) {
        char* p = const_cast<char*>(sv.data());
        this->setg(p, p, p + sv.size());
      }
    };

    AnythingFromString() = default;

    template<class T>
    static T from_string(std::string_view sv) {
      if constexpr (std::is_constructible_v<T, std::string_view>) {
        return T(sv);  // direct construction from string_view
      } else if constexpr (std::is_integral_v<T> or std::is_floating_point_v<T>) {
        return std::stoX<T>(sv);
      } else if constexpr (std::is_constructible_v<T, std::string>) {
        return T(std::string(sv));  // conversion with string copy
      } else if constexpr (std::is_constructible_v<T>) {
        viewbuf vb(sv);
        std::istream is(&vb);
        T value;
        is >> value;
        if(!is) throw std::runtime_error("parse error");
        return value;
      } else {
        static_assert([]{ return false; }(), "Don't know how to convert to T from string_view");
      }
    }

    template<class T = Default>
    T operator()(std::string_view sv) const { return from_string<T>(sv); }
  };

}
namespace std {
  template <typename A, typename B>
  std::ostream& operator<<(std::ostream& os, const std::pair<A,B>& p) { return os << '('<<p.first<<','<<p.second<<')'; }
  template <typename A>
  std::ostream& operator<<(std::ostream& os, const std::reference_wrapper<A>& r) { return os << r.get(); }

  template<class First, class... Ts>
  std::ostream& operator<<(std::ostream& os, const std::variant<First, Ts...>& var) {
    std::visit([&os](const auto& v) { os << v; }, var);
    return os;
  }

  template<mstd::IterableType C> requires (not mstd::Stringlike<C>)
  std::ostream& _print_iterable(std::ostream& os, C&& objs, const char delim = ' ') {
    auto _end = std::end(objs);
    auto _beg = std::begin(objs);

    if(mstd::config::print_empty_containers or (_beg != _end)) {
      os << '[';
      bool first = true;
      while(_beg != _end) {
        if(first) first = false; else os << delim;
        decltype(auto) obj = *_beg;
        using Item = std::remove_cvref_t<decltype(obj)>;
        if constexpr (mstd::PointerType<Item>) {
          os << hex << obj;
        } else if constexpr (mstd::ArithmeticType<Item>) {
          os << +obj;
        } else os << mstd::printable{&obj};
        ++_beg;
      }
      return os << ']';
    } else return os;
  }

  template<mstd::IterableType C> requires (not mstd::Stringlike<C>)
  inline std::ostream& operator<<(std::ostream& os, C&& objs) {
    return _print_iterable(os, std::forward<C>(objs), ' ');
  }
}

namespace mstd {
  template<mstd::IterableType C> requires (not mstd::Stringlike<C>)
  struct Linewise {
    C c;
    char delimeter = '\n';
    bool print_empty = true;
    Linewise(const C& _c, const bool _print_empty = true, const char _delimeter = '\n'):
      c{_c}, delimeter{_delimeter}, print_empty{_print_empty} {}

    template<class LW> requires (mstd::is_same_v<LW, Linewise>)
    friend std::ostream& operator<<(std::ostream& os, LW&& L) {
      std::swap(L.print_empty, config::print_empty_containers);
      auto& result = _print_iterable(os, std::forward<LW>(L).c, L.delimeter);
      std::swap(L.print_empty, config::print_empty_containers);
      return result;
    }
  };


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

 

  // -------------------- variants --------------------------------

  template<class... Args>
  std::ostream& operator<<(std::ostream& os, const std::variant<Args...>& var) {
    return os << std::visit([](const auto& x) { std::cout << x; }, var);
  }


}



