
#pragma once

/* This is a generic interface to add/remove things from containers, where
 * append(x,y,...) means:
 *    x + y                 if both x and y are arithmetic
 *    x(y)                  if x can be invoked with y
 *    [x1 x2..][y1 y2 ..]   if both x and y are containers of the same type (concatenation)
 *    x.emplace_back(y)     if x is a vector of y's
 *    x.try_emplace(y,...)  if x is a map with y's as keys
 *    x.emplace(y)          if x is a non-vector, non-map container of y's
 */

#include "stl_utils.hpp"

namespace mstd {
  template<class T>
  using emplace_result = std::pair<typename std::remove_reference_t<T>::iterator, bool>;

  // ----------- append for: arithmetics -----------------
  // on arithmetic types just adds the second to the first
  template<ArithmeticType P, ArithmeticType Q>
  auto append(P& p, Q&& q) { p += std::forward<Q>(q); return std::pair{&p, true}; }

  // ----------- append for: adding items to containers -----------------
  // on vectors, append = emplace_back 
  // this is bad: vector_map<> can be "upcast" to vector<> so this will always conflict with the append for maps
  // the suggestion on stackoverflow is "stop spitting against the wind"... :(
  // so for now, I'm using try_emplace() in all places that would be ambiguous
  template<class V, class First, class... Args>
    requires (not ConvertibleValueTypes<V, First> and  // make sure we're not trying to append the Container 'First' to the end of 'V'
              (VectorOrStringType<V> or is_derived_from_template_v<V, std::vector>))
  auto append(V& _vec, First&& first, Args&&... args) { 
    return emplace_result<V>{_vec.emplace(_vec.end(), std::forward<First>(first), std::forward<Args>(args)...), true};
  }
  // dummy function to not insert anything into an appended vector
  template<VectorType V>
  auto append(V&& _vec) { return emplace_result<V>{_vec.begin(), true}; }

  // allow passing pairs to maps in order to emplace them
  template<MapType M, class Key, class... Args>
    requires std::is_convertible_v<Key, value_type_of_t<M>>
  auto append(M& _map, Key&& _key, Args&&... args)
  { return _map.emplace(std::forward<Key>(_key), std::forward<Args>(args)...); }

  // on maps to primitives, append = try_emplace
  //NOTE: this can be used also if mapped_type is NOT a primitive, but no initialization arguments have been given
  template<MapType M, class Key, class... Args>
    requires (std::is_convertible_v<Key, key_type_of_t<M>> and not (ContainerType<mapped_type_of_t<M>> and (sizeof...(Args) > 0)))
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


  // on non-map non-vector containers, append = emplace
  template<IterableType C, class First, class ...Args>
    requires (not MapType<C> and not VectorOrStringType<C> and not CompatibleValueTypes<C, First> and not is_derived_from_template_v<C, std::vector> and
        std::is_constructible_v<mstd::value_type_of_t<C>, First&&, Args&&...>)
  auto append(C& container, First&& first, Args&&... args) { return container.emplace(std::forward<First>(first), std::forward<Args>(args)...); }



  // ----------- append for: callables -----------------
  // on callables, append will call the function and return the result
  template<class F, class... Args> requires std::is_invocable_v<F, Args&&...>
  auto append(F&& f, Args&&... args) {
    using F_Result = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
    if constexpr (std::is_void_v<F_Result>)
      return std::forward<F>(f)(std::forward<Args>(args)...);
    else return std::pair<F_Result,bool>{std::forward<F>(f)(std::forward<Args>(args)...), true};
  }


  // ----------- append for: concatenating containers -----------------
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

  template<ContainerType C1, IterableType C2> requires (ConvertibleValueTypes<C1,C2> and not IterableTypeWithSameIterators<C2>)
  auto append(C1& x, C2&& y) {
    for(decltype(auto) i: std::forward<C2>(y)) {
      if constexpr (std::is_rvalue_reference_v<decltype(i)>)
        append(x, std::move(i)); // I'm not sure I understand when an rvalue_ref is considered an lvalue ref and when not...
      else append(x, i);
    }
    return emplace_result<C1>{x.begin(), true};
  }

  template<ContainerType C, VerifyableIter Iter> // if C is not a container of iterators and we get a verifyable iterator, then we can iterate it into C
    requires (ConvertibleValueTypes<C,Iter> && not IterableType<Iter> && not mstd::is_convertible_v<Iter, value_type_of_t<C>>)
  auto append(C& x, Iter it) {
    while(it.is_valid()) {
      append(x, *it);
      ++it;
    }
    return emplace_result<C>{x.begin(), true};
  }



  template<class T, class... Args>
  concept is_appendable_v = requires(T t, Args&&... args) { append(t, args...); };
  template<class T, TypeRune rune, class... Args>
  concept AppendableR = apply_rune_v<T, rune> || is_appendable_v<T, Args...>;
  template<class T, class... Args>
  concept Appendable = AppendableR<T, TR_ConstRefOK, Args...>;

}
