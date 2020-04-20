
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
    using pointer     = self_deref<reference>;
    using difference_type = ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;


    raw_vector_map_iterator(_Element* const & _start, size_t _index = 0): 
      start(_start),
      index(_index)
    {}

    reference operator*() const { return {(_Key)index, *(start + index)}; }
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
    using Parent = vector<_Element>;
  public:
    using key_type = const _Key;
    using mapped_type = _Element;
    using iterator = raw_vector_map_iterator<_Key, _Element>;
    using const_iterator = raw_vector_map_iterator<const _Key, const _Element>;
    using value_type = pair<key_type, _Element>;
    using insert_result = pair<iterator, bool>;

    using Parent::data;
    using Parent::size;
  protected:
    friend iterator;

  public:

    // ATTENTION: ERASE DOES NOT NECCESSARILY DO WHAT YOU EXPECT!
    // erase will just reinisialize x to the default element
    // if this is not what you want, you probably want to use vector_map from vector_map2.hpp
    void erase(const key_type x) { data()[x] = _Element(); }
    void erase(const iterator& it) { it->second = _Element(); } 

    template<class T>
    void insert(std::iterator_of_t<T> _start, const std::iterator_of_t<T>& _end)
    {
      while(_start != _end){
        if(_start->first >= size()) {
          Parent::reserve(_start->first + 1);
          Parent::resize(_start->first);
          Parent::emplace_back(_start->second);
        } else operator[](_start->first) = _start->second;
        ++_start;
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
	  insert_result insert(const pair<key_type, _Element>& x) { return try_emplace(x.first, x.second); }

    mapped_type& operator[](const key_type& key) { return Parent::operator[]((size_t)key); }
    const mapped_type& operator[](const key_type& key) const { return Parent::operator[]((size_t)key); }
    mapped_type& at(const key_type& key) { return Parent::at((size_t)key); }
    const mapped_type& at(const key_type& key) const { return Parent::at((size_t)key); }

    iterator begin() { return {data()}; }
    const_iterator begin() const { return {data()}; }
    const_iterator cbegin() const { return {data()}; }
    iterator end() { return {data(), size()}; }
    const_iterator end() const { return {data(), size()}; }
    const_iterator cend() const { return {data(), size()}; }

    iterator find(const _Key& x) { if((size_t)x < size()) return {data() + (size_t)x}; else return end(); }
    const_iterator find(const _Key& x) const { if((size_t)x < size()) return {data() + (size_t)x}; else return end(); }
  };


}
