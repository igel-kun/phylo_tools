
#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include "string.h"
#include "utils.hpp"
#include "edge.hpp"

namespace PT{

  // move items from overlapping ranges
  template<class _Item>
  void move_items(const _Item* dest, const _Item* source, const uint32_t num_items)
  {
    memmove(dest, source, num_items * sizeof(_Item));
  }

  template<class _Item = uint32_t>
  void move_items(const typename std::vector<_Item>::iterator& dest,
                  const typename std::vector<_Item>::iterator& source,
                  const uint32_t num_items)
  {
    if(std::distance(source, dest) > 0)
      std::move(source, std::next(source, num_items), dest);
    else
      std::move_backward(source, std::next(source, num_items), std::next(dest, num_items));
  }





  //note: this should not be a std::vector because we want to be able to point it into a consecutive block of items
  template<typename _Item>
  class ConsecutiveStorageNoMem
  {
  protected:
    _Item* start;
    uint32_t count;

  public:
    using value_type = _Item;
    using reference = _Item&;
    using const_reference = const _Item&;
    using iterator = _Item*;
    using const_iterator = const _Item*;

    ConsecutiveStorageNoMem():
      ConsecutiveStorageNoMem(nullptr)
    {}

    ConsecutiveStorageNoMem(_Item* _start, const uint32_t _count = 1):
      start(_start),
      count(_start == nullptr ? 0 : _count)
    {}

    template<class __Item>
    ConsecutiveStorageNoMem(const ConsecutiveStorageNoMem<__Item>& storage):
      start((_Item*)(storage.cbegin())),
      count(storage.size())
    {
    }


    // convenience functions to not have to write .start[] all the time
    _Item& operator[](const uint32_t i) { return start[i]; }
    const _Item& operator[](const uint32_t i) const { return start[i]; }

    inline uint32_t size() const { return count; }
    inline void clear() { start = nullptr; count = 0; }
    inline bool empty() const { return count == 0; }
    iterator begin() { return start; }
    iterator end() { return start + count; }
    const_iterator begin() const { return start; }
    const_iterator end() const { return start + count; }
    iterator rbegin() { return end() - 1; }
    iterator rend() { return begin() - 1; }
    const_iterator rbegin() const { return end() - 1; }
    const_iterator rend() const { return begin() - 1; }
    const_iterator cbegin() const { return start; }
    const_iterator cend() const { return start + count; }
    reference       back()       { return *rbegin(); }
    const_reference back() const { return *rbegin(); }
    reference       front()       { return *begin(); }
    const_reference front() const { return *begin(); }

    iterator emplace_back(const _Item& e)
    {
      return new(start + count++) _Item(e);
    }

    //! O(n) search
    iterator find(const _Item& x) const
    {
      for(iterator i = begin(); i != end(); ++i)
        if(*i == x) return i;
      return end();
    }

    inline void pop_back()
    {
      --count;
    }

    //! O(1) remove
    void erase(const iterator& i)
    {
      assert(begin() <= i);
      assert(i < end());
      
      *i = std::move(back());
      pop_back();
    }
  };

  template<class _Item>
  class ConsecutiveStorage: public ConsecutiveStorageNoMem<_Item>
  {
    using Parent = ConsecutiveStorageNoMem<_Item>;
    using Parent::start;
  public:

    ConsecutiveStorage(const uint32_t _count = 1):
      Parent::ConsecutiveStorageNoMem((_Item*)malloc(_count * sizeof(_Item)), _count)
    {
    }

    ~ConsecutiveStorage()
    {
      free(start);
    }
  };


  template<typename _Item>  
  std::ostream& operator<<(std::ostream& os, const ConsecutiveStorageNoMem<_Item>& storage)
  {
    for(const auto& i: storage) os << i << ' ';
    return os;
  }



/*
  //! a sorted neighbor list can be searched in O(log n) time, but replace() and delete() might take Omega(n) time
  //NOTE: insert and delete are done via the _extremely_fast_ memmove() function
  //note: _Item should provide:
  //        operator==(uint32_t) for find(),
  //        operator=(const _Item&)
  //        operator<(const _Item&) for is_sorted() and sort()
  template<class _Items>
  class SortedItems: public _Items
  {
  public:
    using Parent = _Items;
    using typename Parent::iterator;
    using typename Parent::const_iterator;
    using _Item = typename Parent::value_type;

    const_iterator find(const uint32_t node) const
    {
      const uint32_t nh_index = binary_search(*this, node);
      return (*this)[nh_index] == node ? std::next(this->begin(), nh_index) : this->end();
    }
    iterator find(const uint32_t node)
    {
      const uint32_t nh_index = binary_search(*this, node);
      return (*this)[nh_index] == node ? std::next(this->begin(), nh_index) : this->end();
    }

    //! replace the old item at old_iter with the new item
    // return false if the new item is already in the list (in which case the old item persists), and return true otherwise
    //NOTE: this function replaces insert(), since we have to make sure that the data structure does not grow
    bool replace(const iterator& old_iter, const _Item& new_item)
    {
      const iterator new_iter = std::next(this->begin(), binary_search(*this, new_item));
      if(*new_iter == new_item) return false;

      // move everything between the new and old index by one _Item
      if(std::next(old_iter) < new_iter){
        move_items(old_iter, std::next(old_iter), std::distance(old_iter, new_iter) - 1);
      } else if(new_iter < old_iter){
        move_items(std::next(new_iter), new_iter, std::distance(new_iter, old_iter) - 1);
        // finally write the new node index in its cell using placement new
        *new_iter = new_item;
      } else *new_iter = new_item;
      return true;
    }
    
    void remove(const const_iterator& old)
    {
      // swap old to rbegin() and then remove_last
    }

    void remove(const _Item& x)
    {
    }



    bool is_sorted() const
    {
      if(!std::is_sorted(this->begin(), this->end()))
        throw std::logic_error("sorted storage is not sorted");
      return true;
    }
    void sort()
    {
      std::sort(this->begin(), this->end());
    }
  };

  // ----------------- abbreviations ------------------

  template<typename _Item>
  using SortedConsecutiveStorage = SortedItems<ConsecutiveStorage<_Item>>;

  template<typename _Item>
  using SortedNonConsecutiveStorage = SortedItems<NonConsecutiveStorage<_Item>>;
*/
  template<typename _Item>
  using NonConsecutiveStorage = std::unordered_set<_Item>;

}
