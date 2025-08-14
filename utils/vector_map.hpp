
#pragma once

#include "utils.hpp"
#include "optional.hpp"
#include "raw_vector_map.hpp"
#include "iter_bitset.hpp"
#include "filter.hpp"

namespace mstd{

  // ========== vector_map ==========
  //  a vector that can decide if an item is present of not,
  //  thus allowing proper handling of emplace() and insert().

  // ------- vector_map: helpers ---------
   template<class Iter>
  using proto_vector_map_iterator = filtered_iterator<Iter, HasValuePredicate>;

  template<class Key, class Iter>
  using vector_map_iterator = transforming_iterator<proto_vector_map_iterator<Iter>, PairFromVectorIter<Key, proto_vector_map_iterator<Iter>>, true>;

 
  // ------- vector_map: main class ---------
  template<class _Key,
           class _Element,
           class Allocator = std::allocator<OptFor<_Element>>>
  struct vector_map:
    public raw_vector_map<_Key, OptFor<_Element>, Allocator>
  {
    // ------- static stuff --------
    using Parent = raw_vector_map<_Key, OptFor<_Element>, Allocator>;
    using Parent::Parent;
    using typename Parent::Vector;
    using typename Parent::key_type;
    using typename Parent::mapped_type;
    using typename Parent::VectorIter;
    using typename Parent::VectorConstIter;

    using Element = OptFor<_Element>;

    static_assert(std::is_same_v<Vector, std::vector<OptFor<_Element>, Allocator>>);
    static_assert(std::is_same_v<mapped_type, typename Vector::value_type>);

    using FilterIter = proto_vector_map_iterator<VectorIter>;
    using FilterConstIter = proto_vector_map_iterator<VectorConstIter>;
    using AutoIter = typename FilterIter::Iterator;
    using AutoConstIter = typename FilterConstIter::Iterator;

    using iterator = vector_map_iterator<_Key, VectorIter>;
    using const_iterator = vector_map_iterator<_Key, VectorConstIter>;
    using insert_result = std::pair<iterator, bool>;

    // ------- members --------
  protected:
    size_t _count = 0;

    // ------- construction & desctruction ---------
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: query --------
  protected:
    using Parent::data;

    template<class... Args>
    auto make_iterator(const VectorIter raw_it, Args&&... args)
    { return iterator(FilterIter{std::forward<Args>(args)..., AutoIter{raw_it, Parent::end()}}, data()); }

    template<class... Args>
    auto make_iterator(const VectorConstIter raw_it, Args&&... args) const
    { return const_iterator(FilterConstIter{std::forward<Args>(args)..., AutoConstIter{raw_it, Parent::end()}}, data()); }

  public:
    bool is_valid(const Element& x) const { return HasValuePredicate::value(x); }

    size_t size() const { return count(); }
    bool contains(const key_type x) const { return is_valid(data()[x]); }
    
    size_t count() const { return _count; }
    bool count(const key_type x) const { return contains(x); }

    iterator begin() { return make_iterator(Vector::begin()); }
    const_iterator begin() const { return make_iterator(Vector::begin()); }
    auto end() { return Parent::end(); } //make_iterator(Vector::end(), do_not_fix_index_tag()); }
    auto end() const { return Parent::end(); } //make_iterator(Vector::end(), do_not_fix_index_tag()); }
    auto vm_end() const { make_iterator(Vector::end(), do_not_fix_index_tag()); }                                       

    iterator find(const key_type key) { if(contains(key)) return make_iterator(Vector::begin() + key, do_not_fix_index_tag()); else return vm_end(); }
    const_iterator find(const _Key& key) const { if(contains(key)) return make_iterator(Vector::begin() + key, do_not_fix_index_tag()); else return vm_end(); }

    // ------- methods: modification --------
  protected:
    bool erase(Element& x) {
      if(is_valid(x)) {
        x.reset();
        --_count;
        return true;
      } else return false;
    }

  public:
    bool erase(const key_type key) {
      if(key < Parent::size()) {
        return erase(data()[key]);
      } else return false;
    }
    bool erase(const AutoIter it) { return erase(*it); }


    void clear() {
      Parent::clear();
      _count = 0;
    }

    template<class... Args>
	  insert_result try_emplace(const key_type key, Args&&... args) {
      DEBUG5(std::cout << "inserting "<<key<< " into vector_map of size "<<size()<<" (vector size: "<<Parent::size()<<")\n");
      if(key >= Parent::size()) {
        Parent::reserve(key + 1);
        Parent::resize(key);
        Parent::emplace_back(std::forward<Args>(args)...);
        ++_count;
        DEBUG5(std::cout << "emplaced element " << Parent::back() << " at key "<<key<<", size is now "<<size()<<"\n");
      } else if(!contains(key)){
        DEBUG5(std::cout << key<< " is not in the map, so we'll construct it\n");
        auto* const val = data() + key;
        val->~Element();  // destruct default-created element
        new (val) Element(std::forward<Args>(args)...); // construct new Element in place
        ++_count;
        DEBUG5(std::cout << "constructed element " << *val << " at key "<<key<<"\n");
      } else return { make_iterator(Vector::begin() + key, do_not_fix_index_tag()), false };
      //Containment::set_present(key);
      return { make_iterator(Vector::begin() + key, do_not_fix_index_tag()), true };
    }

    // insert and emplace are (almost) synonymous to try_emplace
    template<class... Args>
    insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, std::forward<Args>(args)...); }
    insert_result insert(const std::pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }
  };

  // ------- vector_map: factories --------- 
  // ------- vector_map: concepts ---------
  // ------- vector_map: deduction guides ---------
  // ------- vector_map: defaults ---------

}

