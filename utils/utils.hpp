
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

#include "debug_utils.hpp"
#include "stl_utils.hpp"

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

#define PWC std::piecewise_construct
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


#if __cplusplus <= 201703L
	//! a more readable containment check
	#define contains(x,y) ((x).find(y) != (x).end())
#endif

// find x in the container "list" and, ONLY IF NOT FOUND, execute constructor, assign findings to result (which should be a reference to value_type)
#define FIND_OR_CONSTRUCT(result,x,list,constructor) {auto ___it = list.find(x); if(___it == list.end()) ___it = list.insert(___it, {x, constructor}); result = ___it->second;}
// get a pointer from a vector iterator
#define vector_iterator_to_pointer(vec, it) ((v).empty() ? nullptr : &((v)[0]) + ((it) - (v).begin()))

//! testing whether a file exists by trying to open it
#define file_exists(x) (std::ifstream(x).good())
// a map lookup that returns a value z on unsuccessful lookups
#define return_map_lookup(x,y,z) {const auto _iter = (x).find(y); return (_iter != (x).end()) ? _iter->second : (z); }

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


// thx @ https://stackoverflow.com
uint32_t integer_log(uint32_t v)
{
  static const int MultiplyDeBruijnBitPosition[32] =
  {
      0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
      8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
  };
  v |= v >> 1; // first round down to one less than a power of 2
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return MultiplyDeBruijnBitPosition[(uint32_t)(v * 0x07C4ACDDU) >> 27];
}

// move items from overlapping ranges
template<class _Item>
inline void move_items(const _Item* dest, const _Item* source, const uint32_t num_items) { memmove(dest, source, num_items * sizeof(_Item)); }

template<class _Item>
inline void move_item(const typename std::vector<_Item>::iterator& dest,
                       const typename std::vector<_Item>::iterator& source,
                       const uint32_t num_items)
{
  if(std::distance(source, dest) > 0)
    std::move(source, std::next(source, num_items), dest);
  else
    std::move_backward(source, std::next(source, num_items), std::next(dest, num_items));
}



