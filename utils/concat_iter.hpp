
#pragma once

#include "std_utils.hpp"

namespace std {
  // this is an iterator over multiple iterable objects, passing over each of them in turn, effectively concatenating them
  template<IterableType Container>
  class concatenating_iterator: public my_iterator_traits<iterator_of_t<Container>> {
    using Parent = my_iterator_traits<iterator_of_t<Container>>;

    using ContainerRef = reference_wrapper<Container>;
    using ContainerIter = iterator_of_t<vector<ContainerRef>>;
    using ItemIter = iterator_of_t<Container>;
    vector<ContainerRef> containers;
    ContainerIter cit;
    ItemIter it;
  public:
    using Parent::value_type;
    using Parent::reference;
    using Parent::pointer;
    using iterator_category = std::forward_iterator_tag;

    concatenating_iterator() = default;
    concatenating_iterator(concatenating_iterator&&) = default;
    concatenating_iterator(const concatenating_iterator&) = default;

    concatenating_iterator& operator=(const concatenating_iterator&) = default;
    concatenating_iterator& operator=(concatenating_iterator&&) = default;

    bool operator==(const concatenating_iterator& other) const {
      if(is_valid()){
        return it == other.it;
      } else return !other.is_valid();
    }
    bool operator!=(const concatenating_iterator& other) const { return !operator==(other); }

    // construct from only one given container
    template<class First> requires (!is_same_v<remove_cvref_t<First>,concatenating_iterator>)
    concatenating_iterator(First&& first):
      containers(1, first),
      cit(containers.begin())
    {
      if(is_valid()) it = cit->begin();
    }

    // construct from n > 1 given containers by delegating construction to n-1 given containers
    template<class First, class Last, class... Args> requires (!is_same_v<remove_cvref_t<First>,concatenating_iterator>)
    concatenating_iterator(First&& first, Args&&... other, Last&& last):
      concatenating_iterator(forward<First>(first), forward<Args>(other)...)
    {
      containers.push_back(forward<Last>(last));
    }

    // NOTE: do not attempt to call ++ on the end-iterator lest you see segfaults
    concatenating_iterator& operator++() {
      ++it;
      if(it == cit->end())
        while((cit != containers.end()) && cit->empty())
          ++cit;
      if(cit != containers.end()) it = cit->begin();
      return *this;
    }

    concatenating_iterator& operator++(int) { concatenating_iterator result(*this); ++this; return result; }

    reference operator*() const { return *it; }
    pointer operator->() const { return &(*it); }
    bool is_valid() const { return cit != containers.end(); }
  };

}
