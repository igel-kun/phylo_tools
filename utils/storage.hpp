
#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include "string.h"
#include "utils.hpp"
#include "edge.hpp"

namespace PT{

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

    ConsecutiveStorageNoMem(_Item* _start, const size_t _count = 1):
      start(_start),
      count(_start == nullptr ? 0 : _count)
    {
      std::cout << "constructing new storage in pre-allocated memory at "<<start<<" (space for "<<_count<<" items)\n";
    }

    template<class __Item>
    ConsecutiveStorageNoMem(const ConsecutiveStorageNoMem<__Item>& storage):
      start((_Item*)(storage.cbegin())),
      count(storage.size())
    {}

    // convenience functions to not have to write .start[] all the time
    inline _Item& operator[](const size_t i) { return start[i]; }
    inline const _Item& operator[](const size_t i) const { return start[i]; }

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
    inline const_iterator cbegin() const { return start; }
    inline const_iterator cend() const { return start + count; }
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
    using Parent::start;
  public:

    ConsecutiveStorage(const uint32_t _count = 1):
      Parent::ConsecutiveStorageNoMem((_Item*)malloc(_count * sizeof(_Item)), _count)
    {
      DEBUG5(std::cout << "creating ConsecutiveStorage at "<<start<<" for "<<_count<<" items of size "<<sizeof(_Item)<<std::endl);
    }

    ~ConsecutiveStorage() { free(start); }
  };

  template<typename _Item>  
  std::ostream& operator<<(std::ostream& os, const ConsecutiveStorageNoMem<_Item>& storage)
  {
    os << '{';
    for(auto it = storage.begin(); it != storage.end(); ++it) { std::cout << "item at "<<it<<":\n"<<*it<<'\n'; }
    //for(const auto& i: storage) os << i << ' ';
    return os << '}';
  }

}

namespace std{
  template<class _Item> struct is_stl_set_type<PT::ConsecutiveStorage<_Item>> { static constexpr bool value = true; };
  template<class _Item> struct is_stl_set_type<PT::ConsecutiveStorageNoMem<_Item>> { static constexpr bool value = true; };
}
