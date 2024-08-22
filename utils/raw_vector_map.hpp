
// STL does not consider a vector a map, although it does behave similarly

#pragma once

#include "utils.hpp"
#include "stl_utils.hpp"
#include "trans_iter.hpp"


namespace mstd {
  template<std::unsigned_integral _Key, VectorType _base_container>
  class _raw_vector_map;

  /*
  // NOTE: to forbid implicit casting of raw_vector_map to vector, we use this intermediate class which forbids copy construction from raw_vector_map
  // (see https://stackoverflow.com/questions/36473354/prevent-derived-class-from-casting-to-base)
  template<class T>
  struct _vector: public std::vector<T> {
    using std::vector<T>::vector;

    template<std::unsigned_integral _Key, VectorType _base_container>
    _vector(const _raw_vector_map<_Key, _base_container>&) = delete;
  };
*/

  // the transformation takes a vector iterator and outputs the distance to the start of the vector and a reference to the cell
  template<std::unsigned_integral Key, class Iter>
  struct PairFromVectorIter { 
    using IterRef = reference_of_t<Iter>;
    static_assert(std::is_reference_v<IterRef>);

    using iter_ptr = pointer_from_reference<IterRef>;
    using ret_val = std::pair<Key, IterRef>;

    iter_ptr start;

    ret_val operator()(const auto& it) const { return {&(*it) - start, *it}; }
  };

  template<class _Key, class _Iter>
  using raw_vector_map_iterator = transforming_iterator<_Iter, PairFromVectorIter<_Key, _Iter>, true>;

  template<std::unsigned_integral _Key, class _Element>
  using raw_vector_map = _raw_vector_map<_Key, std::vector<_Element>>;

  template<std::unsigned_integral _Key, VectorType _base_container> 
  class _raw_vector_map: public _base_container
  {
  protected:
    using Vector = _base_container;
  public:

    using Traits = iterator_traits<Vector>;
    using VectorIter = iterator_of_t<Vector>;
    using VectorConstIter = const_iterator_of_t<Vector>;
    using iterator = raw_vector_map_iterator<_Key, VectorIter>;
    using const_iterator = raw_vector_map_iterator<_Key, VectorConstIter>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using reverse_const_iterator = std::reverse_iterator<const_iterator>;

    using key_type = const _Key;
    using mapped_type = typename Traits::value_type;
    using value_type = std::pair<key_type, mapped_type>;

    using insert_result = std::pair<iterator, bool>;
    using Vector::size;

    static_assert(std::is_constructible_v<VectorIter, mapped_type*>);
    static_assert(std::is_constructible_v<PairFromVectorIter<_Key, VectorIter>, mapped_type*>);
    static_assert(std::is_constructible_v<iterator, mapped_type*, mapped_type*>);

    auto make_iter(mapped_type* x) { return iterator{x, data()}; }
    auto make_const_iter(const mapped_type* x) const { return const_iterator{x, data()}; }

    explicit operator Vector() { return static_cast<Vector&>(*this); }
    explicit operator const Vector() const { return static_cast<const Vector&>(*this); }
  public:
    using Vector::data;
    using Vector::operator[];
    using Vector::at;

    // inherit some, but not all constructors
    _raw_vector_map() = default;
    //_raw_vector_map(const _raw_vector_map& x) = default;
    //_raw_vector_map(_raw_vector_map&& x) = default;

    template<class InputIt>
    _raw_vector_map(const InputIt& first, const InputIt& last) {
      DEBUG4(std::cout << "constructing raw vector map from range...\n");
      if(std::is_same_v<typename iterator_traits<InputIt>::iterator_category, std::random_access_iterator_tag>)
        Vector::reserve(distance(first, last));
      insert(first, last);
    }

    // ATTENTION: ERASE DOES NOT NECCESSARILY DO WHAT YOU EXPECT!
    // erase will just reinisialize x to the default element
    // if this is not what you want, you probably want to use a vector_map
    void erase(const key_type x) { data()[x] = mapped_type(); }
    void erase(const iterator it) { (*it).second = mapped_type(); } 

    template<class InputIt>
    void insert(InputIt first, const InputIt& last) {
      while(first != last){
        if(first->first >= size()) {
          Vector::reserve(first->first + 1);
          Vector::resize(first->first);
          Vector::emplace_back(first->second);
        } else operator[](first->first) = first->second;
        ++first;
      }
    }
    // ATTENTION: THIS EMPLACE DOES NOT ALWAYS DO WHAT YOU WOULD EXPECT!
    // in particular, if you first emplace(10, ...), then emplace(8, ...) will refuse to emplace since it assumes that all below 10 are already emplaced
    // if this is not what you want, use vector_map instead!
    template<class ...Args>
	  insert_result try_emplace(const key_type x, Args&&... args) {
      const size_t x_idx = static_cast<size_t>(x);
      if(x_idx >= size()) {
        Vector::reserve(x_idx + 1);
        Vector::resize(x_idx);
        Vector::emplace_back(std::forward<Args>(args)...);
        return {data() + x_idx, true };
      } else return {data() + x_idx, false };
    }

    template<class ...Args>
	  insert_result emplace(const key_type x, Args&&... args) { return try_emplace(x, std::forward<Args>(args)...); }
    
    // emplace_hint, ignoring the hint
    template<class Iter, class ...Args>
	  iterator emplace_hint(const Iter&, const key_type x, Args&&... args) { return try_emplace(x, std::forward<Args>(args)...).first; }
	  
    insert_result insert(const value_type& x) { return try_emplace(x.first, x.second); }


    iterator begin() { return make_iter(data()); }
    iterator end() { return make_iter(data() + size()); }
    const_iterator cbegin() const { return make_const_iter(data()); }
    const_iterator cend() const { return make_const_iter(data() + size()); }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }
    reverse_iterator rbegin() { return make_reverse_iterator(end()); }
    reverse_iterator rend() { return make_reverse_iterator(begin()); }
    reverse_const_iterator rbegin() const { return make_reverse_iterator(end()); }
    reverse_const_iterator rend() const { return make_reverse_iterator(begin()); }

    iterator find(const key_type x) { if(contains(x)) return data() + x; else return end(); }
    const_iterator find(const key_type x) const { if(contains(x)) return data() + x; else return end(); }
    
    bool contains(const key_type x) const { return static_cast<size_t>(x) < size(); }
    bool count(const key_type x) const { return contains(x); }
  };
}

