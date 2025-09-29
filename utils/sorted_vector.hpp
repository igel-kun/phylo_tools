
// this implements a set as a sorted vector
//
// complexity:
// construct: O(n log n)
// query: O(log n)
// insert/erase: O(n)

#pragma once

#include <algorithm>
#include <cstring>

#include "stl_utils.hpp"
#include "predicates.hpp"


namespace mstd{
  // do binary search on the vector
  template<class Iter, class Comp, class Key>
  std::pair<Iter, bool> my_binary_search(Iter min, Iter past_end, const Comp& _comp, const Key& key)
  {
    // the sought value is always in the range [min, past_end - 1] until min == past_end
    while(min != past_end){
      const Iter i = next(min, distance(min, past_end) / 2);
      if(!_comp(*i, key)){
        if(_comp(key, *i)){
          past_end = i;
        } else return {past_end, false};
      } else ++(min = i);
    }
    return {past_end, true};
  }

  // a flat set, if you pass std::less, then the smallest value will be first
  // NOTE: by default, KeysEqual is FalsePredicate, meaning that storage of duplicates is allowed (so it's a flat multimap)
  template<class Key_, class Compare_ = std::less<Key_>, class KeysEqual_ = pred::FalsePredicate, class Allocator = std::allocator<Key_>>
    requires (std::invocable<Compare_, Key_, Key_> && (std::is_void_v<KeysEqual_> || std::invocable<KeysEqual_, Key_, Key_>))
  class sorted_vector:
    public std::vector<Key_>
  {
  public:
    using Parent = std::vector<Key_>;
    using Key = Key_;
    using Compare = Compare_;
    using KeysEqual = KeysEqual_;

    using typename Parent::iterator;
    using typename Parent::const_iterator;
    using insert_result = std::pair<iterator, bool>;
    using const_insert_result = std::pair<const_iterator, bool>;


    using Parent::erase;
    using Parent::begin;
    using Parent::end;
    using Parent::size;

  protected:
    using Parent::Parent;

    [[no_unique_address]] Compare cmp;
    [[no_unique_address]] KeysEqual keys_equal;

    void sortme() { std::sort(this->begin(), this->end(), cmp); }

    // return an iterator to the smallest element that is at least as large as key (or end() if no such element exists)
    // return whether the key was NOT found (true = failure)
    iterator _find_this_or_next(const Key& key) {
      return lower_bound(this->begin(), this->end(), key, cmp);
    }
    const_iterator _find_this_or_next(const Key& key) const {
      return lower_bound(this->begin(), this->end(), key, cmp);
    }
    insert_result find_this_or_next(const Key& key) {
      auto x = _find_this_or_next(key);
      return {x, (x != this->end()) ? not keys_equal(x, key) : true};
    }
    const_insert_result find_this_or_next(const Key& key) const {
      auto x = _find_this_or_next(key);
      return {x, (x != this->end()) ? not keys_equal(x, key) : true};
    }

  public:

    sorted_vector() = default;

    sorted_vector(const std::initializer_list<Key>& li):
      Parent(li)
    { sortme(); }
    
    template<mstd::HasIterCategory InputIt, class... Args>
    sorted_vector(const InputIt& _begin, const InputIt& _end, Args&&... args):
      Parent(_begin, _end),
      cmp(std::forward<Args>(args)...)
    { sortme(); }

    template<class... Args>
    sorted_vector(Compare _cmp, Args&&... args):
      Parent(std::forward<Args>(args)...),
      cmp(std::move(_cmp))
    {}

    const Compare& key_comp() const { return cmp; }

    iterator find(const Key& key) {
      auto x = _find_this_or_next(key);
      if(x != this->end())
        return (*x == key) ? x : this->end();
      else return x;
    }

    const_iterator find(const Key& key) const {
      auto x = _find_this_or_next(key);
      if(x != this->end())
        return (*x == key) ? x : this->end();
      else return x;
    }

    size_t count(const Key& key) const {
      const auto x = _find_this_or_next(key);
      return (x != this->end());
    }

    // return iterator to element and bool indicating whether insertion took place
    template<class K> requires mstd::is_constructible_v<Key, K>
    auto emplace(K&& key) {
      insert_result i = find_this_or_next(key);
      if(i.second) Parent::emplace(i.first, std::forward<K>(key));
      return i;
    }
    template<class First, class... Args> requires (not mstd::is_same_v<First, Key>)
    auto emplace(First&& first, Args&&... args) { return emplace(Key(std::forward<First>(first), std::forward<Args>(args)...)); }
  
    template<class K> requires mstd::is_same_v<K, Key>
    auto insert(K&& key) { return emplace(std::forward<K>(key)); }

    template<class InputIt>
    void insert(const InputIt& _begin, const InputIt& _end) {
      // first, insert the range at the end of the vector
      const iterator x = Parent::insert(this->end(), _begin, _end);
      // then, sort the new range
      sort(x, this->end(), cmp);
      // finally, inplace_merge the ranges
      std::inplace_merge(this->begin(), x, this->end());
    }
  
    template<class InputIt>
    void insert_sorted(const InputIt& _begin, const InputIt& _end) {
      // first, insert the range at the end of the vector
      const iterator x = Parent::insert(this->end(), _begin, _end);
      assert(std::is_sorted(x, this->end(), cmp));
      // finally, inplace_merge the ranges
      std::inplace_merge(this->begin(), x, this->end());
    }
    
    void insert(const sorted_vector& other) {
      // first, insert the range at the end of the vector
      const iterator x = Parent::insert(this->end(), other.begin(), other.end());
      // then, inplace_merge the ranges
      std::inplace_merge(this->begin(), x, this->end());
    }


    bool erase(const Key& key) {
      const insert_result i = find_this_or_next(key);
      if(!i.second){
        this->erase(i.first);
        return 1;
      } else return 0;
    }
  };
}// namespace
