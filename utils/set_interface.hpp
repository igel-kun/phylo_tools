
#pragma once

#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <type_traits>
#include <numeric>
#include "auto_iter.hpp"
#include "iter_bitset.hpp"
#include "singleton.hpp"
#include "stl_utils.hpp"
// unifcation for the set interface
// this is to use unordered_set<uint32_t> with the same interface as iterable_bitset

namespace std { // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :) 
#warning TODO: write a back() function (doing front() for unordered containers)

  template<class T>
  using emplace_result = pair<typename remove_reference_t<T>::iterator, bool>;

  template<SetType Set, class ValueType = typename Set::value_type>
  void flip(Set& _set, const ValueType& index) {
    const auto [iter, success] = _set.emplace(index);
    if(!success) _set.erase(iter);
  }
  template<class T>
  void flip(iterable_bitset<T>& _set, const uintptr_t index) { return _set.flip(index); }

  // intersect 2 sets
  template<SetType Set, SetType Other>
  void intersect(Set& target, const Other& source) {
    if(target.size() > source.size()) {
      for(const auto& x: source)
        target.erase(x);
    } else erase_if(target, [&source](const auto& x) { return !test(source, x); });
  }
  // intersect a set with a list/vector/etc
  template<SetType Set, ContainerType Other>
  void intersect(Set& target, const Other& source) {
    for(const auto& x: source) target.erase(x);
  }
  template<SetType S, class T>
  void intersect(singleton_set<T>& target, const S& source) {
    if(!target.empty() && !test(source, front(target)))
      target.clear();
  }
  // intersect 2 bitsets
  template<class T>
  void intersect(iterable_bitset<T>& target, const iterable_bitset<T>& source) { target &= source; }


  template<SetType S1, SetType S2 = S1>
  bool are_disjoint(const S1& x, const S2& y) {
    if(x.size() < y.size()){
      for(const auto& item: x) if(test(y, item)) return false;
    } else {
      for(const auto& item: y) if(test(x, item)) return false;
    }
    return true;
  }
  template<class T, SetType S>
  bool are_disjoint(const singleton_set<T>& x, const S& y) { return x.empty() ? true : test(y,front(x)); }
  template<class T, SetType S> requires (!is_convertible_v<S,singleton_set<typename S::value_type>>)
  bool are_disjoint(const S& y, const singleton_set<T>& x) { return are_disjoint(x, y); }

  template<SetType S1, SetType S2 = S1> requires is_convertible_v<typename S2::const_iterator, typename S1::const_iterator>
  auto_iter<typename S1::const_iterator> common_element(const S1& x, const S2& y)
  {
    typename S1::const_iterator result;
    if(x.size() < y.size()){
      for(const auto& element: x)
        if((result = find(y, element)) != y.end())
          return {result, y.end()};
    } else {
      for(const auto& element: y)
        if((result = find(x, element)) != x.end())
          return {result, x.end()};
    }
    return {x.end(), x.end()};
  }
  template<class T, SetType S>
  auto_iter<typename singleton_set<T>::const_iterator> common_element(const singleton_set<T>& x, const S& y) {
    if(!x.empty() && test(y,front(x)))
      return {begin(x), end(x)};
    else
      return {end(x), end(x)};
  }
  template<class T, SetType S> requires (!is_convertible_v<S,singleton_set<typename S::value_type>>)
  auto_iter<typename S::const_iterator> common_element(const S& y, const singleton_set<T>& x) {
    if(!x.empty())
      return {find(y, front(x)), end(y)};
    else
      return {end(y), end(y)};
  }


  template<class T> requires ContainerType<std::remove_cvref_t<T>>
  constexpr auto& front(T&& c)
  {
    assert(!c.empty());
    return *(c.begin());
  }

  template<class T> requires ContainerType<std::remove_cvref_t<T>>
  constexpr auto& back(T&& c)
  {
    assert(!c.empty());
    return *(c.rbegin());
  }

  // unordered_set has no rbegin(), so we just alias it to begin()
  template<class Key, class Hash, class KE, class A>
  auto rbegin(const unordered_set<Key, Hash, KE, A>& s) { return s.begin(); }
  template<class Key, class Hash, class KE, class A>
  auto rbegin(unordered_set<Key, Hash, KE, A>& s) { return s.begin(); }
  template<class Key, class Hash, class KE, class A>
  auto rend(const unordered_set<Key, Hash, KE, A>& s) { return s.end(); }
  template<class Key, class Hash, class KE, class A>
  auto rbend(unordered_set<Key, Hash, KE, A>& s) { return s.end(); }


  // on non-map containers, append = emplace
  template<ContainerType C, class ...Args> requires (!MapType<C> && !VectorType<C>)
  emplace_result<C> append(C& container, Args&&... args) { return container.emplace(forward<Args>(args)...); }

  // on vectors, append =  emplace_back 
  // this is bad: vector_map<> can be "upcast" to vector<> so this will always conflict with the append for maps
  // the suggestion on stackoverflow is "stop spitting against the wind"... :(
  // so for now, I'm using try_emplace() in all places that would be ambiguous
  template<class T, class A, class First, class ...Args> requires (!ContainerType<First>)
  emplace_result<vector<T, A>> append(vector<T, A>& _vec, First&& first, Args&&... args)
  { 
    return {_vec.emplace(_vec.end(), forward<First>(first), forward<Args>(args)...), true};
  }
  // dummy function to not insert anything into an appended vector
  template<class T, class A>
  emplace_result<vector<T, A>> append(vector<T, A>& _vec) { return {_vec.begin(), true}; }
 
  // on maps to primitives, append = try_emplace
  template<MapType M, class ...Args> requires (!ContainerType<typename M::mapped_type>)
  emplace_result<M> append(M& _map, const typename M::key_type& _key, Args&&... args)
  { return _map.try_emplace(_key, std::forward<Args>(args)...); }

  // on maps to sets, append = append to set at map[key]
  //NOTE: return an iterator to the pair in the map whose second now contains the newly constructed item
  //      also return a bool indicating whether insertion took place
  //NOTE: this also works for emplacing a string into a map that maps to strings, the "inserting" appends below are called in this case
  template<MapType M, class ...Args> requires ContainerType<typename M::mapped_type>
  emplace_result<M> append(M& _map, const typename M::key_type& _key, Args&&... args)
  {
    const auto iter = _map.try_emplace(_key).first;
    const bool success = append(iter->second, forward<Args>(args)...).second;
    return {iter, success};
  }

  // append with 2 containers means to add the second to the end of the first
  template<ContainerType C1, ContainerType C2> requires convertible_to<typename C2::value_type, typename C1::value_type>
  emplace_result<C1> append(C1& x, const C2& y)
  {
    x.insert(y.begin(), y.end());
    return {x.begin(), true};
  }
  template<class T, class A, ContainerType C2> requires convertible_to<typename C2::value_type, T>
  emplace_result<std::vector<T,A>> append(std::vector<T,A>& x, const C2& y)
  {
    x.insert(x.end(), y.begin(), y.end());
    return {x.begin(), true};
  }

  // all this BS with containers supporting contains() or not is really unnerving
  template<class T>
  bool test(const T& _set, const typename T::value_type& key) { return _set.count(key); }
  template<MapType T>
  bool test(const T& _map, const typename T::key_type& key) { return _map.count(key); }



  // I would write an operator= to assign unordered_set<uint32_t> from iterable_bitset, but C++ forbids operator= as free function... WHY?!?!
  template<SetType C1, SetType C2> requires is_convertible_v<typename C1::value_type, typename C2::value_type>
  C2& copy(const C1& x, C2& y) {
    y.clear();
    std::copy(x.begin(), x.end(), std::insert_iterator<C2>(y, y.begin()));
    return y;
  }

  // value-copying pop operations
  template<QueueType Q>
  typename Q::value_type value_pop(Q& q) {
    typename Q::value_type result = move(q.top());
    q.pop();
    return result;
  }
  template<ContainerType C>
  typename C::value_type value_pop(C& q, const typename C::const_iterator& iter) {
    if(iter != end(q)) {
      typename C::value_type result = move(*iter);
      erase(q, iter);
      return result;
    } else throw std::out_of_range();
  }

  template<ContainerType C>
  typename C::value_type value_pop(C& q, const auto& key) {
    return value_pop(q, find(q, key));
  }

  template<ContainerType Q> requires requires(Q q) { { front(q) } -> convertible_to<typename Q::value_type>; }
  typename Q::value_type value_pop_front(Q& q)
  {
    assert(!q.empty());
    const auto it = std::begin(q);
    typename Q::value_type v = move(*it);
    q.erase(it);
    return v;
  }
  template<ContainerType Q> requires requires(Q q) { { back(q) } -> convertible_to<typename Q::value_type>; }
  typename Q::value_type value_pop_back(Q& q)
  {
    assert(!q.empty());
    const auto it = std::end(q);
    typename Q::value_type v = move(*it);
    q.erase(it);
    return v;
  }

  template<class T, ContainerType C = unordered_set<typename iterable_bitset<T>::value_type>>
  C to_set(const iterable_bitset<T>& x, const C& sentinel)
  {
    C result;
    return copy(x, result);
  }

  //! a hash computation for a set, XORing its members
  template<ContainerType C>
  struct set_hash {
    static constexpr std::hash<typename C::value_type> Hasher{};
    size_t operator()(const C& container) const
    {
      return accumulate(begin(container), end(container), size_t(0), [](const auto& x, const size_t y) { return y ^ Hasher(x); });
    }
  };
  // iterable bitset can be hashed faster
  template<> struct set_hash<ordered_bitset>: public hash<ordered_bitset> {};
  template<> struct set_hash<unordered_bitset>: public hash<unordered_bitset> {};

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<ContainerType C>
  struct list_hash {
    static constexpr std::hash<typename C::value_type> Hasher{};
    size_t operator()(const C& container) const
    {
      return accumulate(begin(container), end(container), size_t(0), [](const auto& x, const size_t y) { return rotl(y,1) ^ Hasher(x); });
    }
  };


  template<VectorType V> auto find(V&& v, const auto& x) { return std::find(begin(v), end(v), x); }
  template<SetType S>    auto find(S&& s, const auto& key) { return s.find(key); }
  template<MapType M>    auto find(M&& m, const auto& key) { return m.find(key); }

  template<VectorType V> auto erase(V& v, const typename remove_reference_t<V>::const_iterator& _iter) { return v.erase(_iter); }
  template<VectorType V> auto erase(V& v, const typename remove_reference_t<V>::value_type& x) { return v.erase(find(v,x)); }
  template<SetType S>    auto erase(S& s, const typename remove_reference_t<S>::const_iterator& _iter) { return s.erase(_iter); }
  template<SetType S>    auto erase(S& s, const typename remove_reference_t<S>::value_type& key) { return s.erase(key); }
  template<MapType M>    auto erase(M& m, const typename remove_reference_t<M>::const_iterator& _iter) { return m.erase(_iter); }
  template<MapType M>    auto erase(M& m, const typename remove_reference_t<M>::key_type& key) { return m.erase(key); }

  template<ContainerType C>
  void pop(C& c) {
    assert(!c.empty());
    erase(c, rbegin(c));
  }

  template<SetType S, class Val = typename S::value_type>
  void replace(S& s, const typename S::iterator& _old_it, Val&& _new) {
    auto node = s.extract(_old_it);
    node.value() = forward<Val>(_new);
    s.insert(move(node));
  }

  template<MapType M, class Key = typename M::key_type>
  void replace(M& m, const typename M::iterator& _old_it, Key&& _new) {
    auto node = m.extract(_old_it);
    node.key() = forward<Key>(_new);
    m.insert(move(node));
  }
  template<class T, class A>
  void replace(std::vector<T,A>& c, const typename std::vector<T,A>::iterator& _old_it, T&& _new) {
    *_old_it = forward<T>(_new);
  }
  template<class T>
  void replace(std::singleton_set<T>& c, const typename std::singleton_set<T>::iterator& _old_it, T&& _new) {
    *_old_it = forward<T>(_new);
  }

  template<ContainerType C, class Val = typename C::value_type>
  bool replace(C& c, const typename C::value_type& _old, Val&& _new) {
    const auto iter = find(c, _old);
    if(iter != end(c)) {
      replace(c, iter, forward<Val>(_new));
      return true;
    } else return false;
  }
}
