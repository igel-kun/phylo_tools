

#pragma once


namespace mstd {

  //! an exception for the case that a graph property is read that is not up to date
  struct NeedSorted : public std::exception  {
    const std::string msg;

    NeedSorted(const std::string& _func):
      msg(_func + " needs a sorted data structure") {}

    const char* what() const throw() {
      return msg.c_str();
    }
  };

  //! an exception for the case that a graph property is read that is not up to date
  struct Unimplemented : public std::exception  {
    const std::string msg;

    Unimplemented(const std::string& _func):
      msg(_func + " not yet implemented, sorry") {}

    const char* what() const throw() {
      return msg.c_str();
    }
  };

  //! an exception for problems with the input string
  struct MalformedInput : public std::logic_error {
    using Parent = std::logic_error;
    using Parent::Parent;

    MalformedInput(const std::string_view context, const ssize_t _pos, const std::string& _msg = "unknown error"):
      Parent(_msg + " (position " + std::to_string(_pos) + ")" + DEBUG3(" - relevant substring: " + context.substr(_pos)) + "") {}

    MalformedInput(const ssize_t _pos, const std::string& _msg = "unknown error"):
      Parent(_msg + " (position " + std::to_string(_pos) + ")") {}
  };


}
