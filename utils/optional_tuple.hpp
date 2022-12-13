
#pragma once

namespace mstd {
/*

  template<class T1> struct optional_member {
    static constexpr bool has_first = true;
    T1 first;
  }
  template<class T1> struct optional_tuple1<void> {
    static constexpr bool has_first = false;
  }

  template<class T1, class T2> struct optional_tuple2: public optional_tuple1<T1> {
    static constexpr bool has_second = true;
    T2 second;

    template<class Tuple1, class Tuple2>
    optional_tuple2(const std::piecewise_construct_t, Tuple1&& t1, Tuple2&& t2):
      optional_tuple1(std::forward_as_tuple<Tuple1>(t1)),
      second{std::forward_as_tuple<Tuple2>(t2)}
    {}
  }
  template<class T1, class T2> struct optional_tuple2<void>: public optional_tuple1<T1> {
    using optional_tuple1<T1>::optional_tuple1;
    static constexpr bool has_second = false;
  }




  template<class T1, class T2, class T3> struct optional_tuple3: public optional_tuple2<T1, T2> {
    static constexpr bool has_third = true;
    T3 third;

    template<class Tuple1, class Tuple2, class Tuple3>
    optional_tuple3(const std::piecewise_construct_t, Tuple1&& t1, Tuple2&& t2, Tuple3&& t3):
      optional_tuple2(std::piecewise_construct, std::forward<Tuple1>(t1), std::forward<Tuple2>(t2)),
      third{std::forward_as_tuple<Tuple3>(t3)}
    {}
  }
  template<class T1, class T2, class T3> struct optional_tuple3<void>: public optional_tuple2<T1, T2> {
    using optional_tuple2<T1, T2>::optional_tuple2;
    static constexpr bool has_third = false;
  }

  template<class T1, class T2, class T3, class T4> struct optional_tuple4: public optional_tuple3<T1, T2, T3> {
    static constexpr bool has_fourth = true;
    T4 fourth;

    template<class Tuple1, class Tuple2, class Tuple3, class Tuple4>
    optional_tuple4(const std::piecewise_construct_t, Tuple1&& t1, Tuple2&& t2, Tuple3&& t3, Tuple4&& t4):
      optional_tuple3(std::piecewise_construct, std::forward<Tuple1>(t1), std::forward<Tuple2>(t2), std::forward<Tuple3>(t3)),
      fourth{std::forward_as_tuple<Tuple4>(t4)}
    {}
  }
  template<class T1, class T2, class T3, class T5> struct optional_tuple4<void>: public optional_tuple3<T1, T2, T3> {
    using optional_tuple3<T1, T2, T3>::optional_tuple;
    static constexpr bool has_fourth = false;
  }
*/



  template<size_t i, class T>
  struct optional_item {
    using value_type = T;
    T value;
    static constexpr bool has_value = true;

//    optional_item() { }
//
//    optional_item(const optional_item&) = default;
//    optional_item(optional_item&&) = default;
    
    template<class... Args> requires (!std::is_reference_v<T> || (sizeof...(Args) != 0))
    optional_item(Args&&... args): value(std::forward<Args>(args)...) {}

    operator T&() { return value; }
    operator const T&() const { return value; }
  };
  template<size_t i>
  struct optional_item<i, void> {
    static constexpr bool has_value = false;
    optional_item(){}
    template<class T> optional_item(const T&) {}
  };

  template<size_t i, class... T>
  struct _optional_tuple;

  template<size_t i>
  struct _optional_tuple<i>{
    _optional_tuple(const std::piecewise_construct_t = std::piecewise_construct) {}
  };

  template<size_t i, class LastT, class... Rest>
  struct _optional_tuple<i, LastT, Rest...>:
    public optional_item<i, LastT>,
    public _optional_tuple<i + 1, Rest...>
  {
    _optional_tuple(){};

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
  LastT& _get(_optional_tuple<i, LastT, Rest...>& tuple) {
    return tuple.optional_item<i, LastT>::value;
  }

  template<size_t i, class LastT, class... Rest>
  const LastT& _get(const _optional_tuple<i, LastT, Rest...>& tuple) {
    return tuple.optional_item<i, LastT>::value;
  }

  template<size_t i, class T>
  struct _has_value { static constexpr bool value = !std::is_void_v<T>; };
  template<size_t i, class LastT, class... Rest>
  struct _has_value<i, _optional_tuple<i, LastT, Rest...>> { static constexpr bool value = !std::is_void_v<LastT>; };

  template<class... Ts>
  struct optional_tuple: public _optional_tuple<0, Ts...> {
    using _optional_tuple<0, Ts...>::_optional_tuple;

    optional_tuple() = default;
    
    template<size_t i> static constexpr bool has_value = _has_value<i, optional_tuple>::value;

    template<size_t i> requires (has_value<i>)
    auto& get() { return _get<i>(*this); }

    template<size_t i> requires (has_value<i>)
    const auto& get() const { return _get<i>(*this); }

    template<size_t i>
    auto& get(auto& other) {
      if constexpr (has_value<i>)
        return _get<i>(*this);
      else return other;
    }

    template<size_t i>
    const auto& get(auto& other) const {
      if constexpr (has_value<i>)
        return _get<i>(*this);
      else return other;
    }

  };

}
