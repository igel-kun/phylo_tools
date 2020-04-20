
#pragma once

#include<sstream>
#include<type_traits>
#include "hash_utils.hpp"

// circular shift
#define rotl(x,y) ((x << y) | (x >> (sizeof(x)*CHAR_BIT - y)))
#define rotr(x,y) ((x >> y) | (x << (sizeof(x)*CHAR_BIT - y)))

namespace std{

#if __cplusplus <= 201703L
  template<class T> using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;
#endif

  // iterator_traits<const T*>::value_type is "T" not "const T"? What the hell, STL?
  template<typename T>
  struct my_iterator_traits: public iterator_traits<T>
  {
    // since the ::reference correctly gives "const T&", we'll just remove_reference_t from it
    using value_type = remove_reference_t<typename iterator_traits<T>::reference>;
  };


  template<typename T,typename U>
  struct copy_cv
  {
    using R =    remove_reference_t<T>;
    using U1 =   conditional_t<is_const_v<R>, add_const_t<U>, U>;
    using type = conditional_t<is_volatile_v<R>, add_volatile_t<U1>, U1>;
  };
  template<typename T,typename U> using copy_cv_t = typename copy_cv<T,U>::type;

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


  template<class T> struct is_vector: public false_type {};
  template<class T, class Alloc> struct is_vector<vector<T, Alloc>>: public true_type {};
  template<class T, class Traits, class Alloc> struct is_vector<basic_string<T,Traits,Alloc>>: public true_type {};
  template<class T> constexpr bool is_vector_v = is_vector<remove_cvref_t<T>>::value;

  //a container is something we can iterate through, that is, it has an iterator
  template<class N, class T = void> struct is_container: public false_type {};
  template<class N> struct is_container<N, void_t<typename N::value_type>>: public true_type {};
  template<class N> constexpr bool is_container_v = is_container<remove_cvref_t<N>>::value;
  
  // a map is something with a mapped_type in it
  template<class N, class T = void> struct is_map: public false_type {};
  template<class N> struct is_map<N, void_t<typename N::mapped_type>>: public true_type {};
  template<class N> constexpr bool is_map_v = is_map<remove_cvref_t<N>>::value;


  // ever needed to get an interator if T was non-const and a const_iterator if T was const? Try this:
  template<class T> using iterator_of_t = conditional_t<is_const_v<remove_reference_t<T>>,
    typename remove_reference_t<T>::const_iterator, typename remove_reference_t<T>::iterator>;
  template<class T> using reverse_iterator_of_t = reverse_iterator<iterator_of_t<T>>;


  // turn a reference into const reference or rvalue into const rvalue
  template<class T> struct _const_reference { using type = add_const_t<T>; };
  template<class T> struct _const_reference<T&> { using type = add_const_t<T>&; };
  template<class T> struct _const_reference<T&&> { using type = add_const_t<T>&&; };
  template<class T> using const_reference_t = typename _const_reference<T>::type;



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
    using iterator     = iterator_of_t<Container>;
    static iterator begin(Container& c) { return c.begin(); }
    static iterator end(Container& c) { return c.end(); }
  };
  template<class Container>
  struct BeginEndIters<Container, true>
  {
    using iterator     = reverse_iterator_of_t<Container>;
    static iterator begin(Container& c) { return c.rbegin(); }
    static iterator end(Container& c) { return c.rend(); }
  };


  // why are those things not defined per default by STL???

  template<class _Container>
  inline typename _Container::const_iterator max_element(const _Container& c) { return max_element(c.begin(), c.end()); }




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
  template<class Map, class Key = typename Map::key_type, class Mapped = copy_cvref_t<Map, typename Map::mapped_type>>
  struct MapGetter
  {
    using PropertyType = typename Map::mapped_type;
    Map& m;
    MapGetter(Map& _m): m(_m) {}

    Mapped& operator()(const Key& key) const { return m.at(key); }
  };


  template<class C, class = enable_if_t<is_container_v<C>>>
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


  // a deleter that will or will not delete, depeding on its argument upon construction (for shared_ptr's)
  template<class T>
  struct SelectiveDeleter {
    const bool del;
    SelectiveDeleter(const bool _del): del(_del) {}
    inline void operator()(T* p) const { if(del) delete p; }
  };
  struct NoDeleter {
    template<class T>
    inline void operator()(T* p) const {}
  };

}

