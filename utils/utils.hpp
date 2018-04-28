
/** \file utils.hpp
 * collection of general utilities
 */

#pragma once

#include <limits.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <bitset>
#include <list>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#define PWC std::piecewise_construct
#define byte unsigned char
#define SIZE_T_BITS ((unsigned)(CHAR_BIT*sizeof(size_t)))
// avoid the clutter for map-emplaces where both constructors take just one/two arguments
#define DEEP_EMPLACE(x,y) emplace(PWC, std::make_tuple(x), std::make_tuple(y))
#define DEEP_EMPLACE_TWO(x,y,z) emplace(PWC, std::make_tuple(x), std::make_tuple(y, z))
#define DEEP_EMPLACE_HINT(x,y,z) emplace_hint(x, PWC, std::make_tuple(y), std::make_tuple(z))
// avoid the clutter for map-emplaces where the target is default constructed
#define DEFAULT_EMPLACE(x) emplace(PWC, std::make_tuple(x), std::make_tuple())
#define DEFAULT_EMPLACE_HINT(x,y) emplace(x, PWC, std::make_tuple(y), std::make_tuple())
// merge two lists
#define SPLICE_LISTS(x,y) x.splice(x.end(), y)
//! a more readable containment check
#define contains(x,y) ((x).find(y) != (x).cend())
// find x in the container "list" and, ONLY IF NOT FOUND, execute constructor, assign findings to result (which should be a reference to value_type)
#define FIND_OR_CONSTRUCT(result,x,list,constructor) {auto ___it = list.find(x); if(___it == list.cend()) ___it = list.insert(___it, {x, constructor}); result = ___it->second;}
// circular shift
#define rotl(x,y) ((x << y) | (x >> (sizeof(x)*CHAR_BIT - y)))
#define rotr(x,y) ((x >> y) | (x << (sizeof(x)*CHAR_BIT - y)))


// on debuglevel 3 all DEBUG1, DEBUG2, and DEBUG3 statements are evaluated
#ifndef NDEBUG
  #ifndef debuglevel
    #define debuglevel 5
  #endif
#else
  #define debuglevel 0
#endif


#if debuglevel > 0
#define DEBUG1(x) x
#else
#define DEBUG1(x)
#endif

#if debuglevel > 1
#define DEBUG2(x) x
#else
#define DEBUG2(x)
#endif

#if debuglevel > 2
#define DEBUG3(x) x
#else
#define DEBUG3(x)
#endif

#if debuglevel > 3
#define DEBUG4(x) x
#else
#define DEBUG4(x)
#endif

#if debuglevel > 4
#define DEBUG5(x) x
#else
#define DEBUG5(x)
#endif

#if debuglevel > 5
#define DEBUG6(x) x
#else
#define DEBUG6(x)
#endif

#ifdef STATISTICS
#define STAT(x) x
#else
#define STAT(x) 
#endif


//! output list of things
template<typename Element>
std::ostream& operator<<(std::ostream& os, const std::list<Element>& lst)
{
  os << "[";
  for(auto i : lst) os << i << "  ";
  os << "]";
  return os;
}
//! output vector of things
template<typename Element>
std::ostream& operator<<(std::ostream& os, const std::vector<Element>& lst)
{
  os << "[";
  for(auto i : lst) os << i << "  ";
  os << "]";
  return os;
}

//! output map of things
template<typename Element1, typename Element2>
std::ostream& operator<<(std::ostream& os, const std::unordered_map<Element1, Element2>& map)
{
  os << "[";
  for(auto i : map) os << i << "  ";
  os << "]";
  return os;
}


//! a hash computation for an unordered set, XORing its members
template<typename T>
size_t hash_value(const std::unordered_set<T>& S)
{
  size_t result = 0;
  for(const auto& i : S)
    result = rotl(result, 1) ^ hash_value(i);
  return result;
}

//! testing whether a file exists by trying to open it
#define file_exists(x) (std::ifstream(x).good())

//! reverse a pair of things, that is, turn (x,y) into (y,x)
template<typename A, typename B>
inline std::pair<B,A> reverse(const std::pair<A,B>& p)
{
  return {p.second, p.first};
}

//! add two pairs of things
template <typename A, typename B>
std::pair<A,B> operator+(const std::pair<A,B>& l, const std::pair<A,B>& r)
{
	return {l.first + r.first, l.second + r.second};
}

template <typename A, typename B>
std::ostream& operator<<(std::ostream& os, const std::pair<A,B>& p)
{
	return os << "("<<p.first<<", "<<p.second<<")";
}

template<typename Container>
uint32_t binary_search(const Container& c, const uint32_t target, uint32_t lower_bound, uint32_t upper_bound)
{
  while(lower_bound < upper_bound){
    const uint32_t middle = (lower_bound + upper_bound) / 2;
    if(c[middle] == target) 
      return middle;
    else if(c[middle] < target)
      lower_bound = middle + 1;
    else 
      upper_bound = middle;
  }
  return UINT32_MAX;
}


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

};

