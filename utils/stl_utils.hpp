
#pragma once

#include<cassert>
#include<climits>
#include<sstream>
#include<deque>
#include<vector> // appending to vectors
#include<stack> // deal with container-adaptors not being iterable...
#include<type_traits> // deal with STL's missing type checks
#include<algorithm> // deal with STL's sort problems
#include<functional>
#include<charconv>

#include "hash_utils.hpp"
#include "stl_concepts.hpp"

namespace std{

  // --------------------- FUNDAMENTALS -------------------------------------

  // interpret pointer as fixed-width array
  template<size_t dim, class T>
  auto& cast_to_array(T* t) { return *static_cast<T(*)[dim]>(static_cast<void*>(t)); }

  template<class T> constexpr bool is_pair = false;
  template<class X, class Y> constexpr bool is_pair<std::pair<X,Y>> = true;

  //NOTE: add_rvalue_reference<A&> = A& that's not very intuitive...
  template<class T>
  using make_rvalue_reference = add_rvalue_reference_t<std::remove_reference_t<T>>;

  // don't add const/volatile to void...
  template<class T> using my_add_const_t = conditional_t<is_void_v<T>, void, add_const_t<T>>;
  template<class T> using my_add_volatile_t = conditional_t<is_void_v<T>, void, add_volatile_t<T>>;
  template<class T> constexpr bool is_const_ref = is_const_v<remove_reference_t<T>>;

  // turn a reference into const reference or rvalue into const rvalue
  template<class T> struct _const_reference { using type = my_add_const_t<T>; };
  template<class T> struct _const_reference<T&> { using type = my_add_const_t<T>&; };
  template<class T> struct _const_reference<T&&> { using type = my_add_const_t<T>&&; };
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
  using FirstTypeOf = _FirstTypeOf<Args...>::type;

  // ---------------- conditional invocation ---------------------------

  // if F is invocable with Args... yield the result of this invocation, otherwise yield Else
  template<class F, class Else, class... Args> struct _invoke_result { using type = Else; };
  template<class F, class Else, class... Args> requires std::invocable<F, Args...>
  struct _invoke_result<F, Else, Args...> { using type = std::invoke_result_t<F, Args...>; };
  template<class F, class Else, class... Args> 
  using my_invoke_result = typename _invoke_result<F, Else, Args...>::type;

#warning "TODO: unify this using proper type-/value- extraction from parameter packs"
  // apply a function or pass through first argument if Function cannot be invoked with first and other_args
  template<class Function, class FirstArg, class... OtherArgs>
  my_invoke_result<Function, FirstArg, FirstArg, OtherArgs...>
    apply_or_pass1(Function&& f, FirstArg&& first, OtherArgs&&... other_args) {
      if constexpr (std::invocable<Function, FirstArg, OtherArgs...>)
        return f(std::forward<FirstArg>(first), std::forward<OtherArgs>(other_args)...);
      else return first;
  }
  // apply a function or pass through second argument if Function cannot be invoked with first and second and other_args
  template<class Function, class FirstArg, class SecondArg, class... OtherArgs>
  my_invoke_result<Function, SecondArg, FirstArg, SecondArg, OtherArgs...>
    apply_or_pass2(Function&& f, FirstArg&& first, SecondArg&& second, OtherArgs&&... other_args) {
      if constexpr (std::invocable<Function, FirstArg, SecondArg, OtherArgs...>)
        return f(std::forward<FirstArg>(first), std::forward<SecondArg>(second), std::forward<OtherArgs>(other_args)...);
      else return second;
  }

  // ---------------- copy CV or & qualifiers from a type to the next -------------------
  // this is useful as what we're getting from a "const vector<int>" should be "const int", not "int" (the actualy value_type)

  template<typename T,typename U>
  struct copy_cv {
    using R =    remove_reference_t<T>;
    using U1 =   conditional_t<is_const_v<R>, my_add_const_t<U>, U>;
    using type = conditional_t<is_volatile_v<R>, my_add_volatile_t<U1>, U1>;
  };
  template<typename T,typename U> using copy_cv_t = typename copy_cv<T,U>::type;

  template<typename T,typename U>
  struct copy_ref {
    using URR = add_rvalue_reference_t<U>;
    using ULL = conditional_t<is_lvalue_reference_v<T>, add_lvalue_reference_t<U>, U>;
    using type = conditional_t<is_rvalue_reference_v<T>, URR, ULL>;
  };
  template<typename T,typename U> using copy_ref_t = typename copy_ref<T,U>::type;
  // NOTE: it is important to copy the const before copying the ref!
  template<typename T,typename U> using copy_cvref_t = copy_ref_t<T, copy_cv_t<T, U>>;


  // ---------------- reference_of and value_type_of -----------------

  template<class T> struct reference_of {};
  template<class T> requires requires { typename remove_reference_t<T>::reference; }
  struct reference_of<T> {
    using BareT = remove_reference_t<T>;
    // some containers do not allow modifying their contents via iterators (such as std::unordered_set)
    // Thus, we copy constness of the iterator's reference onto the container to copy it onto the reference later on
    using Traits = iterator_traits<iterator_of_t<BareT>>;
    using TraitsConstness = remove_reference_t<typename Traits::reference>;
    using correctT = copy_cv_t<TraitsConstness, BareT>;
    // copy constness of the container onto the reference
    using Ref = typename BareT::reference;
    static constexpr bool returning_rvalue = !is_reference_v<Ref>;
    using value_type = copy_cv_t<correctT, remove_reference_t<Ref>>;
    using type = conditional_t<returning_rvalue, value_type, add_lvalue_reference_t<value_type>>;
  };
  template<class T> struct reference_of<T*> { using type = T&; };
  template<class T> struct reference_of<T[]> { using type = T&; };
  template<class T> using reference_of_t  = typename reference_of<T>::type;
  template<class T> using value_type_of_t = remove_reference_t<reference_of_t<T>>;


  // ----------------- const_pointer and const_reference ---------------------
  // if a container has a const_pointer type, then return this type, otherwise return a pointer to const value_type
  template<class N, class T = void> struct get_const_ptr { using type = add_pointer_t<my_add_const_t<value_type_of_t<N>>>; };
  template<class N> struct get_const_ptr<N, void_t<typename N::const_pointer>> { using type = typename N::const_pointer; };
  template<class N> using const_pointer_of_t = typename get_const_ptr<remove_reference_t<N>>::type;

  // if a container has a pointer type, then return this type, otherwise return a pointer to value_type
  template<class N, class T = void> struct get_ptr { using type = add_pointer_t<value_type_of_t<N>>; };
  template<class N> struct get_ptr<N, void_t<typename N::pointer>> { using type = typename N::pointer; };
  template<class N> using pointer_of_t = typename get_ptr<remove_reference_t<N>>::type;

  // if a container has a const_reference type, then return this type, otherwise return an lvalue reference to const value_type
  template<class N, class T = void> struct get_const_ref { using type = add_lvalue_reference_t<my_add_const_t<typename iterator_traits<N>::value_type>>; };
  template<class N> struct get_const_ref<N, void_t<typename N::const_reference>> { using type = typename N::const_reference; };
  template<class N> using const_reference_of_t = typename get_const_ref<remove_reference_t<N>>::type;


  // ------------------ ITERATORS -----------------------------------

  // a lightweight end-iterator dummy that can be returned by calls to end() and compared to by other iterators
  struct GenericEndIterator{
    static bool is_valid() { return false; }
    bool operator==(const GenericEndIterator& x) const { return true; }
    template<class Other> bool operator==(const Other& x) const { return (x == *this); }
    template<class Other> bool operator!=(const Other& x) const { return !operator==(x); }
  };
  template<class T>
  bool operator==(const T& other, const GenericEndIterator&) { return !other.is_valid(); }
  template<class T>
  bool operator!=(const T& other, const GenericEndIterator&) { return other.is_valid(); }


  // a class that returns itself on dereference 
  // useful for iterators returning rvalues instead of lvalue references
  template<class T>
  struct self_deref {
    T t;
    template<class... Args>
    self_deref(Args&&... args): t(forward<Args>(args)...) {}
    T& operator*() { return t; }
    const T& operator*() const { return t; }
    T* operator->() { return &t; }
    const T* operator->() const { return &t; }
  };
  template<class R> // if the given reference is not a reference but an rvalue, then a pointer to it is modeled via self_deref
  using pointer_from_reference = conditional_t<is_reference_v<R>, add_pointer<remove_reference_t<R>>, self_deref<R>>;


  template<class Ref>
  struct iter_traits_from_reference {
    static constexpr bool returning_rvalue = !is_reference_v<Ref>;
    using reference = Ref;
    using value_type = remove_reference_t<reference>;
    using const_reference = conditional_t<returning_rvalue, const value_type, const value_type&>;
    using pointer = pointer_from_reference<reference>;
    using const_pointer = pointer_from_reference<const_reference>;
    using difference_type = ptrdiff_t;
    using size_type = size_t;
    using iterator_category = std::forward_iterator_tag; // by default we're a forward_iterator, overwrite this if you do bidirectional or random access
  };

  // not all iterator_traits of the STL provide "const_pointer" and "const_reference", so I'll do that for them
  template<typename T> requires HasIterTraits<T>
  struct my_iterator_traits: public iterator_traits<T> {
    // since the ::reference correctly gives "const T&", we'll just remove_reference_t from it
    using value_type = conditional_t<!is_pointer_v<T>, typename iterator_traits<T>::value_type, remove_reference_t<typename iterator_traits<T>::reference>>;
    using const_reference = const_reference_of_t<T>;
    using const_pointer   = const_pointer_of_t<T>;
  };

  template<MapType M> using key_type_of_t = typename remove_reference_t<M>::key_type;
  template<MapType M> using mapped_type_of_t = typename remove_reference_t<M>::mapped_type;

  template<class _Iterator>
  constexpr bool is_forward_iterator = is_same_v<typename my_iterator_traits<_Iterator>::iterator_category, forward_iterator_tag>;
  template<class _Iterator>
  constexpr bool is_bidirectional_iterator = is_same_v<typename my_iterator_traits<_Iterator>::iterator_category, bidirectional_iterator_tag>;
  template<class _Iterator>
  constexpr bool is_random_access_iterator = is_same_v<typename my_iterator_traits<_Iterator>::iterator_category, random_access_iterator_tag>;


  // compare iterators with their reverse versions
  template<typename T>
  bool operator==(const T& i1, const reverse_iterator<T>& i2) {  return (next(i1) == i2.base()); }
  template<typename T>
  bool operator!=(const T& i1, const reverse_iterator<T>& i2) {  return !operator==(i1,i2); }
  template<typename T>
  bool operator==(const reverse_iterator<T>& i2, const T& i1) {  return operator==(i1, i2); }
  template<typename T>
  bool operator!=(const reverse_iterator<T>& i2, const T& i1) {  return !operator==(i1, i2); }
  


  // ----------------------- lookup ----------------------------------

  template<class T> struct _findable_type { using type = value_type_of_t<T>; };
  template<MapType M> struct _findable_type<M> { using type = key_type_of_t<M>; };
  template<class T> using findable_type = typename _findable_type<remove_cvref_t<T>>::type;
  template<class T, class C> concept FindableType = requires(T t, findable_type<C> other) {
    { t == other } -> convertible_to<bool>;
    { t != other } -> convertible_to<bool>;
  };

  // a map lookup with default
  template<MapType Map, typename Key, typename Ref = mapped_type_of_t<Map>>
  Ref map_lookup(Map&& m, const Key& key, Ref&& default_val = Ref()) {
    const auto iter = m.find(key);
    return (iter == m.end()) ? default_val : iter->second;
  }

  template<size_t get_num>
  struct selector {
    template<class Tuple> auto& operator()(Tuple&& p) { return std::get<get_num>(forward<Tuple>(p)); }
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
  void merge_sort_fwd(FwdIt first, const FwdIt last, const typename my_iterator_traits<FwdIt>::difference_type N, Compare cmp = Compare{})
  {
    if (N <= 1) return;
    FwdIt middle = first;
    const size_t prefix = sorted_prefix(middle, N / 2);
    
    if(prefix < static_cast<size_t>(N/2)) merge_sort_fwd(first, middle, N/2, cmp);
    assert(std::is_sorted(first, middle, cmp));
    
    merge_sort_fwd(middle, last, N - N/2, cmp);
    assert(std::is_sorted(middle, last, cmp));
    
    if(std::is_forward_iterator<FwdIt>){
      //inplace_merge_fwd(first, middle, last, cmp);
      assert(false && "not implemented");
    } else std::inplace_merge(first, middle, last, cmp);
    assert(std::is_sorted(first, last, cmp));
  }
  // N log N sort, no matter what kind of iterator we get...
  template<class FwdIt, class Compare = std::less<>, class category = typename my_iterator_traits<FwdIt>::iterator_category>
  struct flexible_sort
  { flexible_sort(FwdIt first, FwdIt last, Compare cmp = Compare{}) { merge_sort_fwd(first, last, std::distance(first, last), cmp); } };
  template<class FwdIt, class Compare>
  struct flexible_sort<FwdIt, Compare, random_access_iterator_tag>
  { flexible_sort(FwdIt first, FwdIt last, Compare cmp = Compare{}) { std::sort(first, last, cmp); } };




  // ------------------------- HASHING -------------------------------------------

  template<typename T1, typename T2>
  struct hash<pair<T1, T2> >{
    size_t operator()(const pair<T1,T2>& p) const{
      const std::hash<T1> hasher1;
      const std::hash<T2> hasher2;
      return hash_combine(hasher1(p.first), hasher2(p.second));
    }
  };
  template<typename T>
  struct hash<reference_wrapper<T>>: public std::hash<T> {
    size_t operator()(const reference_wrapper<T>& p) const {
      return this->operator()(static_cast<const T&>(p));
    }
  };


  // ------------------------ FUNCTIONS -------------------------------------------
  // deferred function call for emplacements, thx @ Arthur O'Dwyer
  // emplace(f(x)) = construct + move (assuming f does copy elision)
  // emplace(deferred_call(f(x))) = (in-place) construct (assuming f does copy elision)
  template<class F>
  struct deferred_call_t {
    using T = invoke_result_t<F>;
    const F f;

    explicit deferred_call_t(F&& _f): f(forward<F>(_f)) {}
    operator T() { return f(); }
  };
  template<typename F>
  inline auto deferred_call(F&& f) { return deferred_call_t<F>(forward<F>(f)); }

  // a functional that ignores everything (and hopefully gets optimized out)
  template<class ReturnType = void>
  struct IgnoreFunction {
    template<class... Args>
    constexpr ReturnType operator()(Args&&... args) const { if constexpr (!is_void_v<ReturnType>) return ReturnType(); };
  };
  // a functional that just returns its argument (and hopefully gets optimized out)
  template<class T>
  struct IdentityFunction {
    template<class Q> requires is_same_v<remove_cvref_t<T>, remove_cvref_t<Q>>
    constexpr Q&& operator()(Q&& x) const { return forward<Q>(x); };
  };
  template<>
  struct IdentityFunction<void> {
    template<class Arg>
    constexpr Arg&& operator()(Arg&& x) const { return forward<Arg>(x); };
  };


  // --------------------- MODIFIED DATA STRUCTURES ------------------------

  template<class T, ContainerType C = deque<T>>
  class iterable_stack: public stack<T, C>
  {
    using stack<T, C>::c;
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


  // ----------------------- OUTPUT ---------------------------------------

  template<IterableType C> requires (!is_convertible_v<C,string_view>)
  inline std::ostream& operator<<(std::ostream& os, const C& objs)
  {
    os << '[';
    for(auto const& obj : objs) os << obj << ' ';
    return os << ']';
  }

  template<class T> std::string to_string(const T& x) { std::stringstream out; out << x; return out.str(); }

  template <typename A, typename B>
  ostream& operator<<(ostream& os, const pair<A,B>& p)
  {
    return os << '('<<p.first<<','<<p.second<<')';
  }
  template <typename A>
  ostream& operator<<(ostream& os, const reference_wrapper<A>& r)
  {
    return os << r.get();
  }


  // --------------------- PAIR OPERATIONS -------------------------------

  //! add two pairs of things
  template <typename A, typename B>
  pair<A,B> operator+(const pair<A,B>& l, const pair<A,B>& r)
  {
    return {l.first + r.first, l.second + r.second};
  }

  //! reverse a pair of things, that is, turn (x,y) into (y,x)
  template<typename A, typename B>
  inline pair<B,A> reverse(const pair<A,B>& p) { return {p.second, p.first}; }


  // ---------------------- BINARY SEARCH -----------------------------------

  //! find a number in a sorted list of numbers between lower_bound and upper_bound
  //if target is not in c, then return the index of the next larger item in c (or upper_bound if there is no larger item)
  template<class T, class A>
  uint32_t binary_search(const vector<T, A>& c, const uint32_t target, uint32_t lower_bound, uint32_t upper_bound)
  {
    while(lower_bound < upper_bound){
      const uint32_t middle = (lower_bound + upper_bound) / 2;
      const auto& c_middle = c[middle];
      if(c_middle == target) 
        return middle;
      else if(c_middle < target)
        lower_bound = middle + 1;
      else 
        upper_bound = middle;
    }
    assert(target <= c[lower_bound]);
    return lower_bound;
  }
  //! one-bound version of binary search: if only one bound is given, it is interpreted as lower bound
  template<class T, class A>
  uint32_t binary_search(const vector<T, A>& c, const uint32_t target, uint32_t lower_bound = 0)
  {
    return binary_search(c, target, lower_bound, c.size());
  }


  // --------------------- STRINGS & STRING_VIEW ------------------------------
  string operator+(const string& s1, const string_view s2) { return string(s1).append(s2); }
  string operator+(const string_view s1, const string& s2) { return string(s1).append(s2); }
  string operator+(const string_view s1, const string_view s2) { return string(s1).append(s2); }
  string operator+(const char* s1, const string_view s2) { return string(s1).append(s2); }
  string operator+(const string_view s1, const char* s2) { return string(s1).append(s2); }
  string operator+(const string_view s1, const char s2) { return string(s1) += s2; }
  string operator+(const string& s1, const char s2) { return string(s1) += s2; }



  // --------------------- MISC ---------------------------------------------
  template<class T, class Q>
  concept CompatibleValueTypes = is_same_v<value_type_of_t<T>, value_type_of_t<Q>>;
  template<class T, class Q>
  concept ConvertibleValueTypes = convertible_to<value_type_of_t<Q>, value_type_of_t<T>>;

  // functions returning void are treated differently from functions returning anything, even if that anything is then ignored; we unify the two here
  template<class T, class Else = uint_fast8_t>
  using ReturnableType = std::conditional_t<std::is_void_v<T>, Else, T>;

  // string_view conversion
  float stoi(const string_view sv) { int result = 0; from_chars(sv.data(), sv.data() + sv.size(), result); return result; }
  float stof(const string_view sv) { float result = 0.0f; from_chars(sv.data(), sv.data() + sv.size(), result); return result; }
  float stod(const string_view sv) { double result = 0.0; from_chars(sv.data(), sv.data() + sv.size(), result); return result; }

  // cheapo linear interval class - can merge and intersect
  template<class T = uint32_t>
  struct linear_interval: public std::array<T, 2> {
    using Parent = std::array<T,2>;
    using Parent::at;

    T& low() { return (*this)[0]; }
    T& high() { return (*this)[1]; }
    const T& low() const { return (*this)[0]; }
    const T& high() const { return (*this)[1]; }

    linear_interval(const T& init_lo, const T& init_hi): Parent{init_lo, init_hi} {}
    linear_interval(const T& init): linear_interval(init, init) {}
    
    void merge(const linear_interval& other) {
      update_lo(other.at(0));
      update_hi(other.at(1));
    }
    void intersect(const linear_interval& other) {
      at(0) = max(at(0), other.at(0));
      at(1) = min(at(1), other.at(1));
    }
    void update_lo(const T& lo) { at(0) = min(at(0), lo); }
    void update_hi(const T& hi) { at(1) = max(at(1), hi); }
    void update(const T& x) { update_lo(x); update_hi(x); }

    bool contains(const linear_interval& other) const { return (at(0) <= other.at(0)) && (at(1) >= other.at(1)); }
    bool contains(const T& val) const { return (at(0) <= val) && (at(1) >= val); }
    bool overlaps(const linear_interval& other) const { return (at(0) >= other.at(0)) ? (at(0) <= other.at(1)) : (at(1) >= other.at(0)); }
    bool contained_in(const linear_interval& other) const { return other.contains(*this); }
    bool left_of(const T& val) { return at(1) <= val; }
    bool strictly_left_of(const T& val) { return at(1) < val; }
    bool right_of(const T& val) { return val <= at(0); }
    bool strictly_right_of(const T& val) { return val < at(0); }

    friend ostream& operator<<(ostream& os, const linear_interval& i) { return os << '[' << i.at(0) << ',' << i.at(1) << ']'; }
  };
  // an interval is "bigger than" a value if it lies entirely on the right of that value
  template<class T = uint32_t>
  bool operator<(const T& value, const linear_interval<T>& interval) { return interval.strictly_right_of(value); }
  template<class T = uint32_t>
  bool operator>(const T& value, const linear_interval<T>& interval) { return interval.strictly_left_of(value); }


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
    template<class... Args> dispenser(Args&&... args): data(forward<Args>(args)...) {}
    template<class... Args> T& operator()(Args&&... args) { return data; }
    template<class... Args> const T& operator()(Args&&... args) const { return data; }
  };


  template<class T = int>
  struct minus_one { static constexpr T value = -1; };

  // specialize to your hearts desire
  template<class T> struct default_invalid {
    using type = conditional_t<is_basically_arithmetic_v<T>, minus_one<T>, void>;
  };
  template<class T> using default_invalid_t = typename default_invalid<T>::type;
}

