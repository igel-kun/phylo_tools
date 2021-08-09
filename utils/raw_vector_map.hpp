
// STL does not consider a vector a map, although it does behave similarly

#pragma once

#include "utils.hpp"
#include "stl_utils.hpp"

namespace std {

  // proxy classes for value_type and reference in vector-map-iterators
  template<unsigned_integral _Key, class _Value>
  struct value_type_proxy: public pair<_Key, _Value> {
    using Parent = pair<_Key, _Value>;
    using Parent::Parent;
  };
  template<unsigned_integral _Key, class _Value>
  struct reference_proxy: public pair<_Key, _Value&> {
    using Parent = pair<_Key, _Value&>;
    using Parent::Parent;
    reference_proxy(value_type_proxy<_Key, _Value>& val_proxy):
      Parent(val_proxy.first, val_proxy.second) {}
  };
  template<unsigned_integral _Key, class _Value>
  struct reference_proxy<_Key, const _Value>: public pair<_Key, const _Value&> {
    using Parent = pair<_Key, const _Value&>;
    using Parent::Parent;
    reference_proxy(value_type_proxy<_Key, _Value>& val_proxy):
      Parent(val_proxy.first, val_proxy.second) {}
    reference_proxy(const value_type_proxy<_Key, _Value>& val_proxy):
      Parent(val_proxy.first, val_proxy.second) {}
  };

  /*
  template<unsigned_integral _Key, class _Value>
  struct common_reference<reference_proxy<_Key, _Value>&&, value_type_proxy<_Key, _Value>&>{
    using type = reference_proxy<_Key, _Value>;
  };
  template<unsigned_integral _Key, class _Value>
  struct common_reference<value_type_proxy<_Key, _Value>&, reference_proxy<_Key, _Value>&&>{
    using type = reference_proxy<_Key, _Value>;
  };
  template<unsigned_integral _Key, class _Value>
  struct common_reference<reference_proxy<_Key, _Value>&&, const value_type_proxy<_Key, _Value>&>{
    using type = const reference_proxy<_Key, _Value>;
  };
  template<unsigned_integral _Key, class _Value>
  struct common_reference<const value_type_proxy<_Key, _Value>&, reference_proxy<_Key, _Value>&&>{
    using type = const reference_proxy<_Key, _Value>;
  };
  */

  template<unsigned_integral _Key, class _Element>
  class raw_vector_map;

  template<unsigned_integral _Key, class _Element>
  class raw_vector_map_iterator
  {
    _Element* start;
    size_t index = 0;
  public:
    // NOTE: reference cannot be value_type& since a (key, value) pair does not physically exist in memory
    //       however: this breaks the std::forward_iterator concept!
    using value_type  = value_type_proxy<_Key, _Element>;
    using reference   = reference_proxy<_Key, _Element>;
    using const_reference = reference_proxy<_Key, const _Element>;
    using pointer       = self_deref<reference>;
    using const_pointer = self_deref<const_reference>;
    using difference_type = ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    raw_vector_map_iterator(_Element* const & _start, size_t _index = 0): 
      start(_start),
      index(_index)
    {}
    raw_vector_map_iterator():
      raw_vector_map_iterator(nullptr)
    {}
    // convert iterators to const_iterators
    template<class __Element, class = std::enable_if_t<std::is_same_v<_Element, const __Element>>>
    raw_vector_map_iterator(const raw_vector_map_iterator<_Key, __Element>& other):
      start(other.start),
      index(other.index)
    {}

    raw_vector_map_iterator(const raw_vector_map_iterator&) = default;
    raw_vector_map_iterator(raw_vector_map_iterator&&) = default;
    raw_vector_map_iterator& operator=(const raw_vector_map_iterator&) = default;
    raw_vector_map_iterator& operator=(raw_vector_map_iterator&&) = default;

    reference operator*() const { return {static_cast<_Key>(index), *(start + index)}; }
    pointer operator->() const { return operator*(); }

    //! increment & decrement operators
    raw_vector_map_iterator& operator++() { ++index; return *this; }
    raw_vector_map_iterator operator++(int) { raw_vector_map_iterator tmp(*this); ++(*this); return tmp; }
    raw_vector_map_iterator& operator--() { --index; return *this; }
    raw_vector_map_iterator operator--(int) { raw_vector_map_iterator tmp(*this); --(*this); return tmp; }

    raw_vector_map_iterator& operator+=(const difference_type x) { index += x; return *this; }
    raw_vector_map_iterator& operator-=(const difference_type x) { index -= x; return *this; }
    raw_vector_map_iterator  operator+(const difference_type x) const { return {start, index + x}; }
    raw_vector_map_iterator  operator-(const difference_type x) const { return {start, index - x}; }

    difference_type operator-(const raw_vector_map_iterator& it) const { return (start + index) - (it.start + it.index); }

    template<unsigned_integral __Key, class __Element>
    bool operator==(const raw_vector_map_iterator<__Key, __Element>& x) const { return index == x.index; }
    template<unsigned_integral __Key, class __Element>
    bool operator!=(const raw_vector_map_iterator<__Key, __Element>& x) const { return index != x.index; }
    template<unsigned_integral __Key, class __Element>
    friend class raw_vector_map_iterator;
  };


  // NOTE: to forbid implicit casting of raw_vector_map to vector, we use this intermediate class which forbids copy construction from raw_vector_map
  // (see https://stackoverflow.com/questions/36473354/prevent-derived-class-from-casting-to-base)
  template<class T>
  class _vector: public vector<T>
  {
  public:
    using vector<T>::vector;

    template<unsigned_integral _Key, class _Element>
    _vector(const raw_vector_map<_Key, _Element>&) = delete;
  };


  template<unsigned_integral _Key, class _Element>
  class raw_vector_map: public _vector<_Element>
  {
    using Parent = _vector<_Element>;
  public:
    using key_type = const _Key;
    using mapped_type = _Element;
    using value_type = pair<key_type, _Element>;

    using iterator = raw_vector_map_iterator<_Key, _Element>;
    using const_iterator = raw_vector_map_iterator<_Key, const _Element>;
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
    raw_vector_map(const raw_vector_map& x): Parent(static_cast<const Parent&>(x)) {}
    raw_vector_map(raw_vector_map&& x): Parent(static_cast<Parent&&>(x)) {}
    raw_vector_map& operator=(const raw_vector_map& x) { return Parent::operator=(static_cast<const Parent&>(x)); }
    raw_vector_map& operator=(raw_vector_map&& x) { return Parent::operator=(static_cast<Parent&&>(x)); }
   

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

    mapped_type& operator[](const key_type& key) { assert(key < size()); return Parent::operator[]((size_t)key); }
    const mapped_type& operator[](const key_type& key) const { assert(key < size()); return Parent::operator[]((size_t)key); }
    mapped_type& at(const key_type& key) { return Parent::at((size_t)key); }
    const mapped_type& at(const key_type& key) const { return Parent::at((size_t)key); }

    iterator begin() { return {data()}; }
    iterator end() { return {data(), size()}; }
    const_iterator begin() const { return {data()}; }
    const_iterator end() const { return {data(), size()}; }
    const_iterator cbegin() const { return {data()}; }
    const_iterator cend() const { return {data(), size()}; }
    reverse_iterator rbegin() { return make_reverse_iterator(end()); }
    reverse_iterator rend() { return make_reverse_iterator(begin()); }
    reverse_const_iterator rbegin() const { return make_reverse_iterator(end()); }
    reverse_const_iterator rend() const { return make_reverse_iterator(begin()); }

    iterator find(const key_type x) { if(contains(x)) return {data(), x}; else return end(); }
    const_iterator find(const key_type x) const { if(contains(x)) return {data(), x}; else return end(); }
    
    bool contains(const key_type x) const { return (size_t)x < size(); }
    bool count(const key_type x) const { return contains(x); }
  };

/*
using T = raw_vector_map<size_t, int>;
using I = typename T::iterator;
using RT = typename I::reference;
using VT = typename I::value_type;
static_assert(std::copy_constructible<T>);
static_assert(std::is_object_v<T>);
static_assert(std::move_constructible<T>);
static_assert(std::is_lvalue_reference_v<T&>);
static_assert(std::common_reference_with<const std::remove_reference_t<T&>&, const std::remove_reference_t<T>&>);
static_assert(std::assignable_from<T&, T>);
static_assert(std::swappable<T>);
static_assert(std::assignable_from<T&, T&>);
static_assert(std::assignable_from<T&, const T&>);
static_assert(std::assignable_from<T&, const T>);
static_assert(std::regular<T>);
static_assert(std::swappable<T>);
static_assert(std::common_reference_with<RT&&, VT&>);
//[with _Tp = std::raw_vector_map_iterator<long unsigned int, int>; _Tp = std::raw_vector_map_iterator<long unsigned int, int>]
static_assert(std::common_reference_with<std::iter_rvalue_reference_t<I>&&, const VT&>);
static_assert(std::common_reference_with<
  std::iter_rvalue_reference_t<I>&&,
  const typename std::__detail::__iter_traits_impl<I, std::indirectly_readable_traits<I>>::type::value_type&
>);
//[with _In = std::raw_vector_map_iterator<long unsigned int, int>; _Tp = std::raw_vector_map_iterator<long unsigned int, int>
static_assert(std::forward_iterator<I>);

using CI = typename T::const_iterator;
static_assert(std::input_or_output_iterator<CI>);
using T1 = typename CI::reference&&;
using U1 = typename CI::value_type&;
//static_assert(std::same_as<std::common_reference_t<T1, U1>, std::common_reference_t<U1, T1>>);
//static_assert(std::convertible_to<T1, std::common_reference_t<T1, U1>>);
//static_assert(std::convertible_to<U1, std::common_reference_t<T1, U1>>);
static_assert(std::common_reference_with<T1, U1>);
static_assert(std::common_reference_with<std::iter_reference_t<CI>&&, std::iter_value_t<CI>&>);
static_assert(std::common_reference_with<std::iter_reference_t<CI>&&, std::iter_rvalue_reference_t<CI>&&>);
static_assert(std::common_reference_with<std::iter_rvalue_reference_t<CI>&&, const std::iter_value_t<CI>&>);
static_assert(std::indirectly_readable<CI>);
static_assert(std::input_iterator<CI>);
static_assert(std::derived_from<std::random_access_iterator_tag, std::forward_iterator_tag>);
static_assert(std::incrementable<CI>);
static_assert(std::sentinel_for<CI, CI>);
static_assert(std::forward_iterator<typename T::const_iterator>);
static_assert(std::IterableType<T>);
static_assert(ContainerType<T>);
static_assert(MapType<T>);
*/
}
