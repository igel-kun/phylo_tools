
// this implements a hash-set that can only be initialized and queried (no insert, no erase, but very efficient)
// it will be a vector with a good constant hash function
//
// complexity:
// construct: O(n)
// query: O(1)


#pragma once

namespace std{

  template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = std::allocator<Key>>
  class const_unordered_set: public vector<Key>
  {
  };
}// namespace
