
// unifcation for the set interface
// this is to use unordered_set<uint32_t> with the same interface as iterable_bitset

#pragma once

namespace std {
  // since it was the job of STL to provide for it and they failed, I'll pollute their namespace instead :)

  inline bool test(const std::iterable_bitset& _set, const uint32_t index)
  {
    return _set.test(index);
  }
  inline bool test(const std::unordered_set<uint32_t>& _set, const uint32_t index)
  {
    return contains(_set, index);
  }
  inline void intersect(std::iterable_bitset& target, const std::iterable_bitset& source)
  {
    target &= source;
  }
  inline void intersect(std::unordered_set<uint32_t>& target, const std::unordered_set<uint32_t>& source)
  {
    for(auto target_it = target.begin(); target_it != target.end();)
      if(!contains(source, *target_it))
        target_it = target.erase(target_it); 
      else ++target_it;
  }
  inline uint32_t front(const std::iterable_bitset& _set)
  {
    return _set.front();
  }
  inline uint32_t front(const std::unordered_set<uint32_t>& _set)
  {
    return *_set.begin();
  }

}
