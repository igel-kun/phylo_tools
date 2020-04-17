
//  this is a vector_map that can decide if an item is already in the map, thus allowing proper handling of emplace() and insert()

#pragma once

#include "utils.hpp"
#include "predicates.hpp"
#include "raw_vector_map.hpp"
#include "iter_bitset.hpp"
#include "skipping_iter.hpp"

namespace std{

  template<class _Key, class _Element>
  class vector_map;

  template<class _Key, class _Element>
  class vector_map: public raw_vector_map<_Key, _Element>
  {
    using Parent = raw_vector_map<_Key, _Element>;

    // use a bitset to check who is present - NOTE: internally, ordered_bitset uses a raw_vector_map, which forces this separation of header files
    // into raw_vector_map.hpp and vector_map.hpp
    ordered_bitset present;
  public:
    using Parent::Parent;
    using typename Parent::insert_result;
    using typename Parent::key_type;
    using Parent::data;
    using Parent::size;

    using AbsentPredicate = const MapKeyPredicate<const Parent, ContainmentPredicate<const ordered_bitset, false>>;
    using iterator = std::skipping_iterator<Parent, AbsentPredicate>;
    using const_iterator = std::skipping_iterator<const Parent, AbsentPredicate>;

    void erase(const key_type x) { Parent::erase(x); present.erase(x); } 
    void erase(const iterator& it) { present.erase(it->first); Parent::erase(it); } 
    
    iterator make_iterator(const typename Parent::iterator& raw_it, const bool fix_index = true)
      { return iterator(*this, raw_it, fix_index, *this, present); }
    const_iterator make_iterator(const typename Parent::const_iterator& raw_it, const bool fix_index = true) const
      { return const_iterator(*this, raw_it, fix_index, *this, present); }

    template<class ...Args>
	  insert_result try_emplace(const key_type x, Args&&... args)
    {
      const size_t x_idx = (size_t)x;
      if(x_idx >= size()) {
        Parent::reserve(x_idx + 1);
        Parent::resize(x_idx);
        Parent::emplace_back(forward<Args>(args)...);
        present.set(x_idx);
      } else {
        if(!present.test(x_idx)){
          data()[x_idx] = _Element(forward<Args>(args)...);
        } else return { make_iterator({data(), x_idx}, false), false };
      }
      return { make_iterator({data(), x_idx}, false), true };
    }
    // insert and emplace are (almost) synonymous to try_emplace
    template<class ...Args>
    insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, forward<Args>(args)...); }
    insert_result insert(const pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }

    template<class ...Args>
	  iterator emplace_hint(const iterator& it, const key_type x, Args&&... args) { return emplace(x, forward<Args>(args)...).first; }
    template<class ...Args>
	  iterator emplace_hint(const const_iterator& it, const key_type x, Args&&... args) { return emplace(x, forward<Args>(args)...).first; }

    void clear()
    {
      Parent::clear();
      present.clear();
    }

    iterator begin() { return make_iterator(Parent::begin()); }
    const_iterator begin() const { return make_iterator(Parent::begin()); }
    const_iterator cbegin() const { return make_iterator(Parent::begin()); }
    iterator end() { return make_iterator(Parent::end(), false); }
    const_iterator end() const { return make_iterator(Parent::end(), false); }
    const_iterator cend() const { return make_iterator(Parent::end(), false); }

    iterator find(const _Key& x) { if((size_t)x < size()) return make_iterator({data() + (size_t)x}, false); else return end(); }
    const_iterator find(const _Key& x) const { if((size_t)x < size()) return make_iterator({data() + (size_t)x}, false); else return end(); }
  };

}

