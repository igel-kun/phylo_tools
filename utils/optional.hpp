
#pragma once

#include <optional>

namespace mstd {
  // a class implementing std::optional but, instead of using an additional byte, we use an invalid value (much like nullptr)
  template<class T, T _invalid>
  class optional_by_invalid {
    T element = _invalid;
  public:
    using value_type = T;

    constexpr optional_by_invalid() = default;
    //constexpr optional_by_invalid(const optional_by_invalid&) = default;
    //constexpr optional_by_invalid(optional_by_invalid&&) = default;

    // in-place construct the element
    template<class... Args>
    constexpr optional_by_invalid(const std::in_place_t, Args&&... args): element(std::forward<Args>(args)...) {}
    template<class U, U I>
    constexpr optional_by_invalid(const optional_by_invalid<U, I>& other): element(other.element) {}
    template<class U, U I>
    constexpr optional_by_invalid(optional_by_invalid<U, I>&& other): element(move(other.element)) {}
    template<class U = T>
    constexpr optional_by_invalid(const U& other): element(other) {}
    template<class U = T>
    constexpr optional_by_invalid(U&& other): element(move(other)) {}

    // re-construct the element
    template<class... Args>
    constexpr T& emplace(Args&&... args) {
      T* const addr = &element;
      addr->~T();
      new(addr) T(std::forward<Args>(args)...);
      return *addr;
    }
    template<class Q> requires (std::is_trivially_assignable_v<T&, Q&&>)
    constexpr T& emplace(Q&& other) {
      element = std::forward<Q>(other);
      return element;
    }

    
    T& operator*() { return element; }
    const T& operator*() const { return element; }
    T& value() { return element; }
    const T& value() const { return element; }
    operator T() { return element; }
    operator T() const { return element; }


    bool operator==(const optional_by_invalid& other) const { return element == other.element; }
    bool operator==(const T& other) const { return element == other; }

    //optional_by_invalid& operator=(const optional_by_invalid& other) = default;
    //optional_by_invalid& operator=(optional_by_invalid&& other) = default;
    optional_by_invalid& operator=(const T& other) { element = other; return *this; }
    optional_by_invalid& operator=(T&& other) { element = move(other); return *this; }


    template<class U>
    T& value_or(U&& default_value) { return has_value() ? element : static_cast<T>(std::forward<U>(default_value)); }
    template<class U>
    const T& value_or(U&& default_value) const { return has_value() ? element : static_cast<T>(std::forward<U>(default_value)); }

    void reset() { emplace(_invalid); }
    operator bool() const { return has_value(); }
    bool has_value() const { return element != _invalid; }

    friend std::ostream& operator<<(std::ostream& os, const optional_by_invalid& opt) {
      return os << opt.element;
    }
  };

  template<class T>
  static constexpr bool is_optional_v = false;
  template<class T>
  static constexpr bool is_optional_v<std::optional<T>> = true;
  template<class T, T _invalid>
  static constexpr bool is_optional_v<optional_by_invalid<T, _invalid>> = true;

  template<class T>
  concept Optional = is_optional_v<std::remove_reference_t<T>>;

}
