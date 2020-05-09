
#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include "string.h"
#include "utils.hpp"
#include "edge.hpp"

namespace PT{

  struct consecutive_tag_t {};
  struct non_consecutive_tag_t {};
  constexpr consecutive_tag_t consecutive_tag;
  constexpr non_consecutive_tag_t non_consecutive_tag;

  //note: this should not be a std::vector because we want to be able to point it into a consecutive block of items
  template<typename _Item>
  class ConsecutiveStorageNoMem
  {
  protected:
    _Item* start;
    size_t count;

  public:
    using value_type = _Item;
    using reference = _Item&;
    using const_reference = const _Item&;
    using iterator = _Item*;
    using const_iterator = const _Item*;


    ConsecutiveStorageNoMem(_Item* const _start, const size_t _count = 0):
      start(_start),
      count(_start == nullptr ? 0 : _count)
    {
      std::cout << "constructing new storage in pre-allocated memory at "<<start<<" (space for "<<_count<<" items)\n";
    }

    ConsecutiveStorageNoMem(): ConsecutiveStorageNoMem(nullptr) {}

    template<class __Item>
    ConsecutiveStorageNoMem(const ConsecutiveStorageNoMem<__Item>& storage):
      start((_Item*)(storage.cbegin())),
      count(storage.size())
    {}

    // copy/move items from another container into us
    template<class Iterator>
    ConsecutiveStorageNoMem(_Item* _start, Iterator first, const Iterator& last):
      start(_start), count(0)
    {
      if(first != last)
        do{
          new(_start++) _Item(std::move(*first));
        } while(++first != last);  // this looks a bit weird because I want to avoid (possibly) expensive post-increments of Iterator
    }

    // convenience functions to not have to write .start[] all the time
    inline reference operator[](const size_t i) { return start[i]; }
    inline const_reference operator[](const size_t i) const { return start[i]; }

    inline size_t size() const { return count; }
    inline void clear() { start = nullptr; count = 0; }
    inline bool empty() const { return count == 0; }
    inline iterator begin() { return start; }
    inline iterator end() { return start + count; }
    inline const_iterator begin() const { return start; }
    inline const_iterator end() const { return start + count; }
    inline iterator rbegin() { return end() - 1; }
    inline iterator rend() { return begin() - 1; }
    inline const_iterator rbegin() const { return end() - 1; }
    inline const_iterator rend() const { return begin() - 1; }
    inline reference       back()       { return *rbegin(); }
    inline const_reference back() const { return *rbegin(); }
    inline reference       front()       { return *begin(); }
    inline const_reference front() const { return *begin(); }
    inline iterator emplace_back(const _Item& e) { return new(start + count++) _Item(e); }
    inline void pop_back() { --count; }

    //! O(n) search
    iterator find(const _Item& x) const
    {
      for(iterator i = begin(); i != end(); ++i)
        if(*i == x) return i;
      return end();
    }


    //! O(1) remove by swapping with the last item
    inline void erase(const iterator& i)
    {
      assert(begin() <= i);
      assert(i < end());
      
      *i = std::move(back());
      pop_back();
    }
  };

  // ConsecutiveStorage with memory management; other ConsecutiveStorage's can point into this one
  template<class _Item>
  class ConsecutiveStorage: public ConsecutiveStorageNoMem<_Item>
  {
    using Parent = ConsecutiveStorageNoMem<_Item>;
  protected:
    using Parent::start;
    using Parent::count;
  public:

    ConsecutiveStorage(const size_t _count = 0):
      Parent::ConsecutiveStorageNoMem(_count == 0 ? nullptr : reinterpret_cast<_Item*>(std::malloc(_count * sizeof(_Item))), _count)
    {
      if(_count && !start) throw std::bad_alloc();
      DEBUG5(std::cout << "creating ConsecutiveStorage at "<<start<<" for "<<_count<<" items of size "<<sizeof(_Item)<<std::endl);
    }

    void resize(const size_t _count)
    {
      if(_count){
        if(start){
          if((start = reinterpret_cast<_Item*>(std::realloc(start, _count))) == nullptr) throw std::bad_alloc();
        } else
          if((start = reinterpret_cast<_Item*>(std::malloc(_count * sizeof(_Item)))) == nullptr) throw std::bad_alloc();
      } else {
        std::free(start);
        start = nullptr;
      }
      count = _count;
    }
    void extend(const size_t _count) { resize(count + _count); }

    ~ConsecutiveStorage() { std::free(start); }
  };

  template<typename _Item>  
  std::ostream& operator<<(std::ostream& os, const ConsecutiveStorageNoMem<_Item>& storage)
  {
    os << '{';
    //for(auto it = storage.begin(); it != storage.end(); ++it) { std::cout << "item at "<<it<<":\n"<<*it<<'\n'; }
    for(const auto& i: storage) os << i << ' ';
    return os << '}';
  }

}

