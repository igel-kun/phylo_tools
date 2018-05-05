
#pragma once

#include <string>

namespace std{

  class Tokenizer
  {
    const std::string& s;
    const char delim;
    size_t front, next;
  public:
    Tokenizer(const std::string& input_string, const char delimeter, const size_t _front = 0, const size_t _next = 0):
      s(input_string), delim(delimeter), front(_front), next(_next == 0 ? input_string.find(delimeter) : _next)
    {}

    ~Tokenizer() {}

    bool is_valid() const
    {
      return next != std::string::npos;
    }

    operator bool() const
    {
      return is_valid();
    }

    std::string operator*() const
    {
      return s.substr(front, next - front + 1);
    }

    //! increment operator
    Tokenizer& operator++()
    {
      front = next + 1;
      next = s.find(delim, front);
      return *this;
    }

    //! post-increment
    Tokenizer operator++(int)
    {
      const size_t old_front = front;
      const size_t old_next = next;
      ++(*this);
      return Tokenizer(s, delim, old_front, old_next);
    }

    std::pair<size_t,size_t> current_indices() const 
    {
      return {front, next};
    }

}
