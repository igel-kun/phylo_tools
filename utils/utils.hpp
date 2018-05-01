
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

#define NUM_BYTES_IN_INT (sizeof(unsigned int))
#define NUM_BITS_IN_INT (CHAR_BIT * NUM_BYTES_IN_INT)
#define NUM_BYTES_IN_LONG (sizeof(unsigned long))
#define NUM_BITS_IN_LONG (CHAR_BIT * NUM_BYTES_IN_LONG)

#define NUM_LEADING_ZEROS(x) __builtin_clz(x)
#define NUM_LEADING_ZEROSL(x) __builtin_clzl(x)
#define NUM_TRAILING_ZEROS(x) __builtin_ctz(x)
#define NUM_TRAILING_ZEROSL(x) __builtin_ctzl(x)
#define NUM_ONES_IN(x) __builtin_popcount(x)
#define NUM_ONES_INL(x) __builtin_popcountl(x)
#define NUM_ZEROS_IN(x) (NUM_BITS_IN_INT - NUM_ONES_IN(x))
#define NUM_ZEROS_INL(x) (NUM_BITS_IN_LONG - NUM_ONES_INL(x))
#define NUM_ONES_IN_LOWEST_K_BITL(k,x) (NUM_ONES_INL( (x) << (NUM_BITS_IN_LONG - (k)) ))
#define NUM_ZEROS_IN_LOWEST_K_BITL(k,x) ((k) - NUM_ONES_IN_LOWEST_K_BITL(k, x))

// rounded down logarithm base 2
#define INT_LOG(x) (NUM_BITS_IN_LONG - NUM_LEADING_ZEROSL((unsigned long)x))

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


namespace std{
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

template <typename A, typename B>
std::ostream& operator<<(std::ostream& os, const std::pair<A,B>& p)
{
	return os << "("<<p.first<<", "<<p.second<<")";
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

//! add two pairs of things
template <typename A, typename B>
std::pair<A,B> operator+(const std::pair<A,B>& l, const std::pair<A,B>& r)
{
	return {l.first + r.first, l.second + r.second};
}


}


//! testing whether a file exists by trying to open it
#define file_exists(x) (std::ifstream(x).good())

//! reverse a pair of things, that is, turn (x,y) into (y,x)
template<typename A, typename B>
inline std::pair<B,A> reverse(const std::pair<A,B>& p)
{
  return {p.second, p.first};
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

//! merge two sorted vectors of things (the second into the first)
//NOTE: in-place, linear time, but might copy-construct each element 2 times and call operator= once
//NOTE: STL can merge in-place with overlapping input & output only if source & target are consecutive
template<typename Element>
void merge_sorted_vectors(std::vector<Element>& target, const std::vector<Element>& source)
{
  std::vector<Element> tmp;
  tmp.reserve(target.size());
  target.reserve(target.size() + source.size());
  size_t idx1 = 0;
  size_t idx2 = 0;
  size_t idxt = 0;
  bool target_end = target.empty();
  bool source_end = source.empty();
  bool tmp_end = true;
  while(!tmp_end || !source_end){
    if(source_end || (!tmp_end && (tmp[idxt] < source[idx2]))){
      // if source is already eaten up, or its item is larger than tmp's item, feed from tmp
      if(!target_end){
        if(tmp[idxt] < target[idx1]){
          tmp.push_back(target[idx1]);
          target[idx1++] = tmp[idxt++];
        } else ++idx1;
        target_end = (idx1 == target.size());
      } else {
        // source end and target end
        target.push_back(tmp[idxt++]);
        tmp_end = (idxt == tmp.size());
      }
    } else {
      // source's item is smaller than tmp's item, so feed from source
      if(tmp_end){
        if(!target_end){
          if(source[idx2] < target[idx1]){
            tmp.push_back(target[idx1]);
            target[idx1++] = source[idx2++];
            source_end = (idx2 == source.size());
          } else ++idx1;
          target_end = (idx1 == target.size());
        } else {
          // tmp end and target end
          target.push_back(source[idx2++]);
          source_end = (idx2 == source.size());
        }
      }
    }
  }
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

