
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
#include <stack>
#include <map>
#include <set>
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
#define contains(x,y) ((x).find(y) != (x).end())
// find x in the container "list" and, ONLY IF NOT FOUND, execute constructor, assign findings to result (which should be a reference to value_type)
#define FIND_OR_CONSTRUCT(result,x,list,constructor) {auto ___it = list.find(x); if(___it == list.end()) ___it = list.insert(___it, {x, constructor}); result = ___it->second;}
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

// rotation
uint32_t rotl32(uint32_t x, uint32_t n){
  assert (n<32);
  return (x<<n) | (x>>(-n&31));
}
uint32_t rotr32(uint32_t x, uint32_t n){
  assert (n<32);
  return (x>>n) | (x<<(-n&31));
}

// reverse bits
static unsigned char rev_map[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

// Reverse the top and bottom nibble then swap them.
inline uint8_t reverse8(const uint8_t n) { 
  return (rev_map[n & 0xf] << 4) | rev_map[n>>4];
}
inline uint16_t reverse16(const uint16_t n) { 
  return (reverse8(n & 0xff) << 8) | reverse8(n >> 8);
}
inline uint32_t reverse32(const uint32_t n) { 
  return (reverse16(n & 0xffff) << 16) | reverse16(n >> 16);
}
inline uint64_t reverse64(const uint64_t n) {
  const uint64_t n_high = n & 0xffffffff;
  return (reverse32(n_high << 32) | reverse32(n >> 32));
}

size_t hash_combine(size_t x, const size_t y)
{
  x^= y + 0x9e3779b9 + (x << 6) + (x >> 2);
  return x;
}

namespace std{

  // why are those things not defined per default by STL???

  template<class _Container>
  inline typename _Container::const_iterator max_element(const _Container& c) { return max_element(c.begin(), c.end()); }

  template<typename T1, typename T2>
  struct hash<pair<T1, T2> >{
    size_t operator()(const pair<T1,T2>& p) const{
      const std::hash<T1> hasher1;
      const std::hash<T2> hasher2;
      return hash_combine(hasher1(p.first), hasher2(p.second));
    }
  };

  template<typename T, typename Container = std::deque<T>>
  class iterable_stack: public std::stack<T, Container>
  {
    using std::stack<T, Container>::c;
  public:
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    iterator begin() { return c.begin(); }
    iterator end() { return c.end(); }
    const_iterator begin() const { return c.begin(); }
    const_iterator end() const { return c.end(); }
  };


  //! output things
  template<class Iterator>
  inline ostream& output_range(ostream& os, const Iterator& first, const Iterator& last, const char _in, const char _out)
  {
    os << _in;
    for(auto it = first; it != last; ++it) os << *it << " ";
    os << _out;
    return os;
  }

  //! output list of things
  template<typename Element>
  ostream& operator<<(ostream& os, const list<Element>& lst)
  {
    return output_range(os, lst.begin(), lst.end(), '[', ']');
  }

  //! output vector of things
  template<typename Element>
  ostream& operator<<(ostream& os, const vector<Element>& lst)
  {
    return output_range(os, lst.begin(), lst.end(), '[', ']');
  }

  //! output unordered_set of things
  template<typename Element>
  ostream& operator<<(ostream& os, const unordered_set<Element>& lst)
  {
    return output_range(os, lst.begin(), lst.end(), '{', '}');
  }

  //! output set of things
  template<typename Element>
  ostream& operator<<(ostream& os, const set<Element>& lst)
  {
    return output_range(os, lst.begin(), lst.end(), '{', '}');
  }
  //! output set of things
  template<typename Element>
  ostream& operator<<(ostream& os, const iterable_stack<Element>& lst)
  {
    return output_range(os, lst.begin(), lst.end(), '<', '>');
  }

  //! output map of things
  template<typename Element1, typename Element2>
  ostream& operator<<(ostream& os, const unordered_map<Element1, Element2>& _map)
  {
    return output_range(os, _map.begin(), _map.end(), '<', '>');
  }
  //! output map of things
  template<typename Element1, typename Element2>
  ostream& operator<<(ostream& os, const map<Element1, Element2>& _map)
  {
    return output_range(os, _map.begin(), _map.end(), '<', '>');
  }

  template <typename A, typename B>
  ostream& operator<<(ostream& os, const pair<A,B>& p)
  {
    return os << '('<<p.first<<','<<p.second<<')';
  }
  template <typename A>
  ostream& operator<<(ostream& os, const reference_wrapper<A>& r)
  {
    return os << r.get();
  }

  //! a hash computation for a set, XORing its members
  template<typename _Container>
  struct set_hash{
    size_t operator()(const _Container& S) const
    {
      size_t result = 0;
      static const std::hash<typename _Container::value_type> Hasher;
      for(const auto& i : S)
        result ^= Hasher(i);
      return result;
    }
  };

  //! a hash computation for a list, XORing and cyclic shifting its members (such that the order matters)
  template<typename _Container>
  struct list_hash{
    size_t operator()(const _Container& S) const
    {
      size_t result = 0;
      static const std::hash<typename _Container::value_type> Hasher;
      for(const auto& i : S)
        result = rotl(result, 1) ^ Hasher(i);
      return result;
    }
  };

  //! add two pairs of things
  template <typename A, typename B>
  pair<A,B> operator+(const pair<A,B>& l, const pair<A,B>& r)
  {
    return {l.first + r.first, l.second + r.second};
  }

  template <class N>
  struct is_stl_set_type { static const int value = 0; };
  template <class N>
  struct is_stl_map_type { static const int value = 0; };

}


//! testing whether a file exists by trying to open it
#define file_exists(x) (std::ifstream(x).good())

//! reverse a pair of things, that is, turn (x,y) into (y,x)
template<typename A, typename B>
inline std::pair<B,A> reverse(const std::pair<A,B>& p)
{
  return {p.second, p.first};
}


//! find a number in a sorted list of numbers between lower_bound and upper_bound
//if target is not in c, then return the index of the next larger item in c (or upper_bound if there is no larger item)
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
  assert(target <= c[lower_bound]);
  return lower_bound;
}
//! one-bound version of binary search: if only one bound is given, it is interpreted as lower bound
template<typename Container>
uint32_t binary_search(const Container& c, const uint32_t target, uint32_t lower_bound = 0)
{
  return binary_search(c, target, lower_bound, c.size());
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

//! decrease a value in a map, pointed to by an iterator; return true if the value was decreased and false if the item was removed
template<class Map, long threshold = 1>
inline bool decrease_or_remove(Map& m, const typename Map::iterator& it)
{
  if(it->second == threshold) {
    m.erase(it);
    return false;
  } else {
    --(it->second);
    return true;
  }
}

#define return_map_lookup(x,y,z) {const auto _iter = (x).find(y); return (_iter != (x).end()) ? _iter->second : (z); }
/*
// lookup a key in a map and return the default value if the key was not found
template<class Map>
typename Map::mapped_type& map_lookup(Map& _map, const typename Map::key_type& key, typename Map::mapped_type& default_value)
{
  const auto _iter = _map.find(key);
  return (_iter == _map.end()) ? default_value : _iter->second;
}

template<class Map>
const typename Map::mapped_type& map_lookup_const(const Map& _map, const typename Map::key_type& key, const typename Map::mapped_type& default_value)
{
  const auto _iter = _map.find(key);
  return (_iter == _map.end()) ? default_value : _iter->second;
}
*/


