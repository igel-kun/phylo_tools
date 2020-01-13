
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "iter_bitset.hpp"

// unifcation for the set interface
// this is to use unordered_set<uint32_t> with the same interface as iterable_bitset

#pragma once

namespace std {

  template <class N>
  struct is_stl_set_type<std::set<N>> { static const int value = 1; };
  template <class N>
  struct is_stl_set_type<std::unordered_set<N>> { static const int value = 1; };
 
  template <class N, class M>
  struct is_stl_map_type<std::map<N, M>> { static const int value = 1; };
  template <class N, class M>
  struct is_stl_map_type<std::unordered_map<N, M>> { static const int value = 1; };



  // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :) 
  template<class Set>
  inline bool test(const Set& _set, const uint32_t index)
  {
    static_assert(is_stl_set_type<Set>::value);
    return contains(_set, index);
  }
  template<class T>
  inline bool test(const std::iterable_bitset<T>& _set, const uint32_t index)
  { return _set.test(index); }
  // why oh why, STL, do I have to declare those two as well, even though both are iterable_bitset's???
  inline bool test(const std::ordered_bitset& _set, const uint32_t index)
  { return _set.test(index); }
  inline bool test(const std::unordered_bitset& _set, const uint32_t index)
  { return _set.test(index); }

  template<class T>
  inline void flip(std::iterable_bitset<T>& _set, const uint32_t index)
  {
    return _set.flip(index);
  }

  template<class Set>
  inline void flip(Set& _set, const uint32_t index)
  {
    static_assert(is_stl_set_type<Set>::value);
    const auto emp_res = _set.emplace(index);
    if(!emp_res.second) _set.erase(emp_res.first);
  }

  template<class T>
  inline void intersect(std::iterable_bitset<T>& target, const std::iterable_bitset<T>& source)
  {
    target &= source;
  }
  template<class Set>
  inline void intersect(Set& target, const std::unordered_set<uint32_t>& source)
  {
    static_assert(is_stl_set_type<Set>::value);
    for(auto target_it = target.begin(); target_it != target.end();)
      if(!contains(source, *target_it))
        target_it = target.erase(target_it); 
      else ++target_it;
  }

  template<class T>
  inline const typename T::const_reference front(const T& _set)
  {
    assert(!_set.empty());
    return *(_set.begin());
  }

  template<class T>
  inline const typename T::const_reference back(const T& _set)
  {
    assert(!_set.empty());
    return *(_set.rbegin());
  }

  template<class T>
  using emplace_result = std::pair<typename T::iterator, bool>;
  template<class T>
  using const_emplace_result = std::pair<typename T::const_iterator, bool>;

  template<class Set, typename ...Args>
  inline emplace_result<Set> append(Set& _set, Args... args)
  {
    static_assert(is_stl_set_type<Set>::value);
    return _set.emplace(args...);
  }
  template<typename T, typename ...Args>
  inline emplace_result<std::vector<T>> append(std::vector<T>& _vec, Args... args) { return {_vec.emplace(_vec.end(), args...), true}; }
  template<typename T, typename ...Args>
  inline emplace_result<std::vector_list<T>> append(std::vector_list<T>& _vec, Args... args) { return {_vec.emplace(_vec.end(), args...), true}; }

  // this lets us append_to_map() for map<int, int> for example
  template<class T>
  inline std::pair<T*, bool> append(T& x, const T& y)
  {
    x = y;
    return {&x, true};
  }

  // this lets us append_to_map() for map<int, int> for example
  template<class T>
  inline std::pair<T*, bool> append(std::reference_wrapper<T>& x, const T& y)
  {
    x = y;
    return {&x, true};
  }
  template<class T, class Q>
  inline std::pair<std::pair<T,Q>*, bool> append(std::pair<T,Q>& x) { return {&x, false};}

  // append to a map:
  // return an iterator to the key-value pair in the map holding the key to which we appended
  // return a bool that is true if and only if an insertion _into_the_map_ (!!!) took place
  //    in particular this is independant of whether the insertion into the mapped object succeeded or not
  template<class Map, typename ...Args>
  inline emplace_result<Map> append_to_map(Map& _map, const typename Map::key_type& _key, Args... args)
  {
    //static_assert(is_stl_map_type<Map>::value);
    auto key_it = _map.find(_key);
    if(key_it != _map.end()){
      // we ignore the result of appending into the mapped object...
      append(key_it->second, args...);
      // ... and just return that no insertion into the map took place
      return {key_it, false};
    } else {
      return {_map.emplace_hint(key_it, PWC, std::make_tuple(_key), std::make_tuple(args...)), true};
    }
  }

  template<class T, class Q, typename ...Args>
  inline emplace_result<std::unordered_map<T,Q>> append(std::unordered_map<T,Q>& _map, const T& _key, const Args... args)
  {
    return append_to_map(_map, _key, args...);
  }
  template<class T, class Q, typename ...Args>
  inline emplace_result<std::map<T,Q>> append(std::map<T,Q>& _map, const T& _key, Args... args)
  {
    return append_to_map(_map, _key, args...);
  }
  template<class Q, typename ...Args>
  inline emplace_result<std::vector_map<Q>> append(std::vector_map<Q>& _map, const typename std::vector_map<Q>::key_type& _key, Args... args)
  {
    return append_to_map(_map, _key, args...);
  }

  template<class _Container>
  inline void append(_Container& x, const _Container& y)
  {
    x.insert(y.begin(), y.end());
  }
  template<class T>
  inline void append(std::vector<T>& x, const std::vector<T>& y)
  {
    x.insert(x.end(), y.begin(), y.end());
  }




  // I would write an operator= to assign unordered_set<uint32_t> from iterable_bitset, but C++ forbids operator= as free function... WHY?!?!
  template<class T>
  inline std::iterable_bitset<T>& copy(const std::unordered_set<uint32_t>& x, std::iterable_bitset<T>& y)
  {
    y.clear();
    y.insert(x.begin(), x.end());
    return y;
  }
  template<class T, class Set>
  inline std::unordered_set<uint32_t>& copy(const std::iterable_bitset<T>& x, Set& y)
  {
    static_assert(is_stl_set_type<Set>::value);
    y.clear();
    y.insert(x.begin(), x.end());
    return y;
  }
  template<class T>
  inline std::vector<uint32_t>& copy(const std::iterable_bitset<T>& x, std::vector<uint32_t>& y)
  {
    y.clear();
    y.insert(y.begin(), x.begin(), x.end());
    //auto x_it = x.begin();
    //while(x_it) y.emplace_back(*x_it++);
    return y;
  }
  template<class T1, class T2>
  inline std::iterable_bitset<T2>& copy(const std::iterable_bitset<T1>& x, std::iterable_bitset<T2>& y)
  {
    return y = x;
  }
  template<class Set>
  inline Set& copy(const Set& x, Set& y)
  {
    static_assert(is_stl_set_type<Set>::value);
    return y = x;
  }

  template<class Set>
  typename Set::value_type pop_front(Set& s){
    static_assert(is_stl_set_type<Set>::value);
    typename Set::value_type t = *(s.begin());
    s.erase(s.begin());
    return t;
  }

  template<class T>
  std::unordered_set<uint32_t> to_set(const std::iterable_bitset<T>& x)
  {
    std::unordered_set<uint32_t> result;
    return copy(x, result);
  }

}
