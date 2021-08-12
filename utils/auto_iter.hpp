
#pragma once

#include "stl_utils.hpp"

namespace std{

  // a forward iterator that knows the end of the container & converts to false if it's at the end
  //NOTE: this also supports that the end iterator has a different type than the iterator, as long as they can be compared with "!="
  template<class Iterator, class EndIterator = Iterator>
  class auto_iter: public Iterator
  {
    using Parent = Iterator;
    EndIterator end_it;
   public:
    using Traits = my_iterator_traits<Iterator>;
    using Parent::Parent;

    auto_iter(const auto_iter&) = default;
    auto_iter(auto_iter&&) = default;

    template<IterableType Container> requires (is_convertible_v<iterator_of_t<Container>, Iterator>)
    auto_iter(Container&& c): Parent(begin(c)), end_it(end(c)) {}
    
    constexpr auto_iter(const Iterator& it, const Iterator& _end): Parent(it), end_it(_end) {}

    bool is_valid() const { return *this != end_it; }
    bool is_invalid() const { return !is_valid(); }
    operator bool() const { return is_valid(); }
    operator bool() { return is_valid(); }
    Iterator get_iter() const { return *this; }
  };

  template<IterableType Container,
           class Iterator = iterator_of_t<Container>,
           class EndIterator = Iterator>
  auto_iter<Iterator, EndIterator> get_auto_iter(Container&& c) { return c; }

  template<IterableType Container,
           class KeyType,
           class Iterator = iterator_of_t<Container>,
           class EndIterator = Iterator>
  auto_iter<Iterator, EndIterator> auto_find(Container&& c, const KeyType& key) { return {c.find(key), end(c)}; }
 
} //namespace
