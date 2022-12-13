

// by default std::multiset stores instances of multiple copies seperately
// if you don't want to waste this kind of space, you may want a multimap that counts instead

#pragma once

#include "iter_factory.hpp"
#include "generator_iter.hpp"

namespace mstd {
  template<class, class, class, class> class unordered_counting_multiset;

  // the iterator of the counting multiset is a concatenating iter that, for each pair<x,k> creates a generator_iter that returns k times x
  template<ContainerType Storage>
  using counting_multiset_iterator = concatenating_iterator<Storage, IteratorFactory<generator_iter<reference_of_t<Storage>>>;

  template<class Key,
           class Hash = std::hash<Key>,
           class KeyEqual = std::equal_to<Key>,
           class Allocator = std::allocator< std::pair<const Key, size_t>>>
  class unordered_counting_multiset: public std::unordered_map<Key, size_t, Hash, KeyEqual, Allocator> {
    using Parent = std::unordered_map<Key, size_t, Hash, KeyEqual, Allocator>;
    size_t _real_size = 0;
  public:
    using iterator = counting_multiset_iterator<Parent>;
    using const_iterator = counting_multiset_iterator<const Parent>;

    unordered_counting_multiset() {}

    template<SetType S>
    unordered_counting_multiset(S&& s) {
      _real_size = s.size();
      for(auto&& i: std::forward<S>(s))
        Parent::emplace(i, 1);
    }
    unordered_counting_multiset(const unordered_multiset& other):
      Parent(static_cast<const Parent&>(other)), _real_size(other._real_size) {}
    unordered_counting_multiset(unordered_multiset&& other):
      Parent(static_cast<Parent&&>(other)), _real_size(other._real_size) {}

    size_t count(const Key& key) const {
      const auto it = _storage.find(key);
      if(it != _storage.end())
        return it->second;
      else return 0;
    }

    template<class... Args>
    iterator emplace(Args&&... args) {
      auto [it, success] = Parent::emplace(std::forward<Args>(args)...);
      ++_real_size;
      if(!success)
        ++(iter->second);
      return *it;
    }
    iterator find(const Key& key) {
      auto it = Parent::find(key);
      if(it != Parent::end())
        return *it;
      else return iterator();
    }
    auto begin() { if(empty()) return iterator(); else return iterator(*begin()); }
    auto begin() const { if(empty()) return const_iterator(); else return const_iterator(*begin()); }
    auto end() const { return GenericEndIterator(); }
  };

}

