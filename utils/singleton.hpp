
#pragma once

#include <memory>

namespace std{

  // a set holding exactly one element, but having a set-interface
  template<class _Element, class ElementContainer>
  class _singleton_set
  {
  protected:
    ElementContainer element;

    virtual void remove_element() = 0;

    _singleton_set(const ElementContainer& _c): element(_c) {}
  public:
    using iterator = _Element*;
    using const_iterator = const _Element*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    using value_type = _Element;
    using reference = value_type&;
    using const_reference = const value_type&;

    _singleton_set() = default;
    _singleton_set(const _singleton_set&) = default;
    _singleton_set(_singleton_set&&) = default;

    virtual ~_singleton_set() = 0;
    virtual bool empty() const = 0;
    virtual bool erase(const iterator it)
    {
      if(empty() || (it != begin())) return false;
      remove_element();
      return true;
    }
    bool erase(const _Element& el)
    {
      if(empty() || (el != *element)) return false;
      remove_element();
      return true;
    }
    void clear() { if(!empty()) erase(begin()); }

    size_t size() const { return !empty(); }
    reference       front() { return *element; }
    const_reference front() const { return *element; }
    iterator       find(const _Element& x) { return (!empty() && (x == *element)) ? begin() : end(); }
    const_iterator find(const _Element& x) const { return (!empty() && (x == *element)) ? begin() : end(); }
    bool contains(const _Element& x) const { return empty() ? false : x == front(); }
    void reserve(const size_t) {} // compatibility with vector

    virtual iterator begin() = 0;
    virtual const_iterator begin() const = 0;
  
    reverse_iterator rbegin() { return end(); }
    const_reverse_iterator rbegin() const { return end(); }
    iterator end() { return begin() + size(); }
    const_iterator end() const { return begin() + size(); }
    reverse_iterator rend() { return rbegin() + size(); }
    const_reverse_iterator rend() const { return rbegin() + size(); }
  };
  template<class _Element, class ElementContainer>
  _singleton_set<_Element, ElementContainer>::~_singleton_set() {};


  // if we are given an invliad element, use it to represent the empty singleton_set
  template<class _Element, class InvalidElement = void>
  class singleton_set: public _singleton_set<_Element, self_deref<_Element>>
  {
    using Parent = _singleton_set<_Element, self_deref<_Element>>;
  protected:
    using Parent::element;
    static constexpr _Element _invalid_element = InvalidElement::value();
    
    void remove_element() { element = _invalid_element; }
    inline _Element* addr() { return element.operator->(); }
    inline const _Element* addr() const { return element.operator->(); }
  public:
    using typename Parent::iterator;
    using typename Parent::const_iterator;
    using Parent::Parent;
    using Parent::clear;

    singleton_set(): Parent(_invalid_element) {}

    template<class... Args>
    pair<iterator, bool> emplace(Args&&... args) 
    {
      if(empty()){
        new(addr()) _Element(forward<Args>(args)...);
        return {addr(), true};
      } else {
        if(*element == _Element(forward<Args>(args)...))
          return {addr(), false};
        else throw logic_error("trying to add second element to singleton set");
      }
    }

    template<class... Args>
    pair<iterator, bool> emplace_back(Args&&... args) { emplace(std::forward<Args>(args)...); }
    
    template<class Iter>
    void insert(const iterator _ins, Iter src_begin, const Iter& src_end)
    { while(src_begin != src_end) emplace(*src_begin); }
    
    singleton_set& operator=(const _Element& e) { clear(); emplace(e); return *this; }
    singleton_set& operator=(_Element&& e) { clear(); emplace(move(e)); return *this; }

    void push_back(const _Element& el) { emplace(el); }
    bool empty() const { return *element == _invalid_element; }
    iterator begin() { return addr(); }
    const_iterator begin() const { return addr(); }
  };

  // if we have no invalid element, we'll store a pointer instead and use nullptr to represent the empty singleton_set
  template<class _Element>
  class singleton_set<_Element, void>: public _singleton_set<_Element, shared_ptr<_Element>>
  {
    using Parent = _singleton_set<_Element, shared_ptr<_Element>>;
  protected:
    using Parent::element;

    void remove_element() { element.reset(); }
  public:
    using typename Parent::iterator;
    using typename Parent::const_iterator;
    using Parent::Parent;
    using Parent::clear;

    template<class... Args>
    pair<iterator, bool> emplace(Args&&... args) 
    {
      if(empty()){
        element = make_unique<_Element>(forward<Args>(args)...);
        return {element.get(), true};
      } else {
        if(*element == _Element(forward<Args>(args)...))
          return {element.get(), false};
        else throw logic_error("trying to add second element to singleton set");
      }
    }

    template<class... Args>
    pair<iterator, bool> emplace_back(Args&&... args) { emplace(std::forward<Args>(args)...); }

    template<class Iter>
    void insert(const iterator _ins, Iter src_begin, const Iter& src_end)
    { while(src_begin != src_end) emplace(*src_begin); }

    singleton_set& operator=(const _Element& e) { clear(); emplace(e); return *this; }
    singleton_set& operator=(_Element&& e) { clear(); emplace(move(e)); return *this; }

    void push_back(const _Element& el) { emplace(el); }
    bool empty() const { return !element; }
    iterator begin() { return element.get(); }
    const_iterator begin() const { return element.get(); }
  };

}
