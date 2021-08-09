
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

#include "hash_utils.hpp"
#include "stl_concepts.hpp"

namespace std{

  template<class T> constexpr bool is_const_ref = is_const_v<remove_reference_t<T>>;

  // don't add const to void...
  template<class T> using my_add_const_t = conditional_t<is_void_v<T>, void, add_const_t<T>>;
  // is_arithmetic is false for pointers.... why?
  template<class T> constexpr bool is_really_arithmetic_v = is_arithmetic_v<T> || is_pointer_v<T>;
  // anything that can be converted from and to int is considered "basically arithmetic"
  template<class T> constexpr bool is_basically_arithmetic_v = is_convertible_v<int, remove_cvref_t<T>> && is_convertible_v<remove_cvref_t<T>, int>;

  template<class T> concept ArithmeticType =  is_really_arithmetic_v<T>;


  template<class T> constexpr bool is_pair = false;
  template<class X, class Y> constexpr bool is_pair<std::pair<X,Y>> = true;

  // if a container has a const_pointer type, then return this type, otherwise return a pointer to const value_type
  template<class N, class T = void> struct get_const_ptr { using type = add_pointer_t<my_add_const_t<typename iterator_traits<N>::value_type>>; };
  template<class N> struct get_const_ptr<N, void_t<typename N::const_pointer>> { using type = typename N::const_pointer; };
  template<class N> using get_const_ptr_t = typename get_const_ptr<remove_cvref_t<N>>::type;

  // if a container has a const_reference type, then return this type, otherwise return an lvalue reference to const value_type
  template<class N, class T = void> struct get_const_ref { using type = add_lvalue_reference_t<my_add_const_t<typename iterator_traits<N>::value_type>>; };
  template<class N> struct get_const_ref<N, void_t<typename N::const_reference>> { using type = typename N::const_reference; };
  template<class N> using get_const_ref_t = typename get_const_ref<remove_cvref_t<N>>::type;

  // iterator_traits<const T*>::value_type is "T" not "const T"? What the hell, STL?
  template<typename T>
  struct my_iterator_traits: public iterator_traits<T> {
    // since the ::reference correctly gives "const T&", we'll just remove_reference_t from it
    using value_type = conditional_t<!is_pointer_v<T>, typename iterator_traits<T>::value_type, remove_reference_t<typename iterator_traits<T>::reference>>;
    using const_reference = get_const_ref_t<T>;
    using const_pointer   = get_const_ptr_t<T>;
  };

  template<class _Iterator>
  constexpr bool is_forward_iterator = is_same_v<typename my_iterator_traits<_Iterator>::iterator_category, forward_iterator_tag>;
  template<class _Iterator>
  constexpr bool is_bidirectional_iterator = is_same_v<typename my_iterator_traits<_Iterator>::iterator_category, bidirectional_iterator_tag>;
  template<class _Iterator>
  constexpr bool is_random_access_iterator = is_same_v<typename my_iterator_traits<_Iterator>::iterator_category, random_access_iterator_tag>;

  template<typename T,typename U>
  struct copy_cv
  {
    using R =    remove_reference_t<T>;
    using U1 =   conditional_t<is_const_v<R>, my_add_const_t<U>, U>;
    using type = conditional_t<is_volatile_v<R>, add_volatile_t<U1>, U1>;
  };
  template<typename T,typename U> using copy_cv_t = typename copy_cv<T,U>::type;

  //NOTE: add_rvalue_reference<A&> = A& that's not very intuitive...
  template<class T>
  using make_rvalue_reference = std::add_rvalue_reference_t<std::remove_reference_t<T>>;

  template<typename T,typename U>
  struct copy_ref
  {
    using R =    remove_reference_t<T>;
    using U1 =   conditional_t<is_lvalue_reference_v<T>, add_lvalue_reference_t<U>, U>;
    using type = conditional_t<is_rvalue_reference_v<T>, add_rvalue_reference_t<U1>, U1>;
  };
  template<typename T,typename U> using copy_ref_t = typename copy_ref<T,U>::type;
  // NOTE: it is important to copy the const before copying the ref!
  template<typename T,typename U> using copy_cvref_t = copy_ref_t<T, copy_cv_t<T, U>>;

  // ever needed to get an interator if T was non-const and a const_iterator if T was const? Try this:
  template<class T> using iterator_of_t = conditional_t<is_const_v<remove_reference_t<T>>,
                                                        typename remove_reference_t<T>::const_iterator,
                                                        typename remove_reference_t<T>::iterator>;
  template<class T> using reverse_iterator_of_t = reverse_iterator<iterator_of_t<T>>;


  // turn a reference into const reference or rvalue into const rvalue
  template<class T> struct _const_reference { using type = my_add_const_t<T>; };
  template<class T> struct _const_reference<T&> { using type = my_add_const_t<T>&; };
  template<class T> struct _const_reference<T&&> { using type = my_add_const_t<T>&&; };
  template<class T> using const_reference_t = typename _const_reference<T>::type;


  // a map lookup with default
  template <typename _Map, typename _Key, typename _Ref = typename _Map::value_type>
  inline _Ref map_lookup(_Map&& m, const _Key& key, _Ref&& default_val)
  {
    const auto iter = m.find(key);
    return (iter == m.end()) ? default_val : iter->second;
  }


  // a class that returns itself on dereference 
  // useful for iterators returning rvalues instead of lvalue references
  template<class T, bool = is_really_arithmetic_v<remove_reference_t<T>>>
  struct self_deref: public T
  {
    using T::T;
    self_deref(const T& x): T(x) {}
    T& operator*() { return *this; }
    const T& operator*() const { return *this; }
    T* operator->() { return this; }
    const T* operator->() const { return this; }
  };
  template<class T>
  struct self_deref<T, true>
  {
    T value;

    self_deref(const T& x): value(x) {}
    T& operator*() { return value; }
    const T& operator*() const { return value; }
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
  };


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
    
    if(prefix < (size_t)(N/2)) merge_sort_fwd(first, middle, N/2, cmp);
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


  // compare iterators with their reverse versions
  template<typename T>
  bool operator==(const T& i1, const reverse_iterator<T>& i2) {  return (next(i1) == i2.base()); }
  template<typename T>
  bool operator!=(const T& i1, const reverse_iterator<T>& i2) {  return !operator==(i1,i2); }
  template<typename T>
  bool operator==(const reverse_iterator<T>& i2, const T& i1) {  return operator==(i1, i2); }
  template<typename T>
  bool operator!=(const reverse_iterator<T>& i2, const T& i1) {  return !operator==(i1, i2); }
  
  template<bool condition, template<class> class X, template<class> class Y>
  struct conditional_template { template<class Z> using type = X<Z>; };
  template<template<class> class X, template<class> class Y>
  struct conditional_template<false, X, Y> { template<class Z> using type = Y<Z>; };

  // begin() and end() for forward and reverse iterators
  template<IterableType C,
           bool reverse = false,
           template<class> class Iterator = conditional_template<reverse, reverse_iterator_of_t, iterator_of_t>::template type>
  struct BeginEndIters
  {
    using NonConstC = remove_cv_t<C>;
    using iterator = Iterator<C>;
    using const_iterator = Iterator<const C>;
    template<class... Args>
    static iterator begin(NonConstC& c, Args&&... args) { return {std::begin(c), forward<Args>(args)...}; }
    template<class... Args>
    static iterator end(NonConstC& c, Args&&... args) { return {std::end(c), forward<Args>(args)...}; }
    template<class... Args>
    static const_iterator begin(const C& c, Args&&... args) { return {std::begin(c), forward<Args>(args)...}; }
    template<class... Args>
    static const_iterator end(const C& c, Args&&... args) { return {std::end(c), forward<Args>(args)...}; }
  };
  template<IterableType C, template<class> class Iterator>
  struct BeginEndIters<C, true, Iterator>
  {
    using NonConstC = remove_cv_t<C>;
    using iterator = reverse_iterator<Iterator<C>>;
    using const_iterator = reverse_iterator<Iterator<const C>>;
    template<class... Args>
    static iterator begin(NonConstC& c, Args&&... args) { return {std::rbegin(c), forward<Args>(args)...}; }
    template<class... Args>
    static iterator end(NonConstC& c, Args&&... args) { return {std::rend(c), forward<Args>(args)...}; }
    template<class... Args>
    static const_iterator begin(const C& c, Args&&... args) { return {std::rbegin(c), forward<Args>(args)...}; }
    template<class... Args>
    static const_iterator end(const C& c, Args&&... args) { return {std::rend(c), forward<Args>(args)...}; }
  };


  // why are those things not defined per default by STL???
  template<IterableType C>
  inline typename my_iterator_traits<C>::const_iterator max_element(const C& c) { return max_element(begin(c), end(c)); }


  // deferred function call for emplacements, thx @ Arthur O'Dwyer
  // emplace(f(x)) = construct + move (assuming f does copy elision)
  // emplace(deferred_call(f(x))) = (in-place) construct (assuming f does copy elision)
  template<class F>
  struct deferred_call_t
  {
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
    constexpr ReturnType operator()(Args&&... args) const { return ReturnType(); };
  };
  template<>
  struct IgnoreFunction<void> {
    template<class... Args>
    constexpr void operator()(Args&&... args) const { };
  };

  template<typename T1, typename T2>
  struct hash<pair<T1, T2> >{
    size_t operator()(const pair<T1,T2>& p) const{
      const std::hash<T1> hasher1;
      const std::hash<T2> hasher2;
      return hash_combine(hasher1(p.first), hasher2(p.second));
    }
  };
  template<typename T>
  struct hash<reference_wrapper<T>>{
    size_t operator()(const reference_wrapper<T>& p) const{
      return (std::hash<T>())(p);
    }
  };

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


  template<IterableType C> requires (!is_convertible_v<C,string>)
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

  //! add two pairs of things
  template <typename A, typename B>
  pair<A,B> operator+(const pair<A,B>& l, const pair<A,B>& r)
  {
    return {l.first + r.first, l.second + r.second};
  }

  //! reverse a pair of things, that is, turn (x,y) into (y,x)
  template<typename A, typename B>
  inline pair<B,A> reverse(const pair<A,B>& p) { return {p.second, p.first}; }

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


  //! decrease a value in a map, pointed to by an iterator; return true if the value was decreased and false if the item was removed
  template<class Map, long threshold = 1>
  inline bool decrease_or_remove(Map& m, const typename Map::iterator& it)
  {
    if(it->second == threshold) {
      m.erase(it);
      return false;
    } else {
      --(it->second);
      return true;
    }
  }


  // a deleter that will or will not delete, depeding on its argument upon construction (for shared_ptr's)
  template<class T>
  struct SelectiveDeleter {
    const bool del;
    SelectiveDeleter(const bool _del): del(_del) {}
    inline void operator()(T* p) const { if(del) delete p; }
  };
  using NoDeleter = IgnoreFunction<>;

  template<class T = int>
  struct minus_one
  {
    static constexpr T value = -1;
  };

  // specialize to your hearts desire
  template<class T> struct default_invalid
  {
    using type = conditional_t<is_basically_arithmetic_v<T>, minus_one<T>, void>;
  };
  template<class T> using default_invalid_t = typename default_invalid<T>::type;
}

