
#pragma once

#include <optional>

namespace std{

  // if a type has a specific invalid element (like nullptr), then this can be used to indicate that the singleton is absent
  template<class T, T invalid>
  struct SingletonByInvalid {
    using value_type = T;
    T element = invalid;

    // in-place construct the element
    template<class... Args>
    SingletonByInvalid(const std::in_place_t, Args&&... args):
      T(std::forward<Args>(args)...) {}
    
    // re-construct the element
    template<class... Args>
    T& emplace(Args&&... args){
      T const * addr = &element;
      addr->~T();
      new(addr) T(std::forward<Args>(args)...);
      return *addr;
    }
    T& operator*() { return element; }
    const T& operator*() const { return element; }

    void reset() { emplace(invalid); }
    operator bool() const { return element == invalid; }
  };
  // if a type does not have a specific invalid element, then we'll use std::optional (which stores 1 additional byte for the "absence" information)



  // a set holding at most one element, but having a set-interface
  template<class ElementContainer>
  class _singleton_set {
    ElementContainer storage;
  public:
    using value_type = typename ElementContainer::value_type;
    using difference_type = ptrdiff_t;
    using size_type = size_t;
    using iterator = value_type*;
    using const_iterator = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using rvalue_reference = value_type&&;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    _singleton_set() = default;
    _singleton_set(const value_type& el): storage(std::in_place_t(), el) {}
    _singleton_set(value_type&& el): storage(std::in_place_t(), std::move(el)) {}

    bool non_empty() const { return static_cast<bool>(storage); }
    bool empty() const { return !non_empty(); }
    void clear() { storage.reset(); }

    template<class X, class = enable_if_t<is_same_v<remove_cvref_t<X>, remove_cvref_t<value_type>>>>
    void push_back(X&& el) {
      assert((non_empty(), "trying to add second element to singleton set"));
      storage.emplace(forward<X>(el));
    }

    template<class X, class = enable_if_t<is_same_v<remove_cvref_t<X>, remove_cvref_t<value_type>>>>
    _singleton_set& operator=(X&& x) { clear(); push_back(forward<X>(x)); return *this; }
    //singleton_set& operator=(const singleton_set& s) = default;

    bool erase(const iterator& it) {
      if(empty() || (it != begin())) return false;
      clear();
      return true;
    }
    bool erase(const_reference el) {
      if(empty() || (el != *storage)) return false;
      clear();
      return true;
    }

    template<class... Args>
    pair<iterator, bool> emplace(Args&&... args) {
      if(empty()){
        reference emplace_result = storage.emplace(forward<Args>(args)...);
        return {&emplace_result, true};
      } else {
        assert(false && "trying to add second element to singleton set");
        exit(-1);
      }
    }

    template<class... Args>
    pair<iterator, bool> emplace_back(Args&&... args) { emplace(forward<Args>(args)...); }
 
    template<class Iter>
    void insert(const Iter& src_begin, const Iter& src_end)
    { 
      if(src_begin != src_end) emplace(*src_begin);
      assert(("trying to add second element to singleton set", next(src_begin) == src_end));
    }

    template<class Iter>
    void insert(const iterator& _ins, const Iter& src_begin, const Iter& src_end)
    {
      assert(("trying to add second element to singleton set", _ins == begin()));
      insert(src_begin, src_end);
    }
    
    operator const_reference() const { return front(); }
    operator reference() { return front(); }
    size_t size() const { return non_empty(); }
    reference       front() { return *storage; }
    const_reference front() const { return *storage; }
    iterator       find(const_reference x) { return (non_empty() && (x == *storage)) ? begin() : end(); }
    const_iterator find(const_reference x) const { return (non_empty() && (x == *storage)) ? begin() : end(); }
    uint_fast8_t count(const_reference x) const { return non_empty() ? (x == front()) : 0; }
    bool contains(const_reference x) const { return count(x); }
    void reserve(const size_t) const {} // compatibility with vector

    iterator begin() { return empty() ? nullptr : &(front()); }
    const_iterator begin() const { return empty() ? nullptr : &(front()); }
    iterator end() { return begin() + non_empty(); }
    const_iterator end() const { return begin() + non_empty(); }

    reverse_iterator rbegin() { return end(); }
    const_reverse_iterator rbegin() const { return end(); }
    reverse_iterator rend() { return rbegin() + non_empty(); }
    const_reverse_iterator rend() const { return rbegin() + non_empty(); }

    bool operator==(const _singleton_set& other) const {
      return empty() ? other.empty() : (storage == other.storage);
    }
    bool operator!=(const _singleton_set& other) const { return !operator==(other); }
  };

  // to declare a singleton set, you can either provide an invalid-getter (a struct containing a value attribute) or void (triggering the use of std::optional)
  template<class T, class InvalidGetter>
  struct __singleton_set { using type = _singleton_set<SingletonByInvalid<T, InvalidGetter::value>>; };
  template<class T>
  struct __singleton_set<T, void> { using type = _singleton_set<std::optional<T>>; };

  template<class T, class InvalidGetter = void>
  using singleton_set = typename __singleton_set<T, InvalidGetter>::type;

}
