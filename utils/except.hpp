

#pragma once


namespace PT{

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



}
