
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

    //! post-increment
    TokenIter operator++(int) {
      const size_t old_front = front;
      const size_t old_next = next;
      ++(*this);
      return TokenIter(s, delim, old_front, old_next);
    }

    std::pair<size_t,size_t> current_indices() const  {
      return {front, next};
    }
  };

  template<class Delim>
  using Tokenizer = mstd::IterFactory<TokenIter<Delim>>;

  template<class Delim>
  auto tokenize(const std::string_view sv, Delim&& delim) {
    return Tokenizer<Delim>(sv, std::forward<Delim>(delim));
  }

}

