
//  this is a vector_map that can decide if an item is already in the map, thus allowing proper handling of emplace() and insert()

#pragma once

#include "utils.hpp"
#include "optional.hpp"
#include "raw_vector_map.hpp"
#include "iter_bitset.hpp"
#include "filter.hpp"

namespace mstd{
  template<class key_type, bool invert = false>
  class ContainmentTracker {};

  template<class key_type, bool invert> requires (!mstd::Optional<key_type> && std::is_convertible_v<key_type, size_t>)
  class ContainmentTracker<key_type, invert> {
    ordered_bitset _containment_bits;
    void set_present(const key_type& x) { _containment_bits.set(x); }
    void set_absent(const key_type&) {}
  public:
    bool contains(const key_type& x) const { return test(_containment_bits, x) != invert; }
    auto make_predicate() const { return [&](const key_type& x){ return test(_containment_bits, x) != invert;}; }
  };

  template<mstd::Optional key_type, bool invert>
  class ContainmentTracker<key_type, invert> {
    static constexpr void set_present(const key_type&) {}
    static constexpr void set_absent(const key_type&) {}
  public:
    static constexpr bool contains(const key_type& x){ return x.has_value() != invert; }
    static constexpr auto make_predicate() { return mstd::optional_value_predicate(); }
  };

  template<class _Key, class _Element>
  class vector_map: public ContainmentTracker<_Element, false>,
                    public raw_vector_map<_Key, _Element>
  {
    using Parent = raw_vector_map<_Key, _Element>;
    using Containment = ContainmentTracker<_Element, false>;

  public:
    using Parent::Parent;
    using typename Parent::key_type;
    using Parent::data;
    using Parent::size;
    using Containment::contains;
    using Containment::make_predicate;
    using AbsentPredicate = decltype(make_predicate());

    using iterator = filtered_iterator<Parent, AbsentPredicate>;
    using const_iterator = filtered_iterator<const Parent, AbsentPredicate>;
    using insert_result = std::pair<iterator, bool>;
  public:
    bool count(const key_type x) const { return contains(x); }

    void erase(const key_type x) { Parent::erase(x); set_absent(x); } 
    void erase(const iterator& it) { erase(it->first); }
    
    iterator make_iterator(const typename Parent::iterator& raw_it)
    { return iterator(raw_it, Parent::end(), make_predicate()); }
    iterator make_iterator(const do_not_fix_index_tag, const typename Parent::iterator& raw_it)
    { return iterator(do_not_fix_index_tag(), raw_it, Parent::end(), make_predicate()); }
    
    const_iterator make_iterator(const typename Parent::const_iterator& raw_it) const
    { return const_iterator(raw_it, Parent::end(), make_predicate()); }
    const_iterator make_iterator(const do_not_fix_index_tag, const typename Parent::const_iterator& raw_it) const
    { return const_iterator(do_not_fix_index_tag(), raw_it, Parent::end(), make_predicate()); }

    template<class... Args>
	  insert_result try_emplace(const key_type key, Args&&... args)
    {
      if(key >= size()) {
        Parent::reserve(key + 1);
        Parent::resize(key);
        Parent::emplace_back(forward<Args>(args)...);
      } else if(!contains(key)){
        _Element* const val = data() + key;
        val->~_Element();  // destruct default-created element
        new (val) _Element(forward<Args>(args)...); // construct new _Element in place
      } else return { make_iterator(do_not_fix_index_tag(), {data(), key}), false };
      Containment::set_present(key);
      return { make_iterator(do_not_fix_index_tag(), {data(), key}), true };
    }

    // insert and emplace are (almost) synonymous to try_emplace
    template<class... Args>
    insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, forward<Args>(args)...); }
    insert_result insert(const std::pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }

    iterator begin() { return make_iterator(Parent::begin()); }
    const_iterator begin() const { return make_iterator(Parent::begin()); }
    iterator end() { return make_iterator(do_not_fix_index_tag(), Parent::end()); }
    const_iterator end() const { return make_iterator(do_not_fix_index_tag(), Parent::end()); }

    iterator find(const key_type key) { if(contains(key)) return make_iterator(do_not_fix_index_tag(), {data() + key}); else return end(); }
    const_iterator find(const _Key& key) const { if(contains(key)) return make_iterator(do_not_fix_index_tag(), {data() + key}); else return end(); }
  };

}

