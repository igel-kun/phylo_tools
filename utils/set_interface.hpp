
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "iter_bitset.hpp"

// unifcation for the set interface
// this is to use unordered_set<uint32_t> with the same interface as iterable_bitset

#pragma once

namespace std {

  template<class T>
  struct is_vector: public false_type {};
  template<class T, class Alloc>
  struct is_vector<vector<T, Alloc>>: public true_type {};
  template<class T>
  static constexpr bool is_vector_v = is_vector<T>::value;

  //a container is something we can iterate through, that is, it has an iterator
  template<class N, class T = void>
  struct is_container: public false_type {};
  template<class N>
  struct is_container<N, void_t<typename N::value_type>>: public true_type {};
  template<class N>
  static constexpr bool is_container_v = is_container<remove_reference_t<N>>::value;
  
  // a map is something with a mapped_type in it
  template<class N, class T = void>
  struct is_map: public false_type {};
  template<class N>
  struct is_map<N, void_t<typename N::mapped_type>>: public true_type {};
  template<class N>
  static constexpr bool is_map_v = is_map<remove_reference_t<N>>::value;



  // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :) 
  template<class Set>
  inline void flip(Set& _set, const typename Set::reference index)
  {
    const auto emp_res = _set.emplace(index);
    if(!emp_res.second) _set.erase(emp_res.first);
  }
  template<class T>
  inline void flip(iterable_bitset<T>& _set, const uintptr_t index) { return _set.flip(index); }

  template<class Set, class Other>
  inline void intersect(Set& target, const Other& source)
  {
    static_assert(is_container<Set>::value);
    for(auto target_it = target.begin(); target_it != target.end();)
      if(!contains(source, *target_it))
        target_it = target.erase(target_it); 
      else ++target_it;
  }
  template<class T>
  inline void intersect(iterable_bitset<T>& target, const iterable_bitset<T>& source) { target &= source; }

  template<class T>
  inline typename iterator_traits<IteratorOf_t<T>>::reference front(T& _set)
  {
    assert(!_set.empty());
    return *(_set.begin());
  }

  template<class T>
  inline typename iterator_traits<IteratorOf_t<T>>::reference back(T& _set)
  {
    assert(!_set.empty());
    return *(_set.rbegin());
  }


  template<class T>
  using emplace_result = pair<typename T::iterator, bool>;

  // on non-map containers, append = emplace
  template<class Set, enable_if_t<is_container_v<Set> && !is_map_v<Set> && !is_vector_v<Set>, int> = 0, class ...Args>
  inline emplace_result<Set> append(Set& _set, Args&&... args) { return _set.emplace(forward<Args>(args)...); }
  // on vectors, append =  emplace_back
  template<typename T, class ...Args>
  inline emplace_result<vector<T>> append(vector<T>& _vec, Args&&... args) { return {_vec.emplace(_vec.end(), forward<Args>(args)...), true}; }
  // on maps to primitives, append = try_emplace
  template<class Map, enable_if_t<!is_container_v<typename Map::mapped_type>, int> = 0, class ...Args>
  inline emplace_result<Map> append(Map& _map, const typename Map::key_type& _key, Args&&... args)
  {
    return _map.try_emplace(_key, std::forward<Args>(args)...);
  }
  // on maps to sets, append = append to set at map[key]
  template<class Map, enable_if_t<is_container_v<typename Map::mapped_type>, int> = 0, class ...Args>
  inline emplace_result<Map> append(Map& _map, const typename Map::key_type& _key, Args&&... args)
  {
    return append(_map.try_emplace(_key).first.second, forward<Args>(args)...);
  }
  // append with 2 containers means to add the second to the end of the first
  template<class _Container>
  inline void append(_Container& x, const _Container& y)
  {
    x.insert(y.begin(), y.end());
  }
  template<class T>
  inline void append(vector<T>& x, const vector<T>& y)
  {
    x.insert(x.end(), y.begin(), y.end());
  }


  // all this BS with containers supporting contains() or not is really unnerving
  template<class T>
  inline bool test(const T& _set, const typename T::value_type& key) { return _set.find(key) != _set.end(); }
  template<class T, enable_if_t<is_map_v<T>, int> = 0>
  inline bool test(const T& _map, const typename T::key_type& key) { return _map.find(key) != _map.end(); }
  template<class T>
  inline bool test(const iterable_bitset<T>& bs, const uintptr_t key) { return bs.test(key); }

  // I would write an operator= to assign unordered_set<uint32_t> from iterable_bitset, but C++ forbids operator= as free function... WHY?!?!
  template<class T>
  inline iterable_bitset<T>& copy(const unordered_set<uint32_t>& x, iterable_bitset<T>& y)
  {
    y.clear();
    y.insert(x.begin(), x.end());
    return y;
  }
  template<class T, class Set>
  inline unordered_set<uint32_t>& copy(const iterable_bitset<T>& x, Set& y)
  {
    static_assert(is_container<Set>::value);
    y.clear();
    y.insert(x.begin(), x.end());
    return y;
  }
  template<class T>
  inline vector<uint32_t>& copy(const iterable_bitset<T>& x, vector<uint32_t>& y)
  {
    y.clear();
    y.insert(y.begin(), x.begin(), x.end());
    //auto x_it = x.begin();
    //while(x_it) y.emplace_back(*x_it++);
    return y;
  }
  template<class T1, class T2>
  inline iterable_bitset<T2>& copy(const iterable_bitset<T1>& x, iterable_bitset<T2>& y)
  {
    return y = x;
  }
  template<class Set>
  inline Set& copy(const Set& x, Set& y)
  {
    static_assert(is_container<Set>::value);
    return y = x;
  }

  template<class Set>
  typename Set::value_type pop_front(Set& s){
    static_assert(is_container<Set>::value);
    typename Set::value_type t = *(s.begin());
    s.erase(s.begin());
    return t;
  }

  template<class T>
  unordered_set<uint32_t> to_set(const iterable_bitset<T>& x)
  {
    unordered_set<uint32_t> result;
    return copy(x, result);
  }


  // a set holding exactly one item, but having a set-interface
  template<class _Element>
  class SingletonSet
  {
    _Element item;

  public:
    using iterator = _Element*;
    using const_iterator = const _Element*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    using value_type = _Element;
    using reference = value_type&;
    using const_reference = const reference;

    SingletonSet() = delete;
    SingletonSet(_Element& _item): item(_item) {}

    size_t size() const { return 1; }
    bool empty() const { return false; }
    reference       front() { return *item; }
    const_reference front() const { return *item; }
    iterator       find(const _Element& x) { return ((x == *item) ? begin() : end()); }
    const_iterator find(const _Element& x) const { return ((x == *item) ? begin() : end()); }
    
    iterator begin() { return &item; }
    const_iterator begin() const { return &item; }
    reverse_iterator rbegin() { return &item; }
    const_reverse_iterator rbegin() const { return &item; }
    iterator end() { return begin() + 1; }
    const_iterator end() const { return begin() + 1; }
    reverse_iterator rend() { return begin() + 1; }
    const_reverse_iterator rend() const { return begin() + 1; }
  };
}
