
#pragma once

#include "stl_utils.hpp"

namespace mstd {
  template<class T>
  using emplace_result = std::pair<typename std::remove_reference_t<T>::iterator, bool>;

  // on arithmetic types just adds the second to the first
  template<ArithmeticType P, ArithmeticType Q>
  auto append(P& p, Q&& q) { p += std::forward<Q>(q); return std::pair{&p, true}; }



  // on vectors, append = emplace_back 
  // this is bad: vector_map<> can be "upcast" to vector<> so this will always conflict with the append for maps
  // the suggestion on stackoverflow is "stop spitting against the wind"... :(
  // so for now, I'm using try_emplace() in all places that would be ambiguous
  template<VectorOrStringType V, class First, class... Args>
    requires (not ConvertibleValueTypes<V, First>) // make sure we're not trying to append the Container 'First' to the end of 'V'
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


  // on non-map non-vector containers, append = emplace
  template<IterableType C, class First, class ...Args>
    requires (!MapType<C> && !VectorOrStringType<C> && !CompatibleValueTypes<C, First> &&
        std::is_constructible_v<mstd::value_type_of_t<C>, First&&, Args&&...>)
  auto append(C& container, First&& first, Args&&... args) { return container.emplace(std::forward<First>(first), std::forward<Args>(args)...); }





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

  template<ContainerType C1, IterableType C2> requires (ConvertibleValueTypes<C1,C2> && !IterableTypeWithSameIterators<C2>)
  auto append(C1& x, C2&& y) {
    using ItemRef = copy_cvref_t<C2&&, value_type_of_t<C2>>;
    for(ItemRef i: std::forward<C2>(y))
      append(x, static_cast<ItemRef>(i)); // so, right, I know i should already have that type, but not if ItemRef is an rvalue-reference because... reasons
    return emplace_result<C1>{x.begin(), true};
  }





  template<class T, class... Args>
  concept Appendable = requires(T t, Args&&... args) {
    append(t, args...);
  };

}
