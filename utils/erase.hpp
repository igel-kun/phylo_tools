

#pragma once

/* This is a generic interface to add/remove things from containers, where
 * erase(x, y) means:
 *    x - y                   if both x and y are arithmetic
 *    x.erase(y)              if x is a container of y's or map with y's as keys
 *    x.erase(x.remove_if(y)) if y is invokable with x's values
 *    [x1 x2..] / [y1 y2..]   if x and y are containers with equal-comparable value-types
 */

#include "stl_utils.hpp"
#include "append.hpp"

namespace mstd {

  template<ArithmeticType P, ArithmeticType Q>
  void erase(P& p, Q&& q) { p -= std::forward<Q>(q); }

  template<ContainerType C, class Key>
    requires (std::is_same_v<Key, const_iterator_of_t<C>> || std::is_same_v<Key, iterator_of_t<C>>)
  auto erase(C& c, const Key& key) { return c.erase(key); }

  template<ContainerType C, class Key>
    requires(std::equality_comparable_with<const Key&, value_type_of_t<C>>)
  auto erase(C& c, const Key& key) {
    if constexpr (VectorType<C>) { // erasing keys usually returns the number of keys removed, so we need to massage vector::erase a little
      const auto iter = std::remove(c.begin(), c.end(), key);
      const auto result = std::distance(iter, c.end());
      c.erase(iter, c.end());
      return result;
    } else if constexpr (std::is_convertible_v<Key, value_type_of_t<C>>) {
      return c.erase(key);
    } else return erase(c, [&](const auto x){return key == x; });
  }

  template<MapType M, class Key>
    requires std::is_convertible_v<const Key&, key_type_of_t<M>>
  auto erase(M& m, const Key& key) { return m.erase(key); }

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

  // ----------------- substract containers from each other ---------------------
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
    if constexpr (VectorType<C>)
      if(k_size < c_size) output.reserve(c_size - k_size);
    for(auto& x: c)
      if(!test(keys, x))
        append(output, std::move(x));
    c = std::move(output);
  }

  template<ContainerType C, IterableType Keys>
  void erase(C& c, const Keys& keys) {
    if constexpr (SetType<Keys>) {
      if constexpr (VectorType<C>) {
        erase_by_moving(c, keys);
      } else erase_by_iterating_container(c, keys);
    } else if constexpr (ContainerType<Keys>) {
      if constexpr (SetType<C> || MapType<C>) {
        erase_by_iterating_keys(c, keys);
      } else if constexpr (VectorType<C>) {
        erase_by_moving(c, keys);
      } else erase_by_iterating_container(c, keys);
    } else erase_by_iterating_keys(c, keys); // if the keys are not in a container, we may not be able to iterate multiple times over them
  }

  // a quick erase for a vector, swapping the item with the last item and pop_back() that last item
  template<VectorType V>
  void quick_erase(V& vec, const iterator_of_t<V>& iter) {
    assert(vec.size() != 0);
    std::swap(*iter, *std::prev(vec.end()));
    vec.pop_back();
  }
  template<VectorType V>
  void quick_erase(V& vec, const size_t i) { quick_erase(std::forward<V>(vec), std::advance(vec.begin(), i)); }


}
