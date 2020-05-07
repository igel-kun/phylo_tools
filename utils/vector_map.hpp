
//  this is a vector_map that can decide if an item is already in the map, thus allowing proper handling of emplace() and insert()

#pragma once

#include "utils.hpp"
#include "predicates.hpp"
#include "raw_vector_map.hpp"
#include "iter_bitset.hpp"
#include "filter.hpp"

namespace std{

  template<class _Key, class _Element, class _AbsentPredicate>
  class _vector_map: public raw_vector_map<_Key, _Element>
  {
    using Parent = raw_vector_map<_Key, _Element>;

  public:
    using Parent::Parent;
    using typename Parent::key_type;
    using Parent::data;
    using Parent::size;

    using AbsentPredicate = _AbsentPredicate;
    using iterator = std::filtered_iterator<Parent, AbsentPredicate>;
    using const_iterator = filtered_iterator<const Parent, AbsentPredicate>;
    using insert_result = pair<iterator, bool>;
  protected:
    void raw_resize(const key_type new_size) { Parent::resize(new_size); }

    virtual void set_present(const key_type) = 0;
    virtual void set_absent(const key_type) = 0;
    virtual AbsentPredicate make_predicate() const = 0;
  public:
    virtual bool contains(const key_type) const = 0;
    virtual void resize(const key_type) = 0;
    virtual ~_vector_map() = 0;
   
    void erase(const key_type x) { Parent::erase(x); set_absent(x); } 
    void erase(const iterator& it) { erase(it->first); }
    
    iterator make_iterator(const typename Parent::iterator& raw_it)
    { return iterator(raw_it, Parent::end(), make_predicate()); }
    iterator make_iterator(const do_not_fix_index_tag, const typename Parent::iterator& raw_it)
    { return iterator(do_not_fix_index, raw_it, Parent::end(), make_predicate()); }
    
    const_iterator make_iterator(const typename Parent::const_iterator& raw_it) const
    { return const_iterator(raw_it, Parent::end(), make_predicate()); }
    const_iterator make_iterator(const do_not_fix_index_tag, const typename Parent::const_iterator& raw_it) const
    { return const_iterator(do_not_fix_index, raw_it, Parent::end(), make_predicate()); }

    template<class... Args>
	  insert_result try_emplace(const key_type key, Args&&... args)
    {
      if(key >= size()) {
        Parent::reserve(key + 1);
        resize(key);
        Parent::emplace_back(forward<Args>(args)...);
      } else if(!contains(key)){
        _Element* const val = data() + key;
        val->~_Element();  // destruct invalid element
        new (val) _Element(forward<Args>(args)...); // construct new _Element in place
      } else return { make_iterator(do_not_fix_index, {data(), key}), false };
      set_present(key);
      return { make_iterator(do_not_fix_index, {data(), key}), true };
    }

    // insert and emplace are (almost) synonymous to try_emplace
    template<class... Args>
    insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, forward<Args>(args)...); }
    insert_result insert(const pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }

    iterator begin() { return make_iterator(Parent::begin()); }
    const_iterator begin() const { return make_iterator(Parent::begin()); }
    iterator end() { return make_iterator(do_not_fix_index, Parent::end()); }
    const_iterator end() const { return make_iterator(do_not_fix_index, Parent::end()); }

    iterator find(const key_type key) { if(contains(key)) return make_iterator(do_not_fix_index, {data() + key}); else return end(); }
    const_iterator find(const _Key& key) const { if(contains(key)) return make_iterator(do_not_fix_index, {data() + key}); else return end(); }
  };

  template<class _Key, class _Element, class _AbsentPredicate>
  _vector_map<_Key, _Element, _AbsentPredicate>::~_vector_map() {};

  // if _Element is integral, we can use a given constexpr as "invalid element" instead of adding a bitset mask
  //NOTE: _Element must be constructible from and comparable to InvalidElement
  template<class _Key, class _Element, class InvalidElement = _Element, InvalidElement _invalid_element = -1>
  class integral_vector_map: public _vector_map<_Key, _Element, StaticEqualPredicate<_Element, _invalid_element>>
  {
    using Parent = _vector_map<_Key, _Element, StaticEqualPredicate<_Element, _invalid_element>>;
  public:
    using Parent::Parent;
    using Parent::data;
    using Parent::size;
    using typename Parent::key_type;
    
  protected:
    using typename Parent::AbsentPredicate;

    void set_present(const key_type key) {}
    
    AbsentPredicate make_predicate() const { return AbsentPredicate(); }
    
    void set_absent(const key_type key)
    {
      assert(key < size());
      _Element* const val = data() + key;
      val->~_Element();
      new (val) _Element(_invalid_element);
    }
  public:
    void resize(const key_type new_size) { Parent::resize(new_size, _invalid_element); }

    bool contains(const key_type key) const {
      assert(key < size());
      return *(data() + key) != _invalid_element;
    }
  };

  // for non-integral types, a pointer to a constexpr object can be used as "invalid_element"
  template<class _Key, class _Element, class InvalidElement, InvalidElement* _invalid_element>
  class non_integral_vector_map: public _vector_map<_Key, _Element, StaticEqualPtrPredicate<_Element, _invalid_element>>
  {
    using Parent = _vector_map<_Key, _Element, StaticEqualPtrPredicate<_Element, _invalid_element>>;
  public:
    using Parent::Parent;
    using Parent::data;
    using Parent::size;
    using typename Parent::key_type;
  protected:
    using typename Parent::AbsentPredicate;

    void set_present(const key_type key) {}
    
    AbsentPredicate make_predicate() const { return AbsentPredicate(); }
    
    void set_absent(const key_type key)
    {
      assert(key < size());
      _Element* const val = data() + key;
      val->~_Element();
      new (val) _Element(*_invalid_element);
    }
  public:
    void resize(const key_type new_size) { Parent::resize(new_size, *_invalid_element); }
    bool contains(const key_type key) const {
      assert(key < size());
      return *(data() + key) != *_invalid_element;
    }
  };

  // for types that cannot have an invalid state, we'll have to keep a bitset around, indicating whether or not a key is present
  template<class _Key, class _Element>
  class bitset_vector_map:
    public _vector_map<_Key, _Element, MapKeyPredicate<ContainmentPredicate<const ordered_bitset, false>>>
  {
    using Parent = _vector_map<_Key, _Element, MapKeyPredicate<ContainmentPredicate<const ordered_bitset, false>>>;

    // use a bitset to check who is present - NOTE: internally, ordered_bitset uses a raw_vector_map, which forces this separation of header files
    // into raw_vector_map.hpp and vector_map.hpp
    ordered_bitset present;

  public:
    using Parent::Parent;
    using Parent::data;
    using Parent::size;
    using typename Parent::key_type;
    using typename Parent::AbsentPredicate;

  protected:
    void set_present(const key_type key) { present.set(key); }
    void set_absent(const key_type key) { present.unset(key); }
    AbsentPredicate make_predicate() const { return AbsentPredicate(present); }
  public:
    void resize(const key_type new_size) { Parent::raw_resize(new_size); }

    bool contains(const key_type key) const { cout << "checking containment of "<<key<<" in "<<present<<"\n"; return present.test(key); }

    void clear()
    {
      Parent::clear();
      present.clear();
    }

  };

  // select the correct vector_map, depending on whether an invalid element is given/given as pointer/not given
  template<class _Key, class _Value, class InvalidElement, uintptr_t Invalid>
  struct __vector_map { using type = conditional_t<is_arithmetic_v<InvalidElement>,
        integral_vector_map<_Key, _Value, InvalidElement, static_cast<InvalidElement>(Invalid)>,  // if InvalidElement is integral, use integral_vector_map
        non_integral_vector_map<_Key, _Value, InvalidElement, static_cast<InvalidElement*>(Invalid)>>; }; // otherwise, non_integral_vector_map
  // if InvalidElement is void, use the bitset vector_map
  template<class _Key, class _Value, uintptr_t Invalid>
  struct __vector_map<_Key, _Value, void, Invalid> { using type = bitset_vector_map<_Key, _Value>; };

  // NOTE: if we just have key and value, better play it safe and use the bitset version...
  template<class Key, class Value, class InvalidElement = void, uintptr_t Invalid = -1ul>
  using vector_map = typename __vector_map<Key, Value, InvalidElement, Invalid>::type;
}

