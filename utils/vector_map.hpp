
// STL does not consider a vector a map, although it does behave similarly

#pragma once

#include "utils.hpp"

namespace std {

  template<class _Element>
  class vector_mapIterator;

  template<class _Element>
  class vector_map;

  template <class N>
  struct is_stl_map_type<std::vector_map<N>> { static const int value = 1; };

  template<class _Element1, class _Element2>
  bool operator==(const vector_mapIterator<_Element1>& x, const vector_mapIterator<_Element2>& y);
  template<class _Element1, class _Element2>
  bool operator!=(const vector_mapIterator<_Element1>& x, const vector_mapIterator<_Element2>& y);

  template<class _Element>
  class vector_mapIterator: public std::iterator<std::forward_iterator_tag, pair<size_t, _Element&> >
  {
    _Element* start;
    size_t index = 0;
  public:

    template<class _Element1>
    friend class vector_map;


    vector_mapIterator(_Element* const & _start, size_t _index = 0): 
      start(_start),
      index(_index)
    {}

    pair<size_t, _Element&> operator*() const { return {index, *(start + index)}; }
    pair<size_t, _Element*> operator->() const { return {index, *(start + index)}; }

    //! increment operator
    vector_mapIterator& operator++()
    {
      ++index;
      return *this;
    }

    //! post-increment
    vector_mapIterator operator++(int)
    {
      vector_mapIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    template<class _Element1, class _Element2>
    friend
    bool operator!=(const vector_mapIterator<_Element1>& x, const vector_mapIterator<_Element2>& y);

  };

  template<class _Element1, class _Element2>
  bool operator!=(const vector_mapIterator<_Element1>& x, const vector_mapIterator<_Element2>& y)
  {
    return x.index != y.index;
  }
  template<class _Element1, class _Element2>
  bool operator==(const vector_mapIterator<_Element1>& x, const vector_mapIterator<_Element2>& y)
  {
    return !(x != y);
  }



  template<class _Element>
  class vector_map: public vector<_Element>
  {
    using Parent = vector<_Element>;
  public:
    using iterator = vector_mapIterator<_Element>;
    using const_iterator = vector_mapIterator<const _Element>;
    using key_type = const size_t;
    using mapped_type = _Element;
    using value_type = pair<key_type, _Element>;
    using Parent::data;
    using Parent::size;
    using Parent::capacity;
    using Parent::resize;
  protected:
    friend iterator;

  public:

    // erasing a key-value pair doesn't make sense for a vector
    void erase(const key_type x) { data()[x] = 0; }
    
    pair<iterator,bool> emplace(const key_type x, const _Element& y)
    {
      if(x >= size()) {
        resize(x + 1);
        new(data() + x) _Element(y);
        return { {data(), x}, true };
      } else return { {data(), x}, false };

    }

    iterator emplace_hint(const iterator& hint, const key_type x, const _Element& y)
    {
      return emplace(x, y).first;
    }

    pair<iterator, bool> insert(const pair<key_type, _Element>& x)
    {
      if(x.first >= size()){
        resize(x.first + 1);
        data()[x.first] = x.second;
        return { vector_mapIterator<_Element>(data(), x.first), true};
      } else return { vector_mapIterator<_Element>(data(), x.first), false};
    }
    
    void erase(const iterator& it) { (*it).second = 0; }
    iterator begin() { return {data()}; }
    const_iterator begin() const { return {data()}; }
    const_iterator cbegin() const { return {data()}; }
    iterator end() { return {data(), size()}; }
    const_iterator end() const { return {data(), size()}; }
    const_iterator cend() const { return {data(), size()}; }
    iterator find(const uint64_t x) { return {data(), min(x, size())}; }
    const_iterator find(const uint64_t x) const { return {data(), min(x, size())}; }
  };


  // use a vector as list; all we need to do is overwrite constructor(int)
  template<class _Element>
  class vector_list: public vector<_Element>
  {
    public:
      using Parent = vector<_Element>;
      using Parent::vector;

      vector_list(const _Element& x):
        Parent(1, x)
      {}
  };
 

}
