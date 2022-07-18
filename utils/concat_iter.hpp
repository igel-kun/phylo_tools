
#pragma once

#include "stl_utils.hpp"

namespace mstd {
  // this is an iterator over multiple iterable objects, passing over each of them in turn, effectively concatenating them
  template<class ContainerIter, class ItemIter = iterator_of_t<value_type_of_t<ContainerIter>>>
  class _concatenating_iterator: public iterator_traits<ItemIter> {
    using Parent = iterator_traits<ItemIter>;
    using Container = value_type_of_t<ContainerIter>;

    auto_iter<ContainerIter> cit;
    ItemIter it;
  public:
    using typename Parent::value_type;
    using typename Parent::reference;
    using typename Parent::pointer;
    using iterator_category = std::forward_iterator_tag;

    _concatenating_iterator() = default;
    _concatenating_iterator(_concatenating_iterator&&) = default;
    _concatenating_iterator(const _concatenating_iterator&) = default;

    _concatenating_iterator& operator=(const _concatenating_iterator&) = default;
    _concatenating_iterator& operator=(_concatenating_iterator&&) = default;

    bool operator==(const _concatenating_iterator& other) const {
      return is_valid() ? (it == other.it) : !other.is_valid();
    }

    // construct the container auto_iter from anything (could be a container of containers or a compatible auto_iter or 2 ContainerIter, etc)
    template<class... Args>
    _concatenating_iterator(Args&&... args): cit(forward<Args>(args)...) {
      if(is_valid()) it = cit->begin(); // if the given list of containers is not empty, get the first item of the first container
    }

    // NOTE: do not attempt to call ++ on the end-iterator lest you see segfaults
    _concatenating_iterator& operator++() {
      ++it;
      if(it == cit->end())
        while(cit.is_valid() && cit->empty())
          ++cit;
      if(cit.is_valid()) it = cit->begin();
      return *this;
    }

    _concatenating_iterator& operator++(int) { _concatenating_iterator result(*this); ++this; return result; }

    reference operator*() const { return *it; }
    pointer operator->() const { return &(*it); }
    bool is_valid() const { return cit.is_valid(); }
  };

  // if the first template argument is iterable, then derive the iterator type from it
  template<class T, class ItemIter = iterator_of_t<value_type_of_t<T>>>
  using concatenating_iterator = _concatenating_iterator<iterator_of_t<T>, ItemIter>;

  // factories
  template<class T, class ItemIter = iterator_of_t<value_type_of_t<T>>, class BeginEndTransformation = void>
  using ConcatenatingIterFactory = IterFactory<concatenating_iterator<T, ItemIter>, BeginEndTransformation>;
}
