
#pragma once

#include <optional>

namespace std {
  // a class implementing std::optional but, instead of using an additional byte, we use an invalid value (much like nullptr)
  template<class T, T _invalid>
  class optional_by_invalid {
    using value_type = T;
    T element = _invalid;

    // in-place construct the element
    template<class... Args>
    explicit optional_by_invalid(const std::in_place_t, Args&&... args): element(std::forward<Args>(args)...) {}
    template<class U, U I>
    optional_by_invalid(const optional_by_invalid<U, I>& other): element(other.element) {}
    template<class U, U I>
    optional_by_invalid(optional_by_invalid<U, I>&& other): element(move(other.element)) {}
    template<class U = T>
    optional_by_invalid(const U& other): element(other) {}
    template<class U = T>
    optional_by_invalid(U&& other): element(move(other)) {}

    // re-construct the element
    template<class... Args>
    constexpr T& emplace(Args&&... args) {
      T const * addr = &element;
      addr->~T();
      new(addr) T(std::forward<Args>(args)...);
      return *addr;
    }
    
    T& operator*() { return element; }
    const T& operator*() const { return element; }
    T& value() { return element; }
    const T& value() const { return element; }

    template<class U>
    T& value_or(U&& default_value) { return has_value() ? element : static_cast<T>(std::forward<U>(default_value)); }
    template<class U>
    const T& value_or(U&& default_value) const { return has_value() ? element : static_cast<T>(std::forward<U>(default_value)); }

    void reset() { emplace(_invalid); }
    operator bool() const { return has_value(); }
    bool has_value() const { return element != _invalid; }

  };

  template<class T>
  static constexpr bool is_optional_v = false;
  template<class T>
  static constexpr bool is_optional_v<optional<T>> = true;
  template<class T, T _invalid>
  static constexpr bool is_optional_v<optional_by_invalid<T, _invalid>> = true;

  template<class T>
  concept Optional = is_optional_v<remove_reference_t<T>>;
}
