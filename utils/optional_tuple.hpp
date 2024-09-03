
#pragma once

#include "stl_utils.hpp"

namespace mstd {
  template<size_t i, class T>
  struct optional_item {
    T value;

    /*
    optional_item() = default;
    optional_item(const optional_item&) = default;
    optional_item(optional_item&&) = default;

    optional_item& operator=(const optional_item&) = default;
    optional_item& operator=(optional_item&&) = default;
    
    template<class First, class... Args> 
      requires (!std::is_same_v<std::remove_cvref_t<First>, optional_item> && (!std::is_reference_v<T> || (sizeof...(Args) != 0)))
    optional_item(First&& first, Args&&... args): value(std::forward<Args>(args)...) {}
*/
// NOTE: the ability to convert an optional_tuple to any of its members is confusing and NOT a good idea
//    For example: NodeSet L = N.leaves() returns the SeenSet of the DFS traversal and that's NOT what we want
//
//    operator T&() { return value; }
//    operator const T&() const { return value; }
  };
  template<size_t i> struct optional_item<i, void> {};

  // optional pointers can be initialized from references
  template<size_t i, class T> requires (std::is_pointer_v<T>)
  struct optional_item<i, T> {
    using BareT = std::remove_pointer_t<T>;
    T value = nullptr;

    optional_item() = default;
    optional_item(T t): value{t} {}
    optional_item(BareT& ref): value{&ref} {}
  };



  template<size_t i, class... T>
  struct _optional_tuple {};

  // base case: tuple with no items
  template<size_t i>
  struct _optional_tuple<i>{
    _optional_tuple(const std::piecewise_construct_t = std::piecewise_construct) {}
  };

  // recursive case: tuple with item of type LastT
  template<size_t i, class LastT, class... Rest> requires (!std::is_reference_v<LastT>)
  struct _optional_tuple<i, LastT, Rest...>:
    public optional_item<i, LastT>,
    public _optional_tuple<i + 1, Rest...>
  {
    _optional_tuple() = default;

    template<class _LastT, class... _Rest> requires (!std::is_base_of_v<_optional_tuple<i + 1 + sizeof...(Rest)>, std::remove_cvref_t<_LastT>>)
    _optional_tuple(_LastT&& last, _Rest&&... rest):
      optional_item<i, LastT>(std::forward<_LastT>(last)),
      _optional_tuple<i + 1, Rest...>(std::forward<_Rest>(rest)...)
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(const std::piecewise_construct_t, _LastT&& last, _Rest&&... rest):
      optional_item<i, LastT>(std::make_from_tuple(std::forward<_LastT>(last))),
      _optional_tuple<i + 1, Rest...>(std::forward<_Rest>(rest)...)
    {}

    // construct from any other optional tuple
    // NOTE: if the field i in the other optional tuple is 'void' then we skip initialization of field i of this optional tuple
    template<class _LastT, class... _Rest>
    _optional_tuple(const _optional_tuple<i, _LastT, _Rest...>& other):
      optional_item<i, LastT>(other.optional_item<i, _LastT>::value),
      _optional_tuple<i + 1, Rest...>(static_cast<const _optional_tuple<i+1, _Rest...>&>(other))
    {}
    template<class... _Rest>
    _optional_tuple(const _optional_tuple<i, void, _Rest...>& other):
      _optional_tuple<i + 1, Rest...>(static_cast<const _optional_tuple<i+1, _Rest...>&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>& other):
      optional_item<i, LastT>(other.optional_item<i, _LastT>::value),
      _optional_tuple<i + 1, Rest...>(static_cast<_optional_tuple<i+1, _Rest...>&>(other))
    {}
    template<class... _Rest>
    _optional_tuple(_optional_tuple<i, void, _Rest...>& other):
      _optional_tuple<i + 1, Rest...>(static_cast<_optional_tuple<i+1, _Rest...>&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>&& other):
      optional_item<i, LastT>(move(other.optional_item<i, _LastT>::value)),
      _optional_tuple<i + 1, Rest...>(static_cast<_optional_tuple<i+1, _Rest...>&&>(other))
    {}
    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, void, _Rest...>&& other):
      _optional_tuple<i + 1, Rest...>(static_cast<_optional_tuple<i+1, _Rest...>&&>(other))
    {}
  };

  // specialization for the case that the next field is 'void'
  // NOTE: all initialization will just be delegated
  template<size_t i, class... Rest>
  struct _optional_tuple<i, void, Rest...>: public _optional_tuple<i + 1, Rest...> {
    _optional_tuple(){};

    template<class _LastT, class... _Rest> requires (!std::is_base_of_v<_optional_tuple<i + 1 + sizeof...(Rest)>, std::remove_cvref_t<_LastT>>)
    _optional_tuple(_LastT&& last, _Rest&&... rest):
      _optional_tuple<i + 1, Rest...>(std::forward<_Rest>(rest)...)
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(const std::piecewise_construct_t, _LastT&& last, _Rest&&... rest):
      _optional_tuple<i + 1, Rest...>(std::piecewise_construct, std::forward<_Rest>(rest)...)
    {}

    // construct from any other optional tuple
    template<class _LastT, class... _Rest>
    _optional_tuple(const _optional_tuple<i, _LastT, _Rest...>& other):
      _optional_tuple<i + 1, Rest...>(static_cast<const _optional_tuple<i+1, _Rest...>&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>& other):
      _optional_tuple<i + 1, Rest...>(static_cast<_optional_tuple<i+1, _Rest...>&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>&& other):
      _optional_tuple<i + 1, Rest...>(static_cast<_optional_tuple<i+1, _Rest...>&&>(other))
    {}

  };


  // member access for the optional tuple
  template<size_t i, class LastT, class... Rest>
  LastT& get(_optional_tuple<i, LastT, Rest...>& tuple) {
    return tuple.optional_item<i, LastT>::value;
  }

  template<size_t i, class LastT, class... Rest>
  const LastT& get(const _optional_tuple<i, LastT, Rest...>& tuple) {
    return tuple.optional_item<i, LastT>::value;
  }

  template<size_t i, class T>
  struct has_value { static constexpr bool value = !std::is_void_v<T>; };
  template<size_t i, class LastT, class... Rest>
  struct has_value<i, _optional_tuple<i, LastT, Rest...>> { static constexpr bool value = !std::is_void_v<LastT>; };

  template<class... Ts> requires (not std::disjunction_v<std::is_reference<Ts>...>) // is_reference istead of is_reference_v is correct here! Why, STL???
  struct optional_tuple: public _optional_tuple<0, Ts...> {
    using Parent = _optional_tuple<0, Ts...>;
    using Parent::Parent;

    optional_tuple() = default;
    
    template<size_t i> static constexpr bool has_value = mstd::has_value<i, optional_tuple>::value;

    template<size_t i> requires (has_value<i>)
    auto& get() { return mstd::get<i>(static_cast<Parent&>(*this)); }

    template<size_t i> requires (has_value<i>)
    const auto& get() const { return mstd::get<i>(static_cast<const Parent&>(*this)); }

    template<size_t i>
    auto& get(auto& other) {
      if constexpr (has_value<i>)
        return mstd::get<i>(*this);
      else return other;
    }

    template<size_t i>
    const auto& get(auto& other) const {
      if constexpr (has_value<i>)
        return mstd::get<i>(*this);
      else return other;
    }

  };


  template<class T, class... Ts> requires (var_type_index<T, Ts...>() < sizeof...(Ts))
  auto& get_by_type(optional_tuple<Ts...>& otuple) {
    return mstd::get<var_type_index<T, Ts...>>(otuple);
  }
  template<class T, class... Ts> requires (var_type_index<T, Ts...>() < sizeof...(Ts))
  const auto& get_by_type(const optional_tuple<Ts...>& otuple) {
    return mstd::get<var_type_index<T, Ts...>>(otuple);
  }

  template<size_t i>
  std::ostream& operator<<(std::ostream& os, const _optional_tuple<i>& otuple) { return os; }

  template<size_t i, class First, class... Ts>
  std::ostream& operator<<(std::ostream& os, const _optional_tuple<i, First, Ts...>& otuple) {
    if constexpr (not std::is_void_v<First>) os << get<i>(otuple) << ' ';
    if constexpr (sizeof...(Ts) > 0)
      return os << static_cast<_optional_tuple<i+1, Ts...>>(otuple);
    else return os;
  }

}
