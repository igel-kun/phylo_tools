
// this implements a set as a sorted vector
//
// complexity:
// construct: O(n log n)
// query: O(log n)
// insert/erase: O(n)

#include <algorithm>
#include <cstring>

#pragma once

namespace std{
  // do binary search on the vector
  template<class Iter, class Comp, class Key>
  inline pair<Iter, bool> my_binary_search(Iter min, Iter past_end, const Comp& _comp, const Key& key)
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

  template<class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
  class sorted_vector: private vector<Key>
  {
    protected:
      using Parent = vector<Key>;
    public:
      using typename Parent::iterator;
      using typename Parent::const_iterator;
      using insert_result = pair<iterator, bool>;
      using const_insert_result = pair<const_iterator, bool>;

      using Parent::erase;
      using Parent::begin;
      using Parent::end;

    protected:
      using Parent::Parent;
      using Parent::size;

      inline void sortme() { std::sort(this->begin(), this->end(), Compare()); }

      // return an iterator to the smallest element that is at least as large as key (or end() if no such element exists)
      // return whether the key was NOT found (true = failure)
      inline iterator _find_this_or_next(const Key& key)
      {
        return lower_bound(this->begin(), this->end(), key, Compare());
      }
      inline const_iterator _find_this_or_next(const Key& key) const
      {
        return lower_bound(this->begin(), this->end(), key, Compare());
      }
      inline insert_result find_this_or_next(const Key& key)
      {
        auto x = _find_this_or_next(key);
        return {x, (x != this->end()) ? (*x != key) : true};
      }
      inline const_insert_result find_this_or_next(const Key& key) const
      {
        auto x = _find_this_or_next(key);
        return {x, (x != this->end()) ? (*x != key) : true};
      }


    public:

      sorted_vector(): Parent() {}
      sorted_vector(const initializer_list<Key>& li):
        Parent(li)
      { sortme(); }
      template<class InputIt>
      sorted_vector(const InputIt& _begin, const InputIt& _end):
        Parent(_begin, _end)
      { sortme(); }

      // return iterator to element and bool indicating whether insertion took place
      insert_result emplace(Key&& key)
      {
        insert_result i = find_this_or_next(key);
        if(i.second) Parent::emplace(i.first, key);
        return i;
      }
      insert_result emplace(const Key& key)
      {
        insert_result i = find_this_or_next(key);
        if(i.second) Parent::emplace(i.first, key);
        return i;
      }

      iterator find(const Key& key)
      {
        auto x = _find_this_or_next(key);
        if(x != this->end())
          return (*x == key) ? x : this->end();
        else return x;
      }

      const_iterator find(const Key& key) const
      {
        auto x = _find_this_or_next(key);
        if(x != this->end())
          return (*x == key) ? x : this->end();
        else return x;
      }

      size_t count(const Key& key) const
      {
        auto x = _find_this_or_next(key);
        return (x != this->end());
      }

      // return iterator to element and bool indicating whether insertion took place
      insert_result insert(const Key& key)
      {
        insert_result i = find_this_or_next(key);
        if(i.second) Parent::emplace(i.first, key);
        return i;
      }

      template<class InputIt>
      void insert(const InputIt& _begin, const InputIt& _end)
      {
        // first, insert the range at the end of the vector
        iterator x = Parent::insert(this->end(), _begin, _end);
        // then, sort the new range
        sort(x, this->end(), Compare());
        // finally, inplace_merge the ranges
        inplace_merge(this->begin(), x, this->end());
      }

      size_t erase(const Key& key)
      {
        insert_result i = find_this_or_next(key);
        if(!i.second){
          this->erase(i.first);
          return 1;
        } else return 0;
      }
  };
}// namespace
