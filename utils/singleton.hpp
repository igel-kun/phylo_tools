
#pragma once

#include <memory>

namespace std{

  // a set holding exactly one element, but having a set-interface
  template<class _Element>
  class SingletonSet
  {
    unique_ptr<_Element> element;

  public:
    using iterator = _Element*;
    using const_iterator = const _Element*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    using value_type = _Element;
    using reference = value_type&;
    using const_reference = const reference;

    SingletonSet() = default;
    SingletonSet(const SingletonSet&) = default;
    SingletonSet(SingletonSet&&) = default;

    template<class... Args>
    pair<iterator, bool> emplace(Args&&... args) {
      _Element* p = new _Element(forward<Args>(args)...);
      if(empty()){
        element.reset(p);
        return {element.get(), true};
      } else {
        if(*element == *p)
          return {element.get(), false};
        else throw logic_error("trying to add second element to singleton set");
      }
    }
    template<class... Args>
    pair<iterator, bool> insert(Args&&... args) { return emplace(forward<Args>(args)...); }
  
    bool erase(const iterator it){
      if(it == element){
        element.release();
        return true;
      } else return false;
    }
    bool erase(const _Element& el){
      if(!empty()){
        if(el == *element){
          element.release();
          return true;
        } else return false;
      } else return false;
    }

    size_t size() const { return !empty(); }
    bool empty() const { return element.get() == nullptr; }
    reference       front() { return *element; }
    const_reference front() const { return *element; }
    iterator       find(const _Element& x) { return empty() ? end() : ((x == *element) ? begin() : end()); }
    const_iterator find(const _Element& x) const { return empty() ? end() : ((x == *element) ? begin() : end()); }
    
    iterator begin() { return element.get(); }
    const_iterator begin() const { return element.get(); }
    reverse_iterator rbegin() { return element.get(); }
    const_reverse_iterator rbegin() const { return element.get(); }
    iterator end() { return begin() + size(); }
    const_iterator end() const { return begin() + size(); }
    reverse_iterator rend() { return rbegin() + size(); }
    const_reverse_iterator rend() const { return rbegin() + size(); }
  };

}
