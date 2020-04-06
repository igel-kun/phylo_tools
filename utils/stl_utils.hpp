
#pragma once

#include<sstream>
#include "hash_utils.hpp"

// circular shift
#define rotl(x,y) ((x << y) | (x >> (sizeof(x)*CHAR_BIT - y)))
#define rotr(x,y) ((x >> y) | (x << (sizeof(x)*CHAR_BIT - y)))


namespace std{

  template<typename T>
  struct is_reverse_iterator: public std::false_type {};

  template<typename T>
  struct is_reverse_iterator<std::reverse_iterator<T>>: public std::true_type {};

  // ever needed to get an interator if T was non-const and a const_iterator if T was const? Try this:
  template<class Container, bool reverse>
  struct IteratorOf {
    using type = std::conditional_t<reverse, std::reverse_iterator<typename Container::iterator>, typename Container::iterator>;
  };
  template<class Container, bool reverse>
  struct IteratorOf<const Container, reverse> {
    using type = std::conditional_t<reverse, std::reverse_iterator<typename Container::const_iterator>, typename Container::const_iterator>;
  };

  template<class Container, bool reverse = false>
  using IteratorOf_t = typename IteratorOf<Container, reverse>::type;


  // copy const specifier from T to Q (that is, if T is const, then return const Q, otherwise return Q
  template<class T, class Q>
  struct CopyConst { using type = Q; };
  template<class T, class Q>
  struct CopyConst<const T, Q> { using type = const Q; };

  // a class that returns itself on dereference 
  // (useful for iterators returning constructed things instead of pointers to things - for example producing Edges from a Node and a set of Nodes)
  template<class T>
  struct self_deref: public T
  {
    using T::T;
    self_deref(const T& x): T(x) {}
    T& operator*() { return *this; }
    const T& operator*() const { return *this; }
    T* operator->() { return this; }
    const T* operator->() const { return this; }
  };


  // compare iterators with their reverse versions
  template<typename T>
  bool operator==(const T& i1, const reverse_iterator<T>& i2) {  return (next(i1) == i2.base()); }
  template<typename T>
  bool operator!=(const T& i1, const reverse_iterator<T>& i2) {  return !operator==(i1,i2); }
  template<typename T>
  bool operator==(const reverse_iterator<T>& i2, const T& i1) {  return operator==(i1, i2); }
  template<typename T>
  bool operator!=(const reverse_iterator<T>& i2, const T& i1) {  return !operator==(i1, i2); }
  
  // begin() and end() for forward and reverse iterators
  template<class Container, bool reverse>
  struct BeginEndIters
  {
    using iterator     = IteratorOf_t<Container, false>;
    static iterator begin(Container& c) { return c.begin(); }
    static iterator end(Container& c) { return c.end(); }
  };
  template<class Container>
  struct BeginEndIters<Container, true>
  {
    using iterator     = IteratorOf_t<Container, true>;
    static iterator begin(Container& c) { return c.rbegin(); }
    static iterator end(Container& c) { return c.rend(); }
  };


  // why are those things not defined per default by STL???

  template<class _Container>
  inline typename _Container::const_iterator max_element(const _Container& c) { return max_element(c.begin(), c.end()); }


  // classes to extract second or first from a pair
  template<class _Pair, class _ItemType>
  struct _extract_second{
    using value_type = _ItemType;
    using reference = value_type&;
    const reference operator()(const _Pair& e) const { return e.second; }
    reference operator()(_Pair& e) const { return e.second; }
  };
  template<class _Pair, class _ItemType>
  struct _extract_first{
    using value_type = _ItemType;
    using reference = value_type&;
    const reference operator()(const _Pair& e) const { return e.first; }
    reference operator()(_Pair& e) const { return e.first; }
  };
  template<class _Pair>
  struct extract_second: public _extract_second<_Pair, const typename _Pair::second_type> {};
  template<class _Pair>
  struct extract_second<const _Pair>: public _extract_second<const _Pair, const typename _Pair::second_type> {};
  template<class _Pair>
  struct extract_first: public _extract_first<_Pair, const typename _Pair::first_type> {};
  template<class _Pair>
  struct extract_first<const _Pair>: public _extract_first<const _Pair, const typename _Pair::first_type> {};




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

  template<class T, class Container = std::deque<T>>
  class iterable_stack: public std::stack<T, Container>
  {
    using std::stack<T, Container>::c;
  public:
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    iterator begin() { return c.begin(); }
    iterator end() { return c.end(); }
    const_iterator begin() const { return c.begin(); }
    const_iterator end() const { return c.end(); }
  };


  //! simple property getter
  template<class Map, class Key = typename Map::key_type, class Mapped = typename Map::mapped_type>
  struct MapGetter
  {
    Map& m;
    MapGetter(Map& _m): m(_m) {}

    Mapped& operator()(const Key& key) const { return m[key]; }
  };

  template<class T>
  struct DerefGetter
  {
    template<class Ptr>
    T& operator()(const Ptr& p) const { return *p; }
  };

  template<class C>
  inline std::ostream& _print(std::ostream& os, const C& objs)
  {
    os << '[';
    for(auto const& obj : objs) os << obj << ' ';
    return os << ']';
  }
  template<class T> std::ostream& operator<<(std::ostream& os, const vector<T>& x) { return _print(os, x); }
  template<class T> std::ostream& operator<<(std::ostream& os, const list<T>& x) { return _print(os, x); }
  template<class T> std::ostream& operator<<(std::ostream& os, const set<T>& x) { return _print(os, x); }
  template<class T> std::ostream& operator<<(std::ostream& os, const unordered_set<T>& x) { return _print(os, x); }
  template<class T> std::ostream& operator<<(std::ostream& os, const iterable_stack<T>& x) { return _print(os, x); }
//  template<class T> std::ostream& operator<<(std::ostream& os, const vector_map<T>& x) { return _print(os, x); }
//  template<class T> std::ostream& operator<<(std::ostream& os, const vector_hash<T>& x) { return _print(os, x); }
  template<class P, class Q> std::ostream& operator<<(std::ostream& os, const map<P,Q>& x) { return _print(os, x); }
  template<class P, class Q> std::ostream& operator<<(std::ostream& os, const unordered_map<P,Q>& x) { return _print(os, x); }


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

  //! a hash computation for a set, XORing its members
  template<typename _Container>
  struct set_hash{
    size_t operator()(const _Container& S) const
    {
      size_t result = 0;
      static const std::hash<typename _Container::value_type> Hasher;
      for(const auto& i : S)
        result ^= Hasher(i);
      return result;
    }
  };

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<typename _Container>
  struct list_hash{
    size_t operator()(const _Container& S) const
    {
      size_t result = 0;
      static const std::hash<typename _Container::value_type> Hasher;
      for(const auto& i : S)
        result = rotl(result, 1) ^ Hasher(i);
      return result;
    }
  };

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
  template<typename Container>
  uint32_t binary_search(const Container& c, const uint32_t target, uint32_t lower_bound, uint32_t upper_bound)
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
  template<typename Container>
  uint32_t binary_search(const Container& c, const uint32_t target, uint32_t lower_bound = 0)
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

}

