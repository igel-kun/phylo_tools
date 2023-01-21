
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
// this is to use std::unordered_set<uint32_t> with the same interface as iterable_bitset

namespace mstd { // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :) 
#warning TODO: write a back() function (doing front() for std::unordered containers)

  template<class T>
  using emplace_result = std::pair<typename std::remove_reference_t<T>::iterator, bool>;

  template<SetType Set, class ValueType = value_type_of_t<Set>>
  void flip(Set& _set, const ValueType& index) {
    const auto [iter, success] = _set.emplace(index);
    if(!success) _set.erase(iter);
  }
  template<class T>
  void flip(iterable_bitset<T>& _set, const uintptr_t index) { return _set.flip(index); }

  // intersect a set with a list/vector/etc
  template<SetType Set, ContainerType Other>
  void intersect(Set& target, const Other& source) {
    for(const auto& x: source) erase(target, x);
  }
  // intersect 2 sets
  template<SetType Set, ContainerType Other> requires SetType<Other>
  void intersect(Set& target, const Other& source) {
    if(target.size() > source.size()) {
      for(const auto& x: source)
        erase(target, x);
    } else erase(target, [&source](const auto& x) { return !test(source, x); });
  }
  template<SetType S, class T>
  void intersect(singleton_set<T>& target, const S& source) {
    if(!target.empty() && !test(source, front(target)))
      target.clear();
  }
  // intersect 2 bitsets
  template<ContainerType C> requires IterBitsetType<C>
  void intersect(C& target, const C& source) { target &= source; }

  template<IterableType I, ContainerType C>
  bool are_disjoint(const I& x, const C& y) {
    if constexpr (!SetType<C>) {
        for(const auto& item: y) if(test(x, item)) return false;
    } else if constexpr (!SetType<I>) {
        for(const auto& item: x) if(test(y, item)) return false;
    } else {
      if(x.size() < y.size()){
        for(const auto& item: x) if(test(y, item)) return false;
      } else {
        for(const auto& item: y) if(test(x, item)) return false;
      }
    }
    return true;
  }
  template<ContainerType C, IterableType I> requires (!ContainerType<I>)
  bool are_disjoint(const C& x, const I& y) { return are_disjoint(y,x); }

  template<class T, SetType S>
  bool are_disjoint(const singleton_set<T>& x, const S& y) { return x.empty() ? true : test(y, front(x)); }
  template<class T, SetType S> requires (!std::is_convertible_v<S, singleton_set<value_type_of_t<S>>>)
  bool are_disjoint(const S& y, const singleton_set<T>& x) { return are_disjoint(x, y); }


  template<ContainerType C, FindableType<C> Key>
  auto find(C&& c, const Key& key) {
    if constexpr (!SetType<C> && !MapType<C>){
      return std::find(begin(c), end(c), key);
    } else return c.find(key);
  }
  template<ContainerType C, FindableType<C> Key>
  auto find_reverse(C&& c, const Key& key) {
    if constexpr (!SetType<C> && !MapType<C>){
      auto it = std::end(c);
      while(it != std::begin(c))
        if(*(--it) == key) return it;
      return std::end(c);
    } else return c.find(key);
  }


  template<SetType S1, SetType S2 = S1> requires std::is_convertible_v<typename S2::const_iterator, typename S1::const_iterator>
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

  template<class T, SetType S> requires (!std::is_convertible_v<S,singleton_set<value_type_of_t<S>>>)
  auto_iter<typename S::const_iterator> common_element(const S& y, const singleton_set<T>& x) {
    if(!x.empty())
      return {find(y, front(x)), end(y)};
    else
      return {end(y), end(y)};
  }

  // std::unordered_set has no rbegin(), so we just alias it to begin()
  template<class Key, class Hash, class KE, class A>
  auto rbegin(const std::unordered_set<Key, Hash, KE, A>& s) { return s.begin(); }
  template<class Key, class Hash, class KE, class A>
  auto rbegin(std::unordered_set<Key, Hash, KE, A>& s) { return s.begin(); }
  template<class Key, class Hash, class KE, class A>
  auto rend(const std::unordered_set<Key, Hash, KE, A>& s) { return s.end(); }
  template<class Key, class Hash, class KE, class A>
  auto rbend(std::unordered_set<Key, Hash, KE, A>& s) { return s.end(); }


  template<class T> concept HasFront = requires(T x) { x.front(); };
  template<class T> concept HasBack = requires(T x) { x.back(); };

  template<IterableType T>
  constexpr decltype(auto) front(T&& c) {
    assert(!c.empty());
    if constexpr (HasFront<T>)
      return c.front();
    else return *(std::begin(c));
  }
  template<IterableType T>
  constexpr decltype(auto) next_to_front(T&& c) { assert(!c.empty()); return *(std::next(std::begin(c))); }
  template<IterableType T>
  constexpr decltype(auto) back(T&& c) {
    assert(!c.empty());
    if constexpr (HasBack<T>)
      return c.back();
    else return *(mstd::rbegin(c));
  }
  template<IterableType T>
  constexpr decltype(auto) next_to_back(T&& c) { assert(!c.empty()); return *(std::next(mstd::rbegin(c))); }

  template<class T, T _invalid, IterableType Container> requires std::is_same_v<value_type_of_t<Container>, std::remove_cvref_t<T>>
  constexpr T any_element(Container&& c) {
    return c.empty() ? _invalid : front(std::forward<Container>(c));
  }

  template<class T> concept StrictSettableType = requires(T t, T::value_type x) { { t.set(x) } -> std::convertible_to<bool>; };
  template<class T> concept SettableType = StrictSettableType<std::remove_cvref_t<T>>;

  // on arithmetic types just adds the second to the first
  template<ArithmeticType P, ArithmeticType Q>
  auto append(P& p, Q&& q) { p += std::forward<Q>(q); return std::pair{&p, true}; }

  // on callables, append will call the function and return the result
  template<class F, class... Args> requires std::is_invocable_v<F, Args&&...>
  auto append(F&& f, Args&&... args) {
    using F_Result = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
    if constexpr (std::is_void_v<F_Result>)
      return std::forward<F>(f)(std::forward<Args>(args)...);
    else return std::pair<F_Result,bool>{std::forward<F>(f)(std::forward<Args>(args)...), true};
  }

  // append with 2 containers means to add the second to the end of the first
  template<ContainerType C1, IterableTypeWithSameIterators C2> requires (ConvertibleValueTypes<C1,C2> && !VectorOrStringType<C1>)
  auto append(C1& x, C2&& y) {
    x.insert(y.begin(), y.end());
    return emplace_result<C1>{x.begin(), true};
  }
  template<VectorType V, IterableTypeWithSameIterators C2> requires ConvertibleValueTypes<V,C2>
  auto append(V& x, const C2& y) {
    x.insert(x.end(), y.begin(), y.end());
    return emplace_result<V>{x.begin(), true};
  }

  // on non-map containers, append = emplace
  template<IterableType C, class First, class ...Args>
    requires (!MapType<C> && !VectorOrStringType<C> && !CompatibleValueTypes<C, First> &&
        std::is_constructible_v<mstd::value_type_of_t<C>, First&&, Args&&...>)
  auto append(C& container, First&& first, Args&&... args) { return container.emplace(std::forward<First>(first), std::forward<Args>(args)...); }

  // on vectors, append =  emplace_back 
  // this is bad: vector_map<> can be "upcast" to vector<> so this will always conflict with the append for maps
  // the suggestion on stackoverflow is "stop spitting against the wind"... :(
  // so for now, I'm using try_emplace() in all places that would be ambiguous
  template<VectorOrStringType V, class First, class... Args> requires (!ConvertibleValueTypes<V, First>)
  auto append(V& _vec, First&& first, Args&&... args) { 
    return emplace_result<V>{_vec.emplace(_vec.end(), std::forward<First>(first), std::forward<Args>(args)...), true};
  }
  // dummy function to not insert anything into an appended vector
  template<VectorType V>
  auto append(V&& _vec) { return emplace_result<V>{_vec.begin(), true}; }
 
  // on maps to primitives, append = try_emplace
  //NOTE: this can be used also if mapped_type is NOT a primitive, but no initialization arguments have been given
  template<MapType M, class Key, class... Args>
    requires (std::is_convertible_v<Key, key_type_of_t<M>> && !(ContainerType<mapped_type_of_t<M>> && (sizeof...(Args) > 0)))
  auto append(M& _map, Key&& _key, Args&&... args)
  { return _map.try_emplace(std::forward<Key>(_key), std::forward<Args>(args)...); }

  // on maps to containers, append = append to container at map[key]
  //NOTE: return an iterator to the pair in the map whose second now contains the newly constructed item
  //      also return a bool indicating whether insertion took place
  //NOTE: this also works for emplacing a string into a map that maps to strings, the "inserting" appends below are called in this case
  template<MapType M, class Key, class ...Args>
    requires (std::is_convertible_v<Key, key_type_of_t<M>> && ContainerType<mapped_type_of_t<M>> && (sizeof...(Args) > 0))
  auto append(M& _map, Key&& _key, Args&&... args) {
    const auto iter = _map.try_emplace(std::forward<Key>(_key)).first;
    const bool success = append(iter->second, std::forward<Args>(args)...).second;
    return emplace_result<M>{iter, success};
  }

  template<ContainerType C1, IterableType C2> requires (ConvertibleValueTypes<C1,C2> && !IterableTypeWithSameIterators<C2>)
  auto append(C1& x, C2&& y) {
    using ItemRef = copy_cvref_t<C2&&, value_type_of_t<C2>>;
    for(ItemRef i: std::forward<C2>(y))
      append(x, static_cast<ItemRef>(i)); // so, right, I know i should already have that type, but not if ItemRef is an rvalue-reference because... reasons
    return emplace_result<C1>{x.begin(), true};
  }



  // if we're not interested in the return value, we can set values more efficiently
  template<class S> requires (ContainerType<S> && !SettableType<S>)
  bool set_val(S& s, const auto& val) { return append(s, val).second; }
  template<class S> requires (ContainerType<S> && SettableType<S>)
  bool set_val(S& s, const auto& val) { return s.set(val); }

#warning "TODO: add set_val for everything that we can do append on, but discard the iterator; BEFORE: test if this isn't done automatically by the optimizer"


  template<class Index, class C> requires ContainerType<C>
  decltype(auto) lookup(C&& c, Index&& index) { return c.at(index); }
  template<class Index, class C> requires (std::invocable<C, Index> && !ContainerType<C>)
  decltype(auto) lookup(C&& c, Index&& index) { return c(index); }


  // test if something is in the set
  template<SetType S>
  bool test(const S& _set, const value_type_of_t<S>& key) { return _set.count(key); }
  template<MapType M>
  bool test(const M& _map, const key_type_of_t<M>& key) { return _map.count(key); }
  template<VectorType V>
  bool test(const V& vec, const auto& key) { return mstd::find(vec, key) != std::end(vec); }
  template<class T>
  bool test(const T& x, const T& y) { return x == y; }

  template<class T, std::invocable<T> F>
    requires (std::is_convertible_v<std::invoke_result_t<F,T>, bool> || mstd::IterableTypeWithSize<std::invoke_result_t<F,T>>)
  decltype(auto) test(const F& f, const T& x) {
    decltype(auto) y = f(x);
    if constexpr (mstd::IterableTypeWithSize<decltype(y)>)
      return !y.empty();
    else return y;
  }

  template<SetType C1, SetType C2> requires std::is_convertible_v<value_type_of_t<C1>, value_type_of_t<C2>>
  C2& copy(const C1& x, C2& y) {
    y.clear();
    std::copy(x.begin(), x.end(), std::insert_iterator<C2>(y, y.begin()));
    return y;
  }

  template<OptionalContainerType C = void, class T, class _C = VoidOr<C, std::unordered_set<value_type_of_t<iterable_bitset<T>>>>>
  _C to_set(const iterable_bitset<T>& x) {
    _C result;
    return copy(x, result);
  }

  //! a hash computation for a set, XORing its members
  template<IterableType C>
  struct set_hash {
    constexpr static std::hash<std::remove_cvref_t<value_type_of_t<C>>> Hasher{};
    size_t operator()(const C& container) const {
      return std::accumulate(std::begin(container), std::end(container), size_t(0), [](const size_t x, const auto& y) { return x ^ Hasher(y); });
    }
  };
  // iterable bitset can be hashed faster
  template<> struct set_hash<ordered_bitset>: public std::hash<ordered_bitset> {};
  template<> struct set_hash<unordered_bitset>: public std::hash<unordered_bitset> {};

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<IterableType C>
  struct list_hash {
    static constexpr std::hash<value_type_of_t<C>> Hasher{};
    size_t operator()(const C& container) const {
      return std::accumulate(std::begin(container), std::end(container), size_t(0), [](const size_t x, const auto& y) { return std::rotl(x,1) ^ Hasher(y); });
    }
  };


  template<ContainerType C>
  auto erase(C& c, const const_iterator_of_t<C>& iter) { return c.erase(iter); }

  template<ContainerType C, class Key>
    requires (!std::is_same_v<Key, const_iterator_of_t<C>> && std::is_convertible_v<const Key&, value_type_of_t<C>>)
  auto erase(C& c, const Key& key) {
    if constexpr (std::is_same_v<std::remove_cvref_t<Key>, iterator_of_t<C>>)
      return c.erase(key);
    else if constexpr (std::is_same_v<std::remove_cvref_t<Key>, const_iterator_of_t<C>>)
      return c.erase(key);
    else if constexpr (VectorType<C>) {
      return c.erase(std::remove(c.begin(), c.end(), key), c.end());
    } else return c.erase(key);
  }
  template<MapType M, class Key>
    requires std::is_convertible_v<const Key&, key_type_of_t<M>>
  auto erase(M& m, const Key& key) { return erase(m, key); }

  template<ContainerType C, class Key>
    requires (std::is_invocable_v<Key, mstd::value_type_of_t<C>>)
  auto erase(C& c, const Key& key) {
    if constexpr (VectorType<C>) {
      return c.erase(std::remove_if(c.begin(), c.end(), key), c.end());
    } else {
      for(auto iter = c.begin(); iter != c.end();)
        if(key(*iter)) iter = c.erase(iter); else ++iter;
    }
  }

  template<ContainerType C, IterableType Keys>
    requires (std::is_convertible_v<std::remove_cvref_t<value_type_of_t<Keys>>, value_type_of_t<C>>)
  void erase_by_iterating_keys(C& c, const Keys& keys) {
    for(const auto& key: keys) erase(c, key);
  }
  template<MapType M, IterableType Keys>
    requires (std::is_convertible_v<std::remove_cvref_t<value_type_of_t<Keys>>, key_type_of_t<M>>)
  void erase_by_iterating_keys(M& m, const Keys& keys) {
    for(const auto& key: keys) erase(m, key);
  }

  template<ContainerType C, IterableType Keys>
  void erase_by_iterating_container(C& c, const Keys& keys) {
    for(auto iter = begin(c); iter != end(c);)
      if(test(keys, *iter))
        iter = erase(c, iter);
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
  void erase(C& c, const Keys& keys) {
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
    erase(c, mstd::rbegin(c));
  }

  // value-moving pop operations
  template<QueueType Q>
  auto value_pop(Q& q) {
    using value_type = Q::value_type;
    value_type result = std::move(q.top());
    q.pop();
    return result;
  }
  template<VectorType Q>
  auto value_pop(Q& q) {
    auto result = std::move(q.back());
    q.pop_back();
    return result;
  }
  template<ContainerType Q> requires (!VectorType<Q>)
  auto value_pop(Q& q) {
    const auto iter = q.begin();
    value_type_of_t<Q> result = std::move(*iter);
    erase(q, iter);
    return result;
  }

  template<ContainerType C>
  auto value_pop(C& q, const iterator_of_t<const C>& iter) {
    if(iter != end(q)) {
      auto result = std::move(*iter);
      q.erase(iter);
      return result;
    } else throw std::out_of_range("trying to pop values beyond the end");
  }

  template<IterableType C, class Key> requires requires(C c, Key k) { { mstd::find(c, k) } -> std::convertible_to<iterator_of_t<const C>>; }
  auto value_pop(C& q, const Key& key) {
    return value_pop(q, find(q, key));
  }

  template<ContainerType Q> requires requires(Q q) { { front(q) } -> std::convertible_to<value_type_of_t<Q>>; }
  auto value_pop_front(Q& q) {
    assert(!q.empty());
    const auto it = std::begin(q);
    auto v = std::move(*it);
    erase(q, it);
    return v;
  }
  template<IterableType Q> requires requires(Q q) { { back(q) } -> std::convertible_to<value_type_of_t<Q>>; }
  auto value_pop_back(Q& q) {
    assert(!q.empty());
    const auto it = std::prev(std::end(q));
    auto v = std::move(*it);
    erase(q, it);
    return v;
  }



  template<SetType S, class Val = value_type_of_t<S>> requires (!MapType<S> && !SingletonSetType<S>)
  void replace(S& s, const auto& _old_it, Val&& _new) {
    auto node = s.extract(_old_it);
    node.value() = std::forward<Val>(_new);
    s.insert(std::move(node));
  }

  template<MapType M, class Key = key_type_of_t<M>>
  void replace(M& m, const auto& _old_it, Key&& _new) {
    auto node = m.extract(_old_it);
    node.key() = std::forward<Key>(_new);
    m.insert(std::move(node));
  }
  template<class T, class A>
  void replace(std::vector<T,A>& c, const typename std::vector<T,A>::iterator& _old_it, T&& _new) {
    *_old_it = std::forward<T>(_new);
  }
  template<Optional T, class Q>
  void replace(singleton_set<T>& c, const typename singleton_set<T>::iterator& _old_it, Q&& _new) {
    *_old_it = std::forward<Q>(_new);
  }

  template<ContainerType C, class Val = value_type_of_t<C>>
  bool replace(C& c, const value_type_of_t<C>& _old, Val&& _new) {
    const auto iter = find(c, _old);
    if(iter != end(c)) {
      replace(c, iter, std::forward<Val>(_new));
      return true;
    } else return false;
  }

  template<ContainerType C, class Iter>
  value_type_of_t<C> extract(C& c, const Iter& iter) {
  }

  // clear a container except for 1 item
  template<ContainerType C, class Iter>
  void clear_except(C& c, const Iter& except_iter) {
    if(except_iter != end(c)) {
      if(c.size() > 1) {
        if constexpr (MapType<C>) {
          auto node = c.extract(except_iter);
          c.clear();
          c.try_emplace(std::move(node.key()), std::move(node.mapped()));
        } else if constexpr (SetType<C> && !SingletonSetType<C>) {
          auto tmp = std::move(c.extract(except_iter).value());
          c.clear();
          append(c, std::move(tmp));
        } else {
          auto tmp = std::move(*except_iter);
          c.clear();
          append(c, std::move(tmp));
        }
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
      S(std::move(other))
    { other.clear(); }

    auto_clearing& operator=(const auto_clearing& other) = default;
    auto_clearing& operator=(auto_clearing&& other) {
      S::operator=(std::move(other));
      other.clear();
    }

  };
}


namespace std {
  template<mstd::ContainerType Container, class T> requires (!is_convertible_v<std::remove_cvref_t<Container>, std::string_view>)
  Container& operator-=(Container& container, T&& item) {
    mstd::erase(container, std::forward<T>(item));
    return container;
  }

  template<mstd::ContainerType Container, class T> requires (!is_convertible_v<std::remove_cvref_t<Container>, std::string_view>)
  Container& operator+=(Container& container, T&& item) {
    mstd::append(container, std::forward<T>(item));
    return container;
  }


  template<class... Args> bool test(Args&&... args) { return mstd::test(std::forward<Args>(args)...); }
  template<class... Args> decltype(auto) append(Args&&... args) { return mstd::append(std::forward<Args>(args)...); }
  template<class... Args> decltype(auto) erase(Args&&... args) { return mstd::erase(std::forward<Args>(args)...); }
  template<class... Args> decltype(auto) lookup(Args&&... args) { return mstd::lookup(std::forward<Args>(args)...); }
}


