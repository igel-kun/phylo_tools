
//  this is a vector_map that can decide if an item is already in the map, thus allowing proper handling of emplace() and insert()

#pragma once

#include "utils.hpp"
#include "optional.hpp"
#include "raw_vector_map.hpp"
#include "iter_bitset.hpp"
#include "filter.hpp"

namespace mstd{

  template<class Iter>
  using proto_vector_map_iterator = filtered_iterator<Iter, HasValuePredicate>;

  template<class Key, class Iter>
  using vector_map_iterator = transforming_iterator<proto_vector_map_iterator<Iter>, PairFromVectorIter<Key, proto_vector_map_iterator<Iter>>, true>;


  template<class _Key,
           class _Element,
           class Allocator = std::allocator<OptFor<_Element>>>
  class vector_map: public raw_vector_map<_Key, OptFor<_Element>, Allocator>
  {
    using Element = OptFor<_Element>;
    using Parent = raw_vector_map<_Key, Element>;
    using typename Parent::Vector;
    using Parent::data;
  public:
    using Parent::Parent;
    using typename Parent::key_type;
    using typename Parent::VectorIter;
    using typename Parent::VectorConstIter;
    using Parent::size;

    using FilterIter = proto_vector_map_iterator<VectorIter>;
    using FilterConstIter = proto_vector_map_iterator<VectorConstIter>;
    using AutoIter = typename FilterIter::Iterator;
    using AutoConstIter = typename FilterConstIter::Iterator;

    using iterator = vector_map_iterator<_Key, VectorIter>;
    using const_iterator = vector_map_iterator<_Key, VectorConstIter>;
    using insert_result = std::pair<iterator, bool>;

    bool contains(const key_type x) const { return HasValuePredicate::value(data()[x]); }
    bool count(const key_type x) const { return contains(x); }
    
    template<class... Args>
    auto make_iterator(const VectorIter raw_it, Args&&... args)
    { return iterator(FilterIter{std::forward<Args>(args)..., AutoIter{raw_it, Parent::end()}}); }

    template<class... Args>
    auto make_iterator(const VectorConstIter raw_it, Args&&... args) const
    { return const_iterator(FilterConstIter{std::forward<Args>(args)..., AutoConstIter{raw_it, Parent::end()}}); }
   

    template<class... Args>
	  insert_result try_emplace(const key_type key, Args&&... args) {
      if(key >= size()) {
        Parent::reserve(key + 1);
        Parent::resize(key);
        Parent::emplace_back(std::forward<Args>(args)...);
      } else if(!contains(key)){
        auto* const val = data() + key;
        val->~Element();  // destruct default-created element
        new (val) Element(std::forward<Args>(args)...); // construct new Element in place
      } else return { make_iterator(Vector::begin() + key, do_not_fix_index_tag()), false };
      //Containment::set_present(key);
      return { make_iterator(Vector::begin() + key, do_not_fix_index_tag()), true };
    }

    // insert and emplace are (almost) synonymous to try_emplace
    template<class... Args>
    insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, std::forward<Args>(args)...); }
    insert_result insert(const std::pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }

    iterator begin() { return make_iterator(Vector::begin()); }
    const_iterator begin() const { return make_iterator(Vector::begin()); }
    iterator end() { return make_iterator(Vector::end(), do_not_fix_index_tag()); }
    const_iterator end() const { return make_iterator(Vector::end(), do_not_fix_index_tag()); }

    iterator find(const key_type key) { if(contains(key)) return make_iterator(Vector::begin() + key, do_not_fix_index_tag()); else return end(); }
    const_iterator find(const _Key& key) const { if(contains(key)) return make_iterator(Vector::begin() + key, do_not_fix_index_tag()); else return end(); }
  };

}

