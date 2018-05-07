
#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include "string.h"
#include "utils.hpp"
#include "edge.hpp"

namespace PT{

  //note: this should not be a std::vector because we want to be able to point it into a consecutive block of Edges
  template<typename _Edge = Edge>
  class ConsecutiveEdges{
  public:
    typedef _Edge value_type;
    typedef _Edge* iterator;
    typedef const _Edge* const_iterator;

    _Edge* start;
    uint32_t count = 0;

    // convenience functions to not have to write .start[] all the time
    _Edge& operator[](const uint32_t i) { return start[i]; }
    const _Edge& operator[](const uint32_t i) const { return start[i]; }

    inline uint32_t size() const
    {
      return count;
    }
    inline bool empty() const
    {
      return count == 0;
    }
    iterator begin() const
    {
      return start;
    }
    iterator end() const
    {
      return start + count;
    }
    _Edge& back()
    {
      return *(start + count - 1);
    }
    const _Edge& back() const
    {
      return *(start + count - 1);
    }
    iterator emplace_back(const _Edge& e)
    {
      iterator space = start + count++;
      new(space) _Edge(e);
      return space;
    }
  };

  template<typename _Edge = Edge>
  using NonConsecutiveEdges = std::vector<std::reference_wrapper<_Edge>>;



  // move items from overlapping ranges
  template<class _Edge = Edge>
  void move_items(const _Edge* dest, const _Edge* source, const uint32_t num_items)
  {
    memmove(dest, source, num_items * sizeof(_Edge));
  }
  template<class _Edge = uint32_t>
  void move_items(const typename std::vector<_Edge>::iterator& dest,
                  const typename std::vector<_Edge>::iterator& source,
                  const uint32_t num_items)
  {
    if(std::distance(source, dest) > 0)
      std::move(source, std::next(source, num_items), dest);
    else
      std::move_backward(source, std::next(source, num_items), std::next(dest, num_items));
  }

  //! a sorted neighbor list can be searched in O(log n) time, but replace() and delete() might take Omega(n) time
  //NOTE: insert and delete are done via the _extremely_fast_ memmove() function
  //note: _Edge should provide:
  //        operator==(uint32_t) for find(),
  //        operator=(const _Edge&)
  //        operator<(const _Edge&) for is_sorted() and sort()
  template<class _Edges>
  class SortedEdgesT: public _Edges
  {
  public:
    using typename _Edges::iterator;
    typedef typename _Edges::value_type _Edge;

    iterator find(const uint32_t node) const
    {
      const uint32_t nh_index = binary_search(*this, node);
      return (*this)[nh_index] == node ? std::next(this->begin(), nh_index) : this->end();
    }
    //! try to replace a node with another node
    // return true on outess, otherwise (new_edge was already in the list) return false
    //NOTE: this function replaces insert(), since we have to make sure that the data structure does not grow
    bool replace(const typename _Edges::iterator& old_it, const _Edge& new_edge)
    {
      assert(is_sorted());

      const iterator new_it = std::next(this->begin(), binary_search(*this, new_edge));
      if(*new_it == new_edge) return false;

      // move everything between the new and old index by one _Edge
      if(std::next(old_it) < new_it){
        move_items(old_it, std::next(old_it), std::distance(old_it, new_it) - 1);
      } else if(new_it < old_it){
        move_items(std::next(new_it), new_it, std::distance(new_it, old_it) - 1);
        // finally write the new node index in its cell using placement new
        new_it->~_Edge();
        new(new_it) _Edge(new_edge);
      } else {
        old_it->~_Edge();
        new(old_it) _Edge(new_edge); // just replace the old node by the new one
      }
      return true;
    }
    bool is_sorted() const
    {
      if(!std::is_sorted(this->begin(), this->end()))
        throw std::logic_error("sorted neighbor list is not sorted");
      return true;
    }
    void sort()
    {
      std::sort(this->begin(), this->end());
    }
  };


  template<typename _Edge = Edge>
  using SortedConsecutiveEdges = SortedEdgesT<ConsecutiveEdges<_Edge>>;

  template<typename _Edge = Edge>
  using SortedNonConsecutiveEdges = SortedEdgesT<NonConsecutiveEdges<_Edge>>;


}
