
// STL does not consider a vector a map, although it does behave similarly

#pragma once

#include "utils.hpp"
#include "stl_utils.hpp"

namespace std {


  template<class _Key, class _Element>
  class raw_vector_map;

  template<class _Key, class _Element>
  class raw_vector_map_iterator
  {
    _Element* start;
    size_t index = 0;
  public:
    using value_type  = pair<_Key, _Element>;
    using reference   = pair<_Key, _Element&>;
    using const_reference = pair<_Key, const _Element&>;
    using pointer       = self_deref<reference>;
    using const_pointer = self_deref<const_reference>;
    using difference_type = ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;


    raw_vector_map_iterator(_Element* const & _start, size_t _index = 0): 
      start(_start),
      index(_index)
    {}

    reference operator*() const { return {static_cast<_Key>(index), *(start + index)}; }
    pointer operator->() const { return operator*(); }

    //! increment & decrement operators
    raw_vector_map_iterator& operator++() { ++index; return *this; }
    raw_vector_map_iterator operator++(int) { raw_vector_map_iterator tmp(*this); ++(*this); return tmp; }
    raw_vector_map_iterator& operator--() { --index; return *this; }
    raw_vector_map_iterator operator--(int) { raw_vector_map_iterator tmp(*this); --(*this); return tmp; }

    raw_vector_map_iterator& operator+=(const size_t x) { index += x; return *this; }
    raw_vector_map_iterator& operator-=(const size_t x) { index -= x; return *this; }
    raw_vector_map_iterator  operator+(const size_t x) { return {start, index + x}; }
    raw_vector_map_iterator  operator-(const size_t x) { return {start, index - x}; }

    difference_type operator-(const raw_vector_map_iterator& it) { return (start + index) - (it.start + it.index); }

    template<class __Key, class __Element>
    bool operator==(const raw_vector_map_iterator<__Key, __Element>& x) const { return index == x.index; }
    template<class __Key, class __Element>
    bool operator!=(const raw_vector_map_iterator<__Key, __Element>& x) const { return index != x.index; }
    template<class __Key, class __Element>
    friend class raw_vector_map_iterator;
  };


  // NOTE: to forbid implicit casting of raw_vector_map to vector, we use this intermediate class which forbids copy construction from raw_vector_map
  // (see https://stackoverflow.com/questions/36473354/prevent-derived-class-from-casting-to-base)
  template<class T>
  class _vector: public vector<T>
  {
  public:
    using vector<T>::vector;

    template<class _Key, class _Element>
    _vector(const raw_vector_map<_Key, _Element>&) = delete;
  };


  template<class _Key, class _Element>
  class raw_vector_map: public _vector<_Element>
  {
    using Parent = _vector<_Element>;
  public:
    using key_type = const _Key;
    using mapped_type = _Element;
    using value_type = pair<key_type, _Element>;

    using iterator = raw_vector_map_iterator<_Key, _Element>;
    using const_iterator = raw_vector_map_iterator<const _Key, const _Element>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using reverse_const_iterator = std::reverse_iterator<const_iterator>;

    using insert_result = pair<iterator, bool>;

    using Parent::data;
    using Parent::size;
  protected:
    friend iterator;

  public:
    // inherit some, but not all constructors
    raw_vector_map(): Parent() {}
    raw_vector_map(const raw_vector_map<_Key, _Element>& x): Parent((const Parent&)x) {}
    raw_vector_map(const raw_vector_map<_Key, _Element>&& x): Parent(move((Parent&&)x)) {}

    template<class InputIt>
    raw_vector_map(InputIt first, const InputIt& last): Parent()
    {
      DEBUG4(std::cout << "constructing raw vector map from range...\n");
      if(is_same_v<typename iterator_traits<InputIt>::iterator_category, random_access_iterator_tag>)
        Parent::reserve(distance(first, last));
      insert(first, last);
    }

    // ATTENTION: ERASE DOES NOT NECCESSARILY DO WHAT YOU EXPECT!
    // erase will just reinisialize x to the default element
    // if this is not what you want, you probably want to use vector_map from vector_map2.hpp
    void erase(const key_type x) { data()[x] = _Element(); }
    void erase(const iterator& it) { it->second = _Element(); } 

    template<class InputIt>
    void insert(InputIt first, const InputIt& last)
    {
      while(first != last){
        if(first->first >= size()) {
          Parent::reserve(first->first + 1);
          Parent::resize(first->first);
          Parent::emplace_back(first->second);
        } else operator[](first->first) = first->second;
        ++first;
      }
    }
    // ATTENTION: THIS EMPLACE DOES NOT ALWAYS DO WHAT YOU WOULD EXPECT!
    // in particular, if you first emplace(10, ...), then emplace(8, ...) will refuse to emplace since it assumes that all below 10 are already emplaced
    // if this is not what you want, use the vector_map<> in vector_map2.hpp instead!
    template<class ...Args>
	  insert_result try_emplace(const key_type x, Args&&... args)
    {
      const size_t x_idx = (size_t)x;
      if(x_idx >= size()) {
        Parent::reserve(x_idx + 1);
        Parent::resize(x_idx);
        Parent::emplace_back(forward<Args>(args)...);
        return { {data(), x_idx}, true };
      } else return { {data(), x_idx}, false };
    }
    template<class ...Args>
	  insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, forward<Args>(args)...); }
    // emplace_hint, ignoring the hint
    template<class Iter, class ...Args>
	  iterator emplace_hint(const Iter&, const key_type x, Args&&... args) { return try_emplace(x, forward<Args>(args)...).first; }
	  
    insert_result insert(const pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }

    mapped_type& operator[](const key_type& key) { return Parent::operator[]((size_t)key); }
    const mapped_type& operator[](const key_type& key) const { return Parent::operator[]((size_t)key); }
    mapped_type& at(const key_type& key) { return Parent::at((size_t)key); }
    const mapped_type& at(const key_type& key) const { return Parent::at((size_t)key); }

    iterator begin() { return {data()}; }
    iterator end() { return {data(), size()}; }
    const_iterator begin() const { return {data()}; }
    const_iterator end() const { return {data(), size()}; }
    reverse_iterator rbegin() { return make_reverse_iterator(end()); }
    reverse_iterator rend() { return make_reverse_iterator(begin()); }
    reverse_const_iterator rbegin() const { return make_reverse_iterator(end()); }
    reverse_const_iterator rend() const { return make_reverse_iterator(begin()); }

    iterator find(const key_type x) { if(contains(x)) return {data(), x}; else return end(); }
    const_iterator find(const key_type x) const { if(contains(x)) return {data(), x}; else return end(); }
    
    bool contains(const key_type x) const { return (size_t)x < size(); }
    bool count(const key_type x) const { return contains(x); }
  };


}
