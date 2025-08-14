

#pragma once

#include "utils.hpp"
#include "stl_utils.hpp"
#include "trans_iter.hpp"

namespace mstd {

  // ========== raw_vector_map ==========
  // STL does not consider a vector a map, although it does behave similarly
  // The raw_vector_map serves as a base for both
  //  vector_map and iterable_bitset

  // ------- raw_vector_map: helpers ---------
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
 
  // ------- raw_vector_map: main class ---------
  template<std::unsigned_integral _Key, VectorType _base_container> 
  struct _raw_vector_map:
    public _base_container
  {
    // ------- static stuff --------
    using Vector = _base_container;
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

    static_assert(std::is_constructible_v<VectorIter, mapped_type*>);
    static_assert(std::is_constructible_v<PairFromVectorIter<_Key, VectorIter>, mapped_type*>);
    static_assert(std::is_constructible_v<iterator, mapped_type*, mapped_type*>);
    
    friend class ordered_bitset; 

    // ------- members --------
    // ------- construction & desctruction ---------
    _raw_vector_map() noexcept = default;

    template<class InputIt>
    _raw_vector_map(const InputIt& first, const InputIt& last) {
      DEBUG4(std::cout << "constructing raw vector map from range...\n");
      if(std::is_same_v<typename iterator_traits<InputIt>::iterator_category, std::random_access_iterator_tag>)
        Vector::reserve(distance(first, last));
      insert(first, last);
    }

    // ------- operators --------
    explicit operator Vector() { return static_cast<Vector&>(*this); }
    explicit operator const Vector() const { return static_cast<const Vector&>(*this); }

    // ------- methods: initialization --------
    // ------- methods: query --------
    using Vector::size;
    using Vector::operator[];
    using Vector::at;

  protected:
    using Vector::data;

    auto make_iter(mapped_type* x) { return iterator{x, data()}; }
    auto make_iter(const mapped_type* x) const { return const_iterator{x, data()}; }

  public:
    auto vector_begin() { return Vector::begin(); }
    auto vector_begin() const { return Vector::begin(); }
    auto vector_end() { return Vector::end(); }
    auto vector_end() const { return Vector::end(); }

    auto begin() { return make_iter(data()); }
    auto begin() const { return cbegin(); }
    auto cbegin() const { return make_iter(data()); }
    auto rbegin() { return make_reverse_iterator(end()); }
    auto rbegin() const { return make_reverse_iterator(end()); }

    auto end() { return vector_end(); } //make_iter(data() + size()); }
    auto end() const { return cend(); }
    auto cend() const { return make_iter(data() + size()); }
    auto rend() { return make_reverse_iterator(begin()); } 
    auto rend() const { return make_reverse_iterator(begin()); }

    auto find(const key_type x) { if(contains(x)) return iterator{data() + x}; else return make_iter(data() + size()); }
    auto find(const key_type x) const { if(contains(x)) return const_iterator{data() + x}; else return make_iter(data() + size()); }
    
    bool contains(const key_type x) const { return static_cast<size_t>(x) < size(); }
    bool count(const key_type x) const { return contains(x); }

    // ------- methods: modification --------
  public:
    // ATTENTION: ERASE DOES NOT NECCESSARILY DO WHAT YOU EXPECT!
    // erase will just reinitialize x to the default element
    // if this is not what you want, you probably want to use a vector_map
    void erase(const key_type x) { data()[x] = mapped_type(); }
    void erase(const iterator it) { (*it).second = mapped_type(); } 

    // ATTENTION: THIS EMPLACE DOES NOT ALWAYS DO WHAT YOU WOULD EXPECT!
    // If you emplace(10, ...), then all keys i < 10 are emplaced with default values!
    // In particular, emplace(8, ...) will then refuse to emplace, since key 8 is already present
    // if this is not what you want, you probably want to use a vector_map
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
  };

  template<std::unsigned_integral _Key, class _Element, class Allocator = std::allocator<_Element>>
  using raw_vector_map = _raw_vector_map<_Key, std::vector<_Element, Allocator>>;

  // ------- raw_vector_map: factories ---------
  
  // ------- raw_vector_map: concepts ---------
  template<std::unsigned_integral _Key, VectorType _base_container>
  constexpr bool is_vector_v<_raw_vector_map<_Key, _base_container>> = false;
 
  // ------- raw_vector_map: deduction guides ---------
  // ------- raw_vector_map: defaults ---------

}

