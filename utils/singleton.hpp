
#pragma once

#include "optional.hpp"

namespace std{

  // a set holding at most one element, but having a set-interface
  template<Optional Container>
  class singleton_set: public iter_traits_from_reference<typename Container::value_type&> {
    using traits = iter_traits_from_reference<typename Container::value_type&>;
    Container storage;
  public:
    using typename traits::value_type;
    using typename traits::reference;
    using typename traits::const_reference;
    using iterator = value_type*;
    using const_iterator = const value_type*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    singleton_set() = default;
    singleton_set(const value_type& el): storage(std::in_place_t(), el) {}
    singleton_set(value_type&& el): storage(std::in_place_t(), std::move(el)) {}

    bool non_empty() const { return storage.has_value(); }
    bool empty() const { return !non_empty(); }
    void clear() { storage.reset(); }

    void push_back(value_type&& el) {
      assert((non_empty(), "trying to add second element to singleton set"));
      storage.emplace(move(el));
    }
    void push_back(const value_type& el) {
      assert((non_empty(), "trying to add second element to singleton set"));
      storage.emplace(el);
    }

    singleton_set& operator=(value_type&& x) { clear(); push_back(move(x)); return *this; }
    singleton_set& operator=(const value_type& x) { clear(); push_back(x); return *this; }
    //singleton_set& operator=(const singleton_set& s) = default;

    bool erase(const iterator& it) {
      if(empty() || (it != begin())) return false;
      clear();
      return true;
    }
    bool erase(const value_type& el) {
      if(empty() || (el != *storage)) return false;
      clear();
      return true;
    }

    template<class... Args>
    constexpr pair<iterator, bool> emplace(Args&&... args) {
      if(empty()){
        value_type& emplace_result = storage.emplace(forward<Args>(args)...);
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
    size_t count(const_reference x) const { return non_empty() ? (x == front()) : 0; }
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

    bool operator==(const singleton_set& other) const {
      return empty() ? other.empty() : (storage == other.storage);
    }
    bool operator!=(const singleton_set& other) const { return !operator==(other); }
  };

  template<class T> struct is_singleton_set: public std::false_type {};
  template<class T> struct is_singleton_set<singleton_set<T>>: public std::true_type {};
  template<class T> constexpr bool is_singleton_set_v = is_singleton_set<T>::value;
  template<class T> concept SingletonSetType = is_singleton_set_v<remove_cvref_t<T>>;

}
