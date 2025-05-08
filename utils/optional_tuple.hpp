
#pragma once

#include "stl_utils.hpp"

namespace mstd {
  template<size_t i, class T>
  struct optional_item {
    T value;
  };
  template<size_t i> struct optional_item<i, void> {};

  // optional pointers can be initialized from references
  template<size_t i, class T> requires (std::is_pointer_v<T> and not std::is_void_v<std::remove_pointer_t<T>>)
  struct optional_item<i, T> {
    using BareT = std::remove_pointer_t<T>;
    T value = nullptr;

    optional_item() = default;
    optional_item(T t): value{t} {}
    optional_item(BareT& ref): value{&ref} {}

    optional_item(const std::unique_ptr<BareT>& pt): value{pt.get()} {}
    optional_item(std::unique_ptr<BareT>&&) = delete; // we cannot take ownership if we only have an observing pointer...
    optional_item(const std::shared_ptr<BareT>& pt): value{pt.get()} {}
  };
  template<size_t i> struct optional_item<i, void*> { void* value = nullptr; };

  template<size_t i, class T>
  struct optional_item<i, std::unique_ptr<T>> {
    using BareT = std::remove_pointer_t<T>;
    std::unique_ptr<T> value = nullptr;

    optional_item() = default;
    optional_item(optional_item&& other): optional_item(std::move(other.value)) {}
    optional_item(const optional_item& other): optional_item(other.value) {}

    optional_item(std::unique_ptr<T>&& t): value{std::move(t)} {}
    optional_item(T* t) { if(t != nullptr) value = std::make_unique<T>(*t); }
    optional_item(const std::unique_ptr<T>& t): optional_item(t.get()) {}
    optional_item(const std::shared_ptr<T>& t): optional_item(t.get()) {}

    optional_item(const T& ref): value{std::make_unique<T>(ref)} {}
    optional_item(T&& ref): value{std::make_unique<T>(std::move(ref))} {}

    optional_item& operator=(optional_item&& other) = default;
    optional_item& operator=(const optional_item& other) { if(other.value) value = std::make_unique<T>(other.value); else value.reset(); }
  };

  static_assert(std::copy_constructible<optional_item<0, std::unique_ptr<int>>>);

  template<size_t i, class T>
  struct optional_item<i, std::shared_ptr<T>> {
    using BareT = std::remove_pointer_t<T>;
    std::shared_ptr<T> value = nullptr;

    optional_item() = default;
    optional_item(optional_item&& other): optional_item(std::move(other.value)) {}
    optional_item(const optional_item& other): optional_item(other.value) {}

    optional_item(std::unique_ptr<T>&& t): value{std::move(t)} {}
    optional_item(std::shared_ptr<T>&& t): value{std::move(t)} {}
    optional_item(T* t) { if(t != nullptr) value = std::make_shared<T>(*t); }
    optional_item(const std::unique_ptr<T>& t): optional_item(t.get()) {} // make a copy :/
    optional_item(const std::shared_ptr<T>& t): value(t) {}

    optional_item(const T& ref): value{std::make_shared<T>(ref)} {}
    optional_item(T&& ref): value{std::make_unique<T>(std::move(ref))} {}

    optional_item& operator=(optional_item&& other) = default;
    optional_item& operator=(const optional_item& other) = default;
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
    using Item = optional_item<i, LastT>;
    using Parent = _optional_tuple<i + 1, Rest...>;

    _optional_tuple() = default;

    template<class _LastT, class... _Rest>
      requires (not std::is_base_of_v<_optional_tuple<i + 1 + sizeof...(Rest)>, std::remove_cvref_t<_LastT>> and
          std::is_constructible_v<Item, _LastT&&> and
          std::is_constructible_v<Parent, _Rest&&...>)
    _optional_tuple(_LastT&& last, _Rest&&... rest):
      Item(std::forward<_LastT>(last)),
      Parent(std::forward<_Rest>(rest)...)
    {}

    template<class _LastT, class... _Rest>
        requires(std::is_constructible_v<Parent, _Rest&&...>)
    _optional_tuple(const std::piecewise_construct_t, _LastT&& last, _Rest&&... rest):
      Item(std::make_from_tuple(std::forward<_LastT>(last))),
      Parent(std::piecewise_construct, std::forward<_Rest>(rest)...)
    {}

    // construct from any other optional tuple
    // NOTE: if the field i in the other optional tuple is 'void' then we skip initialization of field i of this optional tuple
    template<class _LastT, class... _Rest>
    _optional_tuple(const _optional_tuple<i, _LastT, _Rest...>& other):
      Item(static_cast<const typename _optional_tuple<i, _LastT, _Rest...>::Item&>(other).value),
      Parent(static_cast<const typename _optional_tuple<i, _LastT, _Rest...>::Parent&>(other))
    {}
    template<class... _Rest>
    _optional_tuple(const _optional_tuple<i, void, _Rest...>& other):
      Parent(static_cast<const typename _optional_tuple<i, void, _Rest...>::Parent&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>& other):
      Item(static_cast<typename _optional_tuple<i, _LastT, _Rest...>::Item&>(other).value),
      Parent(static_cast<typename _optional_tuple<i, _LastT, _Rest...>::Parent&>(other))
    {}
    template<class... _Rest>
    _optional_tuple(_optional_tuple<i, void, _Rest...>& other):
      Parent(static_cast<typename _optional_tuple<i, void, _Rest...>::Parent&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>&& other):
      Item(static_cast<typename _optional_tuple<i, _LastT, _Rest...>::Item&&>(other).value),
      Parent(static_cast<typename _optional_tuple<i, _LastT, _Rest...>::Parent&&>(other))
    {}
    template<class... _Rest>
    _optional_tuple(_optional_tuple<i, void, _Rest...>&& other):
      Parent(static_cast<typename _optional_tuple<i, void, _Rest...>::Parent&&>(other))
    {}
    // base case: default initialize everything that hasn't been initialized by the other tuple
    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i>&& other) {}

  };

  // specialization for the case that the next field is 'void'
  // NOTE: all initialization will just be delegated
  template<size_t i, class... Rest>
  struct _optional_tuple<i, void, Rest...>:
    public _optional_tuple<i + 1, Rest...>
  {
    using Parent = _optional_tuple<i + 1, Rest...>;

    _optional_tuple(){};

    // NOTE: void fields DO NOT consume passed arguments unless piecewise_construct is given!!!
    template<class _LastT, class... _Rest> requires (!std::is_base_of_v<_optional_tuple<i + 1 + sizeof...(Rest)>, std::remove_cvref_t<_LastT>>)
    _optional_tuple(_LastT&& last, _Rest&&... rest):
      Parent(std::forward<_LastT>(last), std::forward<_Rest>(rest)...)
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(const std::piecewise_construct_t, _LastT&& last, _Rest&&... rest):
      Parent(std::piecewise_construct, std::forward<_Rest>(rest)...)
    {}

    // construct from any other optional tuple
    template<class _LastT, class... _Rest>
    _optional_tuple(const _optional_tuple<i, _LastT, _Rest...>& other):
      Parent(static_cast<const _optional_tuple<i+1, _Rest...>&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>& other):
      Parent(static_cast<_optional_tuple<i+1, _Rest...>&>(other))
    {}

    template<class _LastT, class... _Rest>
    _optional_tuple(_optional_tuple<i, _LastT, _Rest...>&& other):
      Parent(static_cast<_optional_tuple<i+1, _Rest...>&&>(other))
    {}

  };


  // member access for the optional tuple
  template<size_t i, class LastT, class... Rest>
  auto& get(_optional_tuple<i, LastT, Rest...>& otuple) {
    using Item = typename _optional_tuple<i, LastT, Rest...>::Item;
    return static_cast<Item&>(otuple).value;
  }

  template<size_t i, class LastT, class... Rest>
  const auto& get(const _optional_tuple<i, LastT, Rest...>& otuple) {
    using Item = typename _optional_tuple<i, LastT, Rest...>::Item;
    return static_cast<const Item&>(otuple).value;
  }

  template<size_t i, class T>
  struct has_value { static constexpr bool value = !std::is_void_v<T>; };
  template<size_t i, class LastT, class... Rest>
  struct has_value<i, _optional_tuple<i, LastT, Rest...>> { static constexpr bool value = !std::is_void_v<LastT>; };

  template<class... Ts> requires (not std::disjunction_v<std::is_reference<Ts>...>) // is_reference istead of is_reference_v is correct here! Why, STL???
  struct optional_tuple: public _optional_tuple<0, Ts...> {
    using Parent = _optional_tuple<0, Ts...>;

    optional_tuple() = default;
    INHERIT_ALL_CONSTRUCTORS(optional_tuple, Parent);

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
