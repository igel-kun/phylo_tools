
#pragma once

#include <list>
#include <unordered_map>

namespace std {

  template<class ForwardMap, class ReverseMap,
    enable_if_t<is_really_arithmetic_v<typename ForwardMap::key_type> &&
                is_really_arithmetic_v<typename ReverseMap::key_type> &&
                is_same_v<typename ForwardMap::key_type, typename ReverseMap::mapped_type> &&
                is_same_v<typename ReverseMap::key_type, typename ForwardMap::mapped_type>>>
  class IntegralBimap
  {
  protected:
    ForwardMap forward;
    ReverseMap reverse;

  public:
    using key_type = ForwardMap::key_type;
    using mapped_type = ForwardMap::mapped_type;
    using iterator = ForwardMap::iterator;
    using const_iterator = ForwardMap::const_iterator;

    const ReverseMap& get_reverse() const { return reverse; }

    insert_result try_emplace(const key_type key, const mapped_type val)
    {
      bool key_res = forward.try_emplace(key, val).first;
#ifndef NDEBUG
      bool val_res =
#endif  
      reverse.try_emplace(val, key).first;
      assert(key_res = val_res);
      return key_res;
    }

    insert_result insert(const pair<key_type, mapped_type>& p)
    { return try_emplace(p.first, p.second); }

    insert_result try_emplace_rev(const mapped_type val, const key_type key)
    { return try_emplace(key, val); }

    key_type at_rev(const mapped_type val) const { return reverse.at(val); }

    bool contains(const key_type key) const { return contains_key(key); }
    bool contains_key(const key_type key) const { return forward.count(key); }
    bool contains_val(const mapped_type val) const { return reverse.count(val); }

    const_iterator begin() const { return forward.begin(); }
    const_iterator end() const { return forward.end(); }

  };




}
