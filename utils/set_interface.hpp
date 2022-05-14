
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

  template<SetType Set, class ValueType = value_type_of_t<Set>>
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
        my_erase(target, x);
    } else erase_if(target, [&source](const auto& x) { return !test(source, x); });
  }
  // intersect a set with a list/vector/etc
  template<SetType Set, ContainerType Other>
  void intersect(Set& target, const Other& source) {
    for(const auto& x: source) my_erase(target, x);
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
  template<class T, SetType S> requires (!is_convertible_v<S,singleton_set<value_type_of_t<S>>>)
  bool are_disjoint(const S& y, const singleton_set<T>& x) { return are_disjoint(x, y); }

  template<SetType S1, SetType S2 = S1> requires is_convertible_v<typename S2::const_iterator, typename S1::const_iterator>
  auto_iter<typename S1::const_iterator> common_element(const S1& x, const S2& y)
  {
    auto_iter<typename S1::const_iterator> result;
    auto& iter = result.first;

    if(x.size() < y.size()){
      for(iter = begin(x); iter != end(x); ++iter)
        if(test(y, *iter)) break;
    } else {
      for(const auto& element: y)
        if((result = find(x, element)) != x.end()) break;
    }
    return result;
  }

  template<class T, SetType S>
  auto_iter<typename singleton_set<T>::const_iterator> common_element(const singleton_set<T>& x, const S& y) {
    if(!x.empty() && test(y,front(x)))
      return {begin(x), end(x)};
    else
      return {end(x), end(x)};
  }

  template<class T, SetType S> requires (!is_convertible_v<S,singleton_set<value_type_of_t<S>>>)
  auto_iter<typename S::const_iterator> common_element(const S& y, const singleton_set<T>& x) {
    if(!x.empty())
      return {find(y, front(x)), end(y)};
    else
      return {end(y), end(y)};
  }


  template<IterableType T>
  constexpr reference_of_t<T> front(T&& c) { assert(!c.empty()); return *(c.begin()); }
  template<IterableType T>
  constexpr reference_of_t<T> next_to_front(T&& c) { assert(!c.empty()); return *(next(c.begin())); }
  template<IterableType T>
  constexpr reference_of_t<T> back(T&& c) { assert(!c.empty()); return *(c.rbegin()); }
  template<IterableType T>
  constexpr reference_of_t<T> next_to_back(T&& c) { assert(!c.empty()); return *(next(c.rbegin())); }

  // unordered_set has no rbegin(), so we just alias it to begin()
  template<class Key, class Hash, class KE, class A>
  auto rbegin(const unordered_set<Key, Hash, KE, A>& s) { return s.begin(); }
  template<class Key, class Hash, class KE, class A>
  auto rbegin(unordered_set<Key, Hash, KE, A>& s) { return s.begin(); }
  template<class Key, class Hash, class KE, class A>
  auto rend(const unordered_set<Key, Hash, KE, A>& s) { return s.end(); }
  template<class Key, class Hash, class KE, class A>
  auto rbend(unordered_set<Key, Hash, KE, A>& s) { return s.end(); }

  template<class T> concept StrictSettableType = requires(T t, T::value_type x) { { t.set(x) } -> convertible_to<bool>; };
  template<class T> concept SettableType = StrictSettableType<std::remove_cvref_t<T>>;

  // on callables, append will call the function and return the result
  template<class T, invocable<T&&> F>
  auto append(F&& f, T&& t) { return forward<F>(f)(forward<T>(t)); }

  // on non-map containers, append = emplace
  template<IterableType C, class First, class ...Args> requires (!MapType<C> && !VectorType<C> && !CompatibleValueTypes<C, First>)
  auto append(C& container, First&& first, Args&&... args) { return container.emplace(forward<First>(first), forward<Args>(args)...); }

  // on vectors, append =  emplace_back 
  // this is bad: vector_map<> can be "upcast" to vector<> so this will always conflict with the append for maps
  // the suggestion on stackoverflow is "stop spitting against the wind"... :(
  // so for now, I'm using try_emplace() in all places that would be ambiguous
  template<VectorType V, class First, class... Args> requires (!ConvertibleValueTypes<V, First>)
  auto append(V& _vec, First&& first, Args&&... args) { 
    return emplace_result<V>{_vec.emplace(_vec.end(), forward<First>(first), forward<Args>(args)...), true};
  }
  // dummy function to not insert anything into an appended vector
  template<VectorType V>
  auto append(V&& _vec) { return emplace_result<V>{_vec.begin(), true}; }
 
  // on maps to primitives, append = try_emplace
  //NOTE: this can be used also if mapped_type is NOT a primitive, but no initialization arguments have been given
  template<MapType M, class Key, class... Args>
    requires (is_convertible_v<Key, key_type_of_t<M>> && !(ContainerType<mapped_type_of_t<M>> && (sizeof...(Args) > 0)))
  auto append(M& _map, Key&& _key, Args&&... args)
  { return _map.try_emplace(forward<Key>(_key), forward<Args>(args)...); }

  // on maps to containers, append = append to container at map[key]
  //NOTE: return an iterator to the pair in the map whose second now contains the newly constructed item
  //      also return a bool indicating whether insertion took place
  //NOTE: this also works for emplacing a string into a map that maps to strings, the "inserting" appends below are called in this case
  template<MapType M, class Key, class ...Args>
    requires (is_convertible_v<Key, key_type_of_t<M>> && ContainerType<mapped_type_of_t<M>> && (sizeof...(Args) > 0))
  auto append(M& _map, Key&& _key, Args&&... args) {
    const auto iter = _map.try_emplace(forward<Key>(_key)).first;
    const bool success = append(iter->second, forward<Args>(args)...).second;
    return emplace_result<M>{iter, success};
  }

  // append with 2 containers means to add the second to the end of the first
  template<ContainerType C1, IterableType C2> requires (ConvertibleValueTypes<C1,C2> && !IterableTypeWithSameIterators<C2>)
  auto append(C1& x, C2&& y) {
    using ItemRef = copy_cvref_t<C2&&, value_type_of_t<C2>>;
    for(ItemRef i: std::forward<C2>(y))
      append(x, static_cast<ItemRef>(i)); // so, right, I know i should already have that type, but not if ItemRef is an rvalue-reference because... reasons
    return emplace_result<C1>{x.begin(), true};
  }

  template<ContainerType C1, IterableTypeWithSameIterators C2> requires (ConvertibleValueTypes<C1,C2> && !VectorType<C1>)
  auto append(C1& x, C2&& y) {
    x.insert(y.begin(), y.end());
    return emplace_result<C1>{x.begin(), true};
  }
  template<VectorType V, IterableTypeWithSameIterators C2> requires ConvertibleValueTypes<V,C2>
  auto append(V& x, const C2& y) {
    x.insert(x.end(), y.begin(), y.end());
    return emplace_result<V>{x.begin(), true};
  }

  // if we're not interested in the return value, we can set values more efficiently
  template<class S> requires (ContainerType<S> && !SettableType<S>)
  bool set_val(S& s, const auto& val) { return append(s, val).second; }
  template<class S> requires (ContainerType<S> && SettableType<S>)
  bool set_val(S& s, const auto& val) { return s.set(val); }
#warning "TODO: replace append into sets by set_val unless we need the iterator"
  // test if something is in the set
  template<SetType S>
  bool test(const S& _set, const value_type_of_t<S>& key) { return _set.count(key); }
  template<MapType M>
  bool test(const M& _map, const key_type_of_t<M>& key) { return _map.count(key); }
  template<VectorType V>
  bool test(const V& vec, const auto& key) { return find(vec, key) != end(vec); }
  template<class T>
  bool test(const T& x, const T& y) { return x == y; }

  // I would write an operator= to assign unordered_set<uint32_t> from iterable_bitset, but C++ forbids operator= as free function... WHY?!?!
#warning "TODO: this can be done by writing a manual conversion to unordered_set<uint32_t>"
  template<SetType C1, SetType C2> requires is_convertible_v<value_type_of_t<C1>, value_type_of_t<C2>>
  C2& copy(const C1& x, C2& y) {
    y.clear();
    std::copy(x.begin(), x.end(), std::insert_iterator<C2>(y, y.begin()));
    return y;
  }

  template<class T, ContainerType C = unordered_set<T>>
  C to_set(const iterable_bitset<T>& x, const C& sentinel = C()) {
    C result;
    return copy(x, result);
  }

  //! a hash computation for a set, XORing its members
  template<IterableType C>
  struct set_hash {
    static constexpr std::hash<value_type_of_t<C>> Hasher{};
    size_t operator()(const C& container) const {
      return accumulate(begin(container), end(container), size_t(0), [](const size_t x, const auto& y) { return x ^ Hasher(y); });
    }
  };
  // iterable bitset can be hashed faster
  template<> struct set_hash<ordered_bitset>: public hash<ordered_bitset> {};
  template<> struct set_hash<unordered_bitset>: public hash<unordered_bitset> {};

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<IterableType C>
  struct list_hash {
    static constexpr std::hash<value_type_of_t<C>> Hasher{};
    size_t operator()(const C& container) const {
      return accumulate(begin(container), end(container), size_t(0), [](const size_t x, const auto& y) { return rotl(x,1) ^ Hasher(y); });
    }
  };


  template<ContainerType C, FindableType<C> Key>
  auto find(C&& c, const Key& key) {
    if constexpr (VectorType<C>)
      return std::find(begin(c), end(c), key);
    else return c.find(key);
  }

  template<ContainerType C, class Key> requires (!IterableType<Key>)
  auto my_erase(C& c, const Key& key) {
    if constexpr (is_same_v<remove_cvref_t<Key>, iterator_of_t<C>>)
      return c.erase(key);
    else if constexpr (is_same_v<remove_cvref_t<Key>, const_iterator_of_t<C>>)
      return c.erase(key);
    else if constexpr (VectorType<C>)
      return c.erase(find(c, key));
    else return c.erase(key);
  }
  template<ContainerType C, IterableType Keys>
  void erase_by_iterating_keys(C& c, const Keys& keys) {
    for(const auto& key: keys) my_erase(c, key);
  }
  template<ContainerType C, IterableType Keys>
  void erase_by_iterating_container(C& c, const Keys& keys) {
    for(auto iter = begin(c); iter != end(c);)
      if(test(keys, *iter))
        iter = my_erase(c, iter);
      else ++iter;
  }
  template<ContainerType C, IterableType Keys>
  void erase_by_moving(C& c, const Keys& keys) {
    C output;
    const size_t c_size = c.size();
    const size_t k_size = keys.size();
    if(k_size < c_size) output.reserve(c_size - k_size);
    for(auto& x: c)
      if(!test(keys, x))
        output.emplace_back(std::move(x));
    c = std::move(output);
  }

  template<ContainerType C, IterableType Keys>
  void my_erase(C& c, const Keys& keys) {
    if constexpr (SetType<Keys>) {
      if constexpr (!VectorType<C>) {
        if(c.size() > keys.size()) {
          erase_by_iterating_keys(c, keys);
        } else erase_by_iterating_container(c, keys);
      } else erase_by_moving(c, keys);
    } else erase_by_iterating_keys(c, keys);
  }

  template<ContainerType C>
  void pop(C& c) {
    assert(!c.empty());
    c.erase(rbegin(c));
  }

  // value-moving pop operations
  template<QueueType Q>
  auto value_pop(Q& q) {
    using value_type = Q::value_type;
    value_type result = move(q.top());
    q.pop();
    return result;
  }
  template<VectorType Q>
  auto value_pop(Q& q) {
    auto result = move(q.back());
    q.pop_back();
    return result;
  }
  template<ContainerType Q> requires (!VectorType<Q>)
  auto value_pop(Q& q) {
    const auto iter = q.begin();
    value_type_of_t<Q> result = move(*iter);
    my_erase(q, iter);
    return result;
  }

  template<ContainerType C>
  auto value_pop(C& q, const iterator_of_t<const C>& iter) {
    if(iter != end(q)) {
      auto result = move(*iter);
      q.erase(iter);
      return result;
    } else throw std::out_of_range("trying to pop values beyond the end");
  }

  template<IterableType C, class Key> requires requires(C c, Key k) { { std::find(c, k) } -> std::convertible_to<iterator_of_t<const C>>; }
  auto value_pop(C& q, const Key& key) {
    return value_pop(q, find(q, key));
  }

  template<ContainerType Q> requires requires(Q q) { { front(q) } -> convertible_to<value_type_of_t<Q>>; }
  auto value_pop_front(Q& q) {
    assert(!q.empty());
    const auto it = std::begin(q);
    auto v = move(*it);
    q.erase(it);
    return v;
  }
  template<IterableType Q> requires requires(Q q) { { back(q) } -> convertible_to<value_type_of_t<Q>>; }
  auto value_pop_back(Q& q) {
    assert(!q.empty());
    const auto it = std::prev(std::end(q));
    auto v = move(*it);
    q.erase(it);
    return v;
  }



  template<SetType S, class Val = value_type_of_t<S>> requires (!MapType<S> && !SingletonSetType<S>)
  void replace(S& s, const auto& _old_it, Val&& _new) {
    auto node = s.extract(_old_it);
    node.value() = forward<Val>(_new);
    s.insert(move(node));
  }

  template<SingletonSetType S, class Val = value_type_of_t<S>>
  void replace(S& s, const auto& _old_it, Val&& _new) {
    assert(_old_it == s.begin());
    s.clear();
    s.emplace_back(forward<Val>(_new));
  }

  template<MapType M, class Key = key_type_of_t<M>>
  void replace(M& m, const auto& _old_it, Key&& _new) {
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

  template<ContainerType C, class Val = value_type_of_t<C>>
  bool replace(C& c, const value_type_of_t<C>& _old, Val&& _new) {
    const auto iter = find(c, _old);
    if(iter != end(c)) {
      replace(c, iter, forward<Val>(_new));
      return true;
    } else return false;
  }

  template<IterableType C>
  iterator_of_t<const C> max_element(const C& c) { return max_element(begin(c), end(c)); }


  // clear a container except for 1 item
  template<ContainerType C, class Iter>
  void clear_except(C& c, const Iter& except_iter) {
    if(except_iter != end(c)) {
      if(c.size() > 1) {
        auto tmp = std::move(*except_iter);
        c.clear();
        append(c, std::move(tmp));
      } else {
        assert(except_iter == begin(c));
      }
    } else c.clear();
  }

  // a modification of a set that automatically clears the other set on move-construction or move-assignment
  template<SetType S>
  struct auto_clearing: public S {
    using S::S;

    auto_clearing() = default;
    auto_clearing(const auto_clearing&) = default;
    auto_clearing(auto_clearing&& other):
      S(move(other))
    { other.clear(); }

    auto_clearing& operator=(const auto_clearing& other) = default;
    auto_clearing& operator=(auto_clearing&& other) {
      S::operator=(move(other));
      other.clear();
    }

  };


}
