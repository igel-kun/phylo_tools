
#pragma once

#include <vector>
#include <algorithm>
#include "string.h"
#include "utils.hpp"
#include "set_interface.hpp"

namespace PT{

#define NODE_TYPE_LEAF 0x00
#define NODE_TYPE_TREE 0x01
#define NODE_TYPE_RETI 0x02
#define NODE_TYPE_ISOL 0x03

  //! note: this should not be a std::vector because we want to be able to point it into a consecutive block of vertices
  class FixNeighborList{
  public:
    uint32_t* start;
    uint32_t count;

    // convenience functions to not have to write .start[] all the time
    uint32_t& operator[](const uint32_t i) { return start[i]; }
    const uint32_t& operator[](const uint32_t i) const { return start[i]; }

    inline uint32_t get_unique_item() const
    {
      return (count == 1) ? *start : UINT32_MAX;
    }
    inline uint32_t size() const
    {
      return count;
    }
    inline bool empty() const
    {
      return count == 0;
    }
    uint32_t* begin() const
    {
      return start;
    }
    uint32_t* end() const
    {
      return start + count;
    }
    uint32_t& back()
    {
      return *(start + count - 1);
    }
    const uint32_t& back() const
    {
      return *(start + count - 1);
    }
  };

  //! a sorted neighbor list can be searched in O(log n) time, but replace() and delete() might take Omega(n) time
  //NOTE: insert and delete are done via the _extremely_fast_ memmove() function
  class SortedFixNeighborList: public FixNeighborList{
  public:
    uint32_t* find(const uint32_t node) const
    {
      const uint32_t nh_index = binary_search(*this, node);
      return start[nh_index] == node ? start + nh_index : end();
    }
    //! try to replace a node with another node
    // return true on success, otherwise (old_index was not in the list or new_index was already in the list) return false
    //NOTE: this function replaces insert(), since we have to make sure that the data structure does not grow
    bool replace(uint32_t old_node, uint32_t new_node)
    {
      assert(is_sorted());

      const uint32_t old_nh_index = binary_search(*this, old_node);
      if(start[old_nh_index] != old_node) return false;

      const uint32_t new_nh_index = binary_search(*this, new_node);
      if(start[new_nh_index] == new_node) return false;

      // move everything between the new and old index by one uint32_t
      if(old_nh_index < new_nh_index - 1){
        memmove(start + old_nh_index, start + old_nh_index + 1, (new_nh_index - old_nh_index - 1) * sizeof(uint32_t));
      } else if(new_nh_index < old_nh_index){
        memmove(start + new_nh_index + 1, start + new_nh_index, (old_nh_index - new_nh_index - 1) * sizeof(uint32_t));
        // finally write the new node index in its cell
        start[new_nh_index] = new_node;
      } else start[old_nh_index] = new_node; // just replace the old node by the new one
      return true;
    }
    bool is_sorted() const
    {
      for(uint32_t i = 1; i < count; ++i)
        if(start[i] > start[i-1]) throw std::logic_error("sorted neighbor list is not sorted");
    }
    void sort()
    {
      std::sort(start, start + count);
    }
  };

  template<class Container = std::vector<uint32_t>>
  class VariableNeighborList: public Container{
    inline uint32_t get_unique_item() const
    {
      return (this->size() == 1) ? front(*this) : UINT32_MAX;
    }
  };

}
