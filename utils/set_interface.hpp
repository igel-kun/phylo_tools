
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <type_traits>
#include "iter_bitset.hpp"
#include "stl_utils.hpp"
// unifcation for the set interface
// this is to use unordered_set<uint32_t> with the same interface as iterable_bitset

#pragma once

namespace std {
#warning TODO: write a back() function (doing front() for unordered containers)

  // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :) 
  template<class Set>
  inline void flip(Set& _set, const typename Set::reference index)
  {
    const auto emp_res = _set.emplace(index);
    if(!emp_res.second) _set.erase(emp_res.first);
  }
  template<class T>
  inline void flip(iterable_bitset<T>& _set, const uintptr_t index) { return _set.flip(index); }

  template<class Set, class Other, class = enable_if_t<is_container_v<Set> && !is_bitset_v<Set>>>
  inline void intersect(Set& target, const Other& source)
  {
    for(auto target_it = target.begin(); target_it != target.end();)
      if(!test(source, *target_it))
        target_it = target.erase(target_it); 
      else ++target_it;
  }
  template<class T>
  inline void intersect(iterable_bitset<T>& target, const iterable_bitset<T>& source) { target &= source; }

  // NOTE: if T is const, then iterator_of_t gives a const_iterator, and the reference of that SHOULD be a const_reference...
  template<class T>
  inline typename my_iterator_traits<iterator_of_t<T>>::reference front(T&& _set)
  {
    assert(!_set.empty());
    return *(_set.begin());
  }

  template<class T>
  inline typename my_iterator_traits<iterator_of_t<T>>::reference back(T&& _set)
  {
    assert(!_set.empty());
    return *(_set.rbegin());
  }


  /*
  template<class T>
  inline typename my_iterator_traits<iterator_of_t<T>>::value_type&& front(T&& _set)
  {
    assert(!_set.empty());
    return move(*(_set.begin()));
  }

  template<class T>
  inline typename my_iterator_traits<iterator_of_t<T>>::value_type&& back(T&& _set)
  {
    assert(!_set.empty());
    return move(*(_set.rbegin()));
  }
*/

  template<class T>
  using emplace_result = pair<typename remove_reference_t<T>::iterator, bool>;

  // on non-map containers, append = emplace
  template<class Set,
    enable_if_t<is_container_v<Set> && !is_map_v<Set> && !is_vector_v<Set>, int> = 0,
    class ...Args>
  inline emplace_result<Set> append(Set& _set, Args&&... args) { return _set.emplace(forward<Args>(args)...); }

  // on vectors, append =  emplace_back 
  // this is bad: vector_map<> can be "upcast" to vector<> so this will always conflict with the append for maps
  // the suggestion on stackoverflow is "stop spitting against the wind"... :(
  // so for now, I'm using try_emplace() in all places that would be ambiguous
  template<class T, class A, class First,
    //enable_if_t<!is_convertible_v<remove_cvref_t<First>, vector<T, A>>, int> = 0,
    enable_if_t<!is_container_v<First>, int> = 0,
    class ...Args>
  inline emplace_result<vector<T, A>> append(vector<T, A>& _vec, First&& first, Args&&... args)
  { // NOTE: as we want an iterator back, I'd much rather say "emplace(end()..)"; however, that will not compile for Adjacencies with "const Node"
    _vec.emplace_back(forward<First>(first), forward<Args>(args)...);
    return {std::prev(_vec.end()), true};
  }
  // dummy function to not insert anything into an appended vector
  template<typename T> inline emplace_result<vector<T>> append(vector<T>& _vec) { return {_vec.begin(), true}; }
 
  // on maps to primitives, append = try_emplace
  template<class Map,
    enable_if_t<is_map_v<Map> && !is_container_v<typename Map::mapped_type>, int> = 0,
    class ...Args>
  inline emplace_result<Map> append(Map& _map, const typename Map::key_type& _key, Args&&... args)
  { return _map.try_emplace(_key, std::forward<Args>(args)...); }

  // on maps to sets, append = append to set at map[key]
  //NOTE: return an iterator to the pair in the map whose second now contains the newly constructed item
  //      also return a bool indicating whether insertion took place
  //NOTE: this also works for emplacing a string into a map that maps to strings, the "inserting" appends below are called in this case
  template<class Map,
    enable_if_t<is_map_v<Map> && is_container_v<typename Map::mapped_type>, int> = 0,
    class ...Args>
  inline emplace_result<Map> append(Map& _map, const typename Map::key_type& _key, Args&&... args)
  {
    const auto iter = _map.try_emplace(_key).first;
    const bool success = append(iter->second, forward<Args>(args)...).second;
    return {iter, success};
    //return append(_map.try_emplace(_key).first->second, forward<Args>(args)...);
  }
  // append with 2 containers means to add the second to the end of the first
  template<class _ContainerA, class _ContainerB,
    enable_if_t<is_container_v<_ContainerA> && is_container_v<_ContainerB> &&
                !is_convertible_v<_ContainerA, vector<typename _ContainerA::value_type>> &&
                is_same_v<typename _ContainerA::value_type, typename _ContainerB::value_type>, int> = 0>
  inline emplace_result<_ContainerA> append(_ContainerA& x, const _ContainerB& y)
  {
    x.insert(y.begin(), y.end());
    return {x.begin(), true};
  }
  template<class T, class A, class _ContainerB,
    enable_if_t<is_container_v<_ContainerB>, int> = 0,
    enable_if_t<is_same_v<std::remove_cvref_t<typename _ContainerB::value_type>, T>, int> = 0>
  inline emplace_result<std::vector<T,A>> append(std::vector<T,A>& x, const _ContainerB& y)
  {
    x.insert(x.end(), y.begin(), y.end());
    return {x.begin(), true};
  }
/*
  template<class Target, class Source, enable_if_t<is_container_v<Target> && !is_vector_v<Target> && is_container_v<Source>, int> = 0>
  Target convert(Source&& s)
  {
    Target t;
    t.insert(s.begin(), s.end());
    return t;
  }
  template<class Target, class Source, enable_if_t<is_vector_v<Target> && is_container_v<Source>, int> = 0>
  Target convert(Source&& s)
  {
    Target t;
    t.insert(t.end(), s.begin(), s.end());
    return t;
  }
*/

  // all this BS with containers supporting contains() or not is really unnerving
  template<class T>
  inline bool test(const T& _set, const typename T::value_type& key) { return _set.count(key); }
  template<class T, class = enable_if_t<is_map_v<T>>>
  inline bool test(const T& _map, const typename T::key_type& key) { return _map.count(key); }


  template<class Container>
  bool are_disjoint(const Container& x, const Container& y)
  {
    if(x.size() < y.size()){
      for(const auto& item: x) if(y.contains(item)) return false;
    } else {
      for(const auto& item: y) if(x.contains(item)) return false;
    }
    return true;
  }

  template<class Container>
  auto_iter<typename Container::const_iterator> any_intersecting(const Container& x, const Container& y)
  {
    typename Container::const_iterator result;
    if(x.size() < y.size()){
      for(const auto& item: x)
        if((result = y.find(item)) != y.end()) return {result, y.end()};
    } else {
      for(const auto& item: y){
        if((result = x.find(item)) != x.end()) return {result, x.end()};
      }
    }
    return {x.end(), x.end()};
  }




  // I would write an operator= to assign unordered_set<uint32_t> from iterable_bitset, but C++ forbids operator= as free function... WHY?!?!
  template<class T>
  inline iterable_bitset<T>& copy(const unordered_set<uint32_t>& x, iterable_bitset<T>& y)
  {
    y.clear();
    y.insert(x.begin(), x.end());
    return y;
  }
  template<class T, class Set, class = enable_if_t<is_container_v<Set>>>
  inline Set& copy(const iterable_bitset<T>& x, Set& y)
  {
    y.clear();
    y.insert(x.begin(), x.end());
    return y;
  }
  template<class T, class I, class = enable_if_t<is_integral_v<I>>>
  inline vector<I>& copy(const iterable_bitset<T>& x, vector<I>& y)
  {
    y.clear();
    y.insert(y.begin(), x.begin(), x.end());
    return y;
  }
  template<class T1, class T2>
  inline iterable_bitset<T2>& copy(const iterable_bitset<T1>& x, iterable_bitset<T2>& y)
  { return y = x; }
  template<class Set, class = enable_if_t<is_container_v<Set>>>
  inline Set& copy(const Set& x, Set& y) { return y = x; }


  template<class T, class=void> struct has_pop : false_type {};
  template<class T> struct has_pop<T, void_t<decltype(declval<T>().pop())>>: std::true_type {};
  template<class T> constexpr bool has_pop_v = has_pop<T>::value;

  // value-copying pop operations
  template<class Set, class = enable_if_t<has_pop_v<Set>>>
  typename Set::value_type value_pop(Set& s)
  {
    typename Set::value_type t = s.top();
    s.pop();
    return t;
  }
  template<class Set, class = enable_if_t<is_container_v<Set>>>
  auto value_pop_front(Set& s)
  {
    typename Set::value_type t = front(s);
    s.erase(s.begin());
    return t;
  }
  template<class Set, class = enable_if_t<is_container_v<Set>>>
  auto value_pop_back(Set& s)
  {
    typename Set::value_type t = back(s);
    s.erase(s.rbegin());
    return t;
  }
  template<class T, class A>
  auto value_pop_front(vector<T,A>& s)
  {
    T t = front(s);
    s.pop_front();
    return t;
  }
  template<class T, class A>
  auto value_pop_back(vector<T,A>& s)
  {
    T t = back(s);
    s.pop_back();
    return t;
  }
  template<class T>
  unordered_set<typename iterable_bitset<T>::value_type> to_set(const iterable_bitset<T>& x)
  {
    unordered_set<typename iterable_bitset<T>::value_type> result;
    return copy(x, result);
  }


  //! a hash computation for a set, XORing its members
  template<typename _Container>
  struct set_hash{
    inline static constexpr std::hash<typename _Container::value_type> Hasher{};
    size_t operator()(const _Container& S) const
    {
      size_t result = 0;
      for(const auto& i : S)
        result ^= Hasher(i);
      return result;
    }
  };
  // iterable bitset can be hashed faster
  template<> struct set_hash<ordered_bitset>: public hash<ordered_bitset> {};
  template<> struct set_hash<unordered_bitset>: public hash<unordered_bitset> {};

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<typename _Container>
  struct list_hash{
    inline static constexpr std::hash<typename _Container::value_type> Hasher{};
    size_t operator()(const _Container& S) const
    {
      size_t result = 0;
      for(const auto& i : S)
        result = rotl(result, 1) ^ Hasher(i);
      return result;
    }
  };



}
