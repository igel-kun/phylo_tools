
#pragma once

#include <queue>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <type_traits>
#include <numeric>
#include "stl_utils.hpp"
#include "append.hpp"
#include "erase.hpp"
#include "auto_iter.hpp"
#include "singleton.hpp"
#include "iter_bitset.hpp"

// unifcation for the set interface
// this is to use std::unordered_set<uint32_t> with the same interface as iterable_bitset

namespace mstd { // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :) 
#warning "TODO: write a back() function (doing front() for std::unordered containers)"

  template<class T> concept StrictSettableType = requires(T t, T::value_type x) { { t.set(x) } -> std::convertible_to<bool>; };
  template<class T> concept SettableType = StrictSettableType<std::remove_cvref_t<T>>;

  // if we're not interested in the return value, we can set values more efficiently
  template<class S> requires (ContainerType<S> && !SettableType<S>)
  bool set_val(S& s, const auto& val) { return append(s, val).second; }
  template<class S> requires (ContainerType<S> && SettableType<S>)
  bool set_val(S& s, const auto& val) { return s.set(val); }

#warning "TODO: add set_val for everything that we can do append on, but discard the iterator; BEFORE: test if this isn't done automatically by the optimizer"

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
  auto common_element(const S1& x, const S2& y) {
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



  template<class Index, class C> requires ContainerType<C>
  decltype(auto) lookup(C&& c, Index&& index) { return c.at(index); }
  template<class Index, class C> requires (std::invocable<C, Index> && !ContainerType<C>)
  decltype(auto) lookup(C&& c, Index&& index) { return c(index); }

  // test if something is in the set
  template<SetType S>
  bool test(const S& _set, const value_type_of_t<S>& key) { return _set.count(key); }
  template<MapType M>
  bool test(const M& _map, const key_type_of_t<M>& key) { return _map.count(key); }
  template<VectorType V, class Key> requires (FindableType<Key, V> and not MapType<V>)
  bool test(const V& vec, const Key& key) { return mstd::find(vec, key) != std::end(vec); }
  template<class T>
  bool test(const T& x, const T& y) { return x == y; }

  template<class T, std::invocable<T> F>
    requires (std::is_convertible_v<std::invoke_result_t<F,T>, bool> || mstd::IterableType<std::invoke_result_t<F,T>>)
  bool test(const F& f, const T& x) {
    if constexpr (mstd::IterableType<decltype(f(x))>)
      return not f(x).empty();
    else return f(x);
  }

  template<class T, class Arg>
  concept is_testable = requires(T t, Arg arg) { mstd::test(t, arg); };



  template<SetType Set, class ValueType = value_type_of_t<Set>>
  void flip(Set& _set, const ValueType& index) {
    const auto [iter, success] = _set.emplace(index);
    if(!success) _set.erase(iter);
  }
  template<class T>
  void flip(iterable_bitset<T>& _set, const uintptr_t index) { return _set.flip(index); }

  // intersect two containers
  template<ContainerType A, ContainerType B> requires ((not SetType<A>) or (not SetType<B>))
  A get_intersection(const A& a, const B& b) {
    A result;
    if constexpr (not SetType<B>) {
      for(const auto& x: b)
        if(mstd::test(a, x))
          append(result, x);
    } else {
      for(const auto& x: a)
        if(mstd::test(b, x))
          append(result, x);
    }
    return result;
  }

  template<IterBitsetType A>
  A get_intersection(const A& a, const A& b) { return a & b; }

  template<SetType A, SetType B> requires ((not IterBitsetType<A>) or (not IterBitsetType<B>))
  A get_intersection(const A& a, const B& b) {
    A result;
    if(a.size() > b.size()) {
      for(const auto& x: b)
        if(mstd::test(a, x))
            append(result, x);
    } else {
      for(const auto& x: a)
        if(mstd::test(b, x))
            append(result, x);
    }
    return result;
  }


  // destructive intersection
  template<ContainerType A, ContainerType B> requires (not SetType<A>)
  void intersect(A& target, const B& source) {
    erase(target, [&source](const auto& x) { return not mstd::test(source, x); });
  }
  template<SetType A, ContainerType B> requires (not SetType<B>)
  void intersect(A& target, const B& source) { target = get_intersection(target, source); }

  template<SetType A, SetType B> requires ((not IterBitsetType<A>) or (not IterBitsetType<B>))
  void intersect(A& target, const B& source) {
    if(target.size() > source.size()) {
      target = get_intersection(target, source);
    } else erase(target, [&source](const auto& x) { return not mstd::test(source, x); });
  }
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
  template<class T> concept HasPopBack = requires(T x) { x.pop_back(); };

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

  template<IterableType C1, SetType C2> requires std::is_convertible_v<value_type_of_t<C1>, value_type_of_t<C2>>
  C2& copy(const C1& x, C2& y) {
    y.clear();
    std::copy(x.begin(), x.end(), std::insert_iterator<C2>(y, y.begin()));
    return y;
  }

  template<OptionalContainerType Target = void, class Source, class Target_ = FirstNonVoid<Target, std::unordered_set<value_type_of_t<Source>>>>
    requires (!std::is_same_v<std::remove_cvref_t<Source>, std::remove_cvref_t<Target_>>)
  Target_ to_set(Source&& source) {
    Target_ result;
    for(auto&& x: source) {
      if constexpr (std::is_rvalue_reference_v<Source&&>) {
        append(result, std::move(x));
      } else {
        append(result, x);
      }
    }
    return result;
  }

  // default to no-op for Source == Target
  template<OptionalContainerType Target = void, class Source, class Target_ = FirstNonVoid<Target, std::unordered_set<value_type_of_t<Source>>>>
    requires (std::is_same_v<std::remove_cvref_t<Source>, std::remove_cvref_t<Target_>>)
  Target_ to_set(Source&& source) { return std::forward<Source>(source); }

  //! a hash computation for a set, XORing its members
  template<IterableType C, class Val = value_type_of_t<C>> requires std::is_convertible_v<value_type_of_t<C>, Val>
  struct XOR_hash {
    using value_type = std::remove_cvref_t<Val>;
    static constexpr std::hash<value_type> Hasher{};
    static constexpr bool is_XOR_hashing = true;

    static constexpr size_t hash_one(const size_t _hash, const value_type& element) { return _hash ^ Hasher(element); }

    template<IterableType Container> requires std::is_convertible_v<value_type_of_t<Container>, Val>
    size_t operator()(const Container& container, const size_t _hash = 0) const {
      return std::ranges::fold_left(container, _hash, hash_one);
    }

  };
  template<IterableType C, class Val = value_type_of_t<C>> requires std::is_convertible_v<value_type_of_t<C>, Val>
  struct set_hash: public XOR_hash<C, Val> {};

  // iterable bitset can be hashed faster
  template<class T> struct set_hash<ordered_bitset, T>: public std::hash<ordered_bitset> {};
  template<class T> struct set_hash<unordered_bitset, T>: public std::hash<unordered_bitset> {};

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<IterableType C>
  struct list_hash {
    static constexpr std::hash<value_type_of_t<C>> Hasher{};
    size_t operator()(const C& container) const {
      return std::accumulate(std::begin(container), std::end(container), size_t(0), [](const size_t x, const auto& y) { return std::rotl(x,1) ^ Hasher(y); });
    }
  };



  template<StrictContainerType C>
  void pop_back(C& c) {
    assert(!c.empty());
    if constexpr (HasPopBack<C>) {
      c.pop_back();
    } else if constexpr (QueueType<C>) {
      c.pop();
    } else erase(c, mstd::rbegin(c));
  }

  template<class T> concept is_poppable = requires(T t) { mstd::pop_back(t); };



  // value-moving pop operations
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
    mstd::erase(q, it);
    return v;
  }
  template<IterableType Q> requires requires(Q q) { { back(q) } -> std::convertible_to<value_type_of_t<Q>>; }
  auto value_pop_back(Q& q) {
    assert(!q.empty());
    const auto it = std::prev(std::end(q));
    auto v = std::move(*it);
    mstd::erase(q, it);
    return v;
  }

  // **** CAUTION: the following is hacky, but technically legal ***
  // NOTE: please someone convince the committy to fix move-access to priority_queues in the STL, so this hack is no longer necessary
  // add container access to priority queues, thanks to https://stackoverflow.com/a/12886393/6470423
  template <class T, class S, class C>
  S& priority_queue_container(std::priority_queue<T, S, C>& q) {
    struct HackedQueue: private std::priority_queue<T, S, C> {
      static S& Container(std::priority_queue<T, S, C>& q) {
        return q.*&HackedQueue::c;
      }
    };
    return HackedQueue::Container(q);
  }

  template <class T, class Storage, class Compare>
  auto value_pop(std::priority_queue<T, Storage, Compare>& q) {
    struct HackedQueue: private std::priority_queue<T, Storage, Compare> {
      static auto value_pop(std::priority_queue<T, Storage, Compare>& q) {
        Storage& c = q.*&HackedQueue::c;
        Compare& comp = q.*&HackedQueue::comp;
        std::pop_heap(c.begin(), c.end(), comp);
        auto value = std::move(c.back());
        c.pop_back();
        return value;
      }
    };
    return HackedQueue::value_pop(q);
  }
  // **** END OF DIRTY HACK ****




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

  template<ContainerType Container, IterableType T>
  Container to_container(T&& x) {
    Container result;
    append(result, std::forward<T>(x));
    return result;
  }

  struct SetSize { size_t operator()(const auto& x) const { return x.size(); } };
}


namespace std {
/*  
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
*/

  template<class... Args> bool test(Args&&... args) { return mstd::test(std::forward<Args>(args)...); }
  template<class... Args> decltype(auto) append(Args&&... args) { return mstd::append(std::forward<Args>(args)...); }
  template<class... Args> decltype(auto) erase(Args&&... args) { return mstd::erase(std::forward<Args>(args)...); }
  template<class... Args> decltype(auto) lookup(Args&&... args) { return mstd::lookup(std::forward<Args>(args)...); }
}


