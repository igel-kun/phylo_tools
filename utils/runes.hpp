
#pragma once

#include <memory>

namespace mstd {

  // ================= Type Runes ====================
  // any custom concept allows adding a rune on top:
  //    say, you want to restrict T to be a container, then you would write, f.ex.: template<ContainerType T>
  //    now, if you would also like to accept T = void, then you can make a new concept OptionalContainerType,
  //    but the type-runes allow you to simplify to: template<ContainerType<TR_VoidOK> T> to indicate that void is also OK
  //    by default, references and const are both OK, so a 'const vector<int>&' passes the concept;
  //    you can circumvent this by using the TR_Strict rune: template<ContainerType<TR_Strict> T> -- now 'vector<int>&' no longer passes

  enum TypeRune: int { TR_Strict = 0, TR_RefOK = 0b0001, TR_ConstOK = 0b0010, TR_PtrOK = 0b0100, TR_VoidOK = 0b1000,
    TR_ConstRefOK = 0b0011, TR_RefPtrOK = 0b0101, TR_PtrVoidOK = 0b1100, 
    TR_ConstRefVoidOK = 0b1011, TR_ConstRefPtrOK = 0b0111, TR_RefPtrVoidOK = 0b1101,
    TR_ConstRefPtrVoidOK = 0b1111};

  template<class T> struct remove_pointer { using type = T; };
  template<class T> struct remove_pointer<T*> { using type = T; };
  template<class T> struct remove_pointer<std::unique_ptr<T>> { using type = T; };
  template<class T> struct remove_pointer<std::shared_ptr<T>> { using type = T; };
  
  template<class T> using remove_pointer_t = typename remove_pointer<T>::type;

  template<class T, TypeRune rune>
  struct apply_rune {
    using T1 = std::conditional_t<rune & TR_RefOK, std::remove_reference_t<T>, T>;
    using T2 = std::conditional_t<rune & TR_ConstOK, std::remove_const_t<T1>, T1>;
    using T3 = std::conditional_t<rune & TR_PtrOK, remove_pointer_t<T2>, T2>;
    using type = T3;
    static constexpr bool value = (std::is_void_v<T> && (rune & TR_VoidOK));
  };

  constexpr auto operator+(const TypeRune x, const TypeRune y) { return TypeRune{x | y}; }

  template<class T, TypeRune rune> using apply_rune_t = typename apply_rune<T, rune>::type;
  template<class T, TypeRune rune> constexpr bool apply_rune_v = apply_rune<T, rune>::value;

  // this allows simple constraining of constructors so that they don't overwrite copy/move constructors
  template<class P, class Q, TypeRune rune = TR_ConstRefOK>
  constexpr bool is_same_v = apply_rune_v<P, rune> || std::is_same_v<apply_rune_t<P, rune>, Q>;

  template<class P, TypeRune rune = TR_RefOK>
  constexpr bool is_const_v = apply_rune_v<P, rune> || std::is_const_v<apply_rune_t<P, rune>>;

  template<class P, class Q, TypeRune rune = TR_ConstRefOK>
  constexpr bool is_convertible_v = apply_rune_v<P, rune> || std::is_convertible_v<apply_rune_t<P, rune>, Q>;

  template<class P, TypeRune rune = TR_ConstRefOK, class... Args>
  constexpr bool _is_constructible_v = apply_rune_v<P, rune> || std::is_constructible_v<apply_rune_t<P, rune>, Args...>;

  template<class P, class... Args>
  constexpr bool is_constructible_v = _is_constructible_v<P, TR_ConstRefOK, Args...>;

  template<class P, TypeRune rune, class... Args>
  constexpr bool is_invocable_v = apply_rune_v<P, rune> || std::is_invocable_v<apply_rune_t<P, rune>, Args...>;

  template<class P, TypeRune rune = TR_ConstRefOK, class... Args>
  constexpr bool predicate = apply_rune_v<P, rune> || std::predicate<apply_rune_t<P, rune>, Args...>;

  template<class P, TypeRune rune = TR_ConstRefOK>
  constexpr bool is_arithmetic_v = apply_rune_v<P, rune> || std::is_arithmetic_v<apply_rune_t<P, rune>>;

  template<class P, TypeRune rune = TR_Strict> // NOTE: strict by default
  constexpr bool is_pointer_v = apply_rune_v<P, rune> || std::is_pointer_v<apply_rune_t<P, rune>>;


  static_assert(mstd::is_same_v<std::unique_ptr<int>, int, TR_PtrOK>);
}
