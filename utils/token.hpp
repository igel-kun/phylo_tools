
#pragma once

#include <string>
#include <string_view>

#include "iter_factory.hpp"

namespace mstd {

  // this iterator 
  // NOTE: if you use a char[] to store your delimeters, then please make sure it's zero-terminated! (char bla[] = "abc"; will be zero-terminated)
  template<class DelimRef = char>
  class TokenIter {
    // if Delim is basically a char*, then make it a const char* instead
    using _Delim = std::remove_cvref_t<DelimRef>;
    using Delim = std::conditional_t<std::is_pointer_v<_Delim> || std::is_array_v<_Delim>,
                    const std::remove_pointer_t<std::decay_t<_Delim>>*, _Delim>;
    static constexpr bool single_delim = std::is_same_v<Delim, char>;

    std::string_view s;
    Delim delim;
    size_t front, next;
  public:
    using value_type = std::string_view;
    using reference  = value_type;
    using const_reference = const value_type;
    using pointer    = mstd::pointer_from_reference<reference>;
    using difference_type = ptrdiff_t;
    using iterator_category = std::forward_iterator_tag; // iterator can be copied and re-used in multi-passes (as long as the underlying string exists)
  
    TokenIter(const std::string_view input_string, const Delim& delimeter, const size_t _front = 0, const size_t _next = 0):
      s(input_string), delim(delimeter), front(_front), next(_next == 0 ? input_string.find_first_of(delimeter) : _next)
    {}

    bool is_valid() const { return front != std::string::npos; }
    explicit operator bool() const { return is_valid(); } 
    reference operator*() const { return s.substr(front, next - front); }

    //! increment operator
    TokenIter& operator++() {
      if(next != std::string::npos) {
        front = next + 1;
        next = s.find_first_of(delim, front);
      } else front = std::string::npos;
      return *this;
    }
    auto operator++(int) { TokenIter result(*this); ++(*this); return result; }

    bool operator==(const TokenIter other) const {
      return (front == other.front) && (s.data() == other.s.data()) && (s.size() == other.s.size());
    }

    std::pair<size_t,size_t> current_indices() const  { return {front, next}; }
  };

  // TokenIter should have iterator_traits now.... hopefully
  static_assert(__LegacyInputIterator<TokenIter<>>);

  template<class Delim = char>
  using Tokenizer = mstd::IterFactory<TokenIter<Delim>>;

  template<class Delim = char>
  auto tokenize(const std::string_view sv, Delim&& delim) {
    return Tokenizer<Delim>(sv, std::forward<Delim>(delim));
  }


  // tokenize and parse a string_view into a tuple
  // Converter provides operator()<T>(string_view) that parses a T from a string_view
  // NOTE: if the Converter is not invocable with a string_view,
  //    then it is interpreted as part of the tuple template args and a default converter is used
  template<class _Converter, class... Args>
  struct TupleParser {
    static constexpr bool conv_invocable = std::is_invocable_v<_Converter, std::string_view>;
    
    // if the first template argument is not invocable with string_view, then interpret it as first tuple element and use the default converter
    using Converter = std::conditional_t<conv_invocable, _Converter, AnythingFromString<>>;
    using Tuple = std::conditional_t<conv_invocable, std::tuple<Args...>, std::tuple<Converter, Args...>>;
    static constexpr size_t num_items = std::tuple_size_v<Tuple>;

    [[ no_unique_address ]] Converter conv;
    std::string delims = ",;:";

    auto to_vec(const std::string_view input) const {
      std::vector<std::string_view> tokens;
      tokens.reserve(num_items);
      for(const auto token: tokenize(input, delims))
        tokens.emplace_back(token);
      return tokens;
    }

    Tuple operator()(const std::string_view input) const {
      const auto vec = to_vec(input);
      const auto apply = [&]<class T>(const size_t index) { if(index < vec.size()) return conv.template operator()<T>(vec[index]); else return T{};};
      return tuple_generate<Tuple>(apply);
    }
    Tuple operator()(const std::string_view input) {
      const auto vec = to_vec(input);
      auto apply = [&]<class T>(const size_t index) { if(index < vec.size()) return conv.template operator()<T>(vec[index]); else return T{};};
      return tuple_generate<Tuple>(apply);
    }
  };

  // parse a T from a string by tokenizing the string into arguments of types Args...
  template<class T, class... Args>
  struct DefaultParser: public TupleParser<Args...> {
    using Parent = TupleParser<Args...>;

    T operator()(const std::string_view input) const { return std::make_from_tuple<T>(Parent::operator()(input)); }
    T operator()(const std::string_view input) { return std::make_from_tuple<T>(Parent::operator()(input)); }
  };



}

