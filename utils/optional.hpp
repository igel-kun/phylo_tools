
#pragma once

#include <cassert>
#include <limits>
#include <optional>

namespace mstd {
  // a class implementing std::optional but, instead of using an additional byte, we use a tombstone

  // ------- optional_by_invalid: helpers ------------
  // by default, a default-constructed thing is invalid
  // specialize to your hearts desire
  template<class T> struct default_invalid {};
  template<class T> requires (std::is_pointer_v<T>)
  struct default_invalid<T> { static constexpr T value() { return nullptr; } };
  template<class T> requires (std::is_arithmetic_v<T>)
  struct default_invalid<T> {
    static constexpr auto value() {
      if constexpr (std::numeric_limits<T>::has_quiet_NaN) {
        return std::numeric_limits<T>::quiet_NaN();
      } else if constexpr (std::numeric_limits<T>::has_infinity) {
        return std::numeric_limits<T>::infinity();
      } else return std::numeric_limits<T>::max();
    }
  };

  template<class T> constexpr auto default_invalid_v = default_invalid<T>::value();
 


  // ------- optional_by_invalid: main class ---------
  // NOTE: tombstone can be either
  //  1. a value that is equal-comparable to T or
  //  2. a constexpr lambda returning the tombstone on operator()
  template<class T, auto tombstone = default_invalid_v<T>>
    requires ((std::equality_comparable_with<T, decltype(tombstone)>) or (std::is_invocable_v<decltype(tombstone)>))
  struct optional_by_invalid {
    // ------- static stuff --------
    static constexpr auto get_tombstone() {
      if constexpr (std::equality_comparable_with<T, decltype(tombstone)>) {
        return tombstone;
      } else if constexpr (std::is_invocable_v<decltype(tombstone)>)
        return tombstone();
      else assert(false and "received invalid choice for tombstone");
    }
    
    using Tombstone = decltype(get_tombstone());
    static_assert(std::is_constructible_v<T, Tombstone&&> and std::is_assignable_v<T&, Tombstone&&>);

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;

    static constexpr bool detect_optional = true;

    // ------- members --------
    T element{get_tombstone()};
    
    // ------- construction & desctruction ---------
    constexpr optional_by_invalid() noexcept = default;

    // in-place construct the element
    template<class... Args>
    constexpr optional_by_invalid(const std::in_place_t, Args&&... args) noexcept:
      element(std::forward<Args>(args)...) {}

    template<class U, auto I>
      requires (std::is_constructible_v<T, const U&> and not mstd::is_same_v<optional_by_invalid, optional_by_invalid<U,I>>)
    constexpr optional_by_invalid(const optional_by_invalid<U, I>& other) noexcept:
      element(other.element) {}

    template<class U, auto I>
      requires (std::is_constructible_v<T, U&&> and not mstd::is_same_v<optional_by_invalid, optional_by_invalid<U,I>>)
    constexpr optional_by_invalid(optional_by_invalid<U, I>&& other) noexcept:
      element(std::move(other.element)) {}
    
    template<class U> requires (std::is_constructible_v<T, U&&>)
    constexpr optional_by_invalid(U&& other) noexcept:
      element(std::forward<U>(other)) {}


    // ------- operators --------
    T& operator*() { return element; }
    const T& operator*() const { return element; }

    operator T&() { return element; }
    operator const T&() const { return element; }
    
    explicit operator bool() const { return has_value(); }

    bool operator==(const optional_by_invalid& other) const { return element == other.element; }
    bool operator==(const T& other) const { return element == other; }

    //optional_by_invalid& operator=(const optional_by_invalid& other) = default;
    //optional_by_invalid& operator=(optional_by_invalid&& other) = default;
    optional_by_invalid& operator=(const T& other) noexcept { element = other; return *this; }
    optional_by_invalid& operator=(T&& other) noexcept { element = std::move(other); return *this; }
    
    // ------- methods: initialization --------
    // ------- methods: query --------
    T& value() { return element; }
    const T& value() const { return element; }

    template<class U>
    T& value_or(U&& default_value) { return has_value() ? element : static_cast<T>(std::forward<U>(default_value)); }
    template<class U>
    const T& value_or(U&& default_value) const { return has_value() ? element : static_cast<T>(std::forward<U>(default_value)); }

    bool has_value() const { return element != get_tombstone(); }

    // ------- methods: modification --------
    void reset() { emplace(get_tombstone()); }

    // re-construct the element
    template<class... Args>
    constexpr T& emplace(Args&&... args) {
      T* const addr = &element;
      addr->~T();
      new(addr) T(std::forward<Args>(args)...);
      return *addr;
    }
    template<class U> requires std::is_assignable_v<T&, U&&>
    constexpr T& emplace(U&& other) {
      element = std::forward<U>(other);
      return element;
    }


    friend std::ostream& operator<<(std::ostream& os, const optional_by_invalid& opt) {
      return os << printable{&(opt.element)};
    }
  };

  // ------- optional_by_invalid: deduction guides ---
  template<class T>
  optional_by_invalid(T) -> optional_by_invalid<T, default_invalid_v<T>>;

  // ------- optional_by_invalid: concepts -----------
  template<class T> constexpr bool std_optional_v = false;
  template<class T> constexpr bool std_optional_v<std::optional<T>> = true;
  template<class T> concept Optional = std_optional_v<std::remove_reference_t<T>> || requires(T t){ T::detect_optional; };
  
  // ------- optional_by_invalid: defaults -----------

  // OptFor tries its best to guess an Optional for a given type T
  template<class T, auto ts> struct OptFor_ { };
  // if T is already an Optional, just use that one
  template<Optional T, auto ts> struct OptFor_<T, ts> { using type = T; };
  // otherwise, use optional_by_invalid
  template<class T, auto ts> requires (not Optional<T>)
  struct OptFor_<T, ts> { using type = mstd::optional_by_invalid<T, ts>; };
  
  template<class T, auto ts = default_invalid_v<std::remove_cvref_t<T>>>
  using OptFor = typename OptFor_<T, ts>::type;
 
  // ValFor guesses the value of something that may be an Optional
  template<class T> struct ValFor_ { using type = T; };
  template<Optional T>
  struct ValFor_<T> { using type = typename T::value_type; };
  
  template<class T>
  using ValFor = typename ValFor_<T>::type;


  template<bool invert = false>
  struct HasValuePredicate_ {
    static constexpr bool value(const auto& x) { return (x.has_value()) != invert; }
    constexpr bool operator()(const auto& x) const { return value(x); }
  };
  using HasValuePredicate = HasValuePredicate_<false>;
  using HasNoValuePredicate = HasValuePredicate_<true>;

}

namespace std {
  template<mstd::Optional T>
  struct hash<T>: public std::hash<typename T::value_type> {
    size_t operator()(const T& x) const {
      if(x) return this->operator()(*x); else return 0;
    }
  };

}
