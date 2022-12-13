
#pragma once

#include "stl_utils.hpp"
#include "iter_factory.hpp"

namespace mstd {
  // from a container Iterator and a possible factory, get the iterator of the items in the container/factory
  template<class ContainerIter, class Factory = void>
  using ItemIterFromContainerAndFactory = iterator_of_t<VoidOr<Factory, value_type_of_t<ContainerIter>>>;

  // this is an iterator over multiple iterable objects, passing over each of them in turn, effectively concatenating them
  // NOTE: all iterable objects must admit the same iterator type 'ItemIter'
  // NOTE: if your container does not contain iteratables, you can use the last template argument to pass an iterator factory
  //       that will build an iterable from the value_type of your container
  //       WARNING: the IterFactory will only be build temporarily and a single iterator will be taken from it before being destroyed!
  template<class ContainerIter,
           class Factory = void,
           class ItemIter = ItemIterFromContainerAndFactory<ContainerIter, Factory>>
  class _concatenating_iterator: public auto_iter<ContainerIter> {
    using Parent = auto_iter<ContainerIter>;
    using Container = value_type_of_t<ContainerIter>;
    using ContainerRef = VoidOr<Factory, reference_of_t<ContainerIter>>;
    using ContainerConstRef = VoidOr<Factory, const_reference_of_t<ContainerIter>>;

    ContainerRef get_container() { return Parent::operator*(); }
    ContainerConstRef get_container() const { return Parent::operator*(); }

    ItemIter it;
  public:
    using Iterator = Parent;
    using Traits = mstd::iterator_traits<ItemIter>;
    using value_type      = typename Traits::value_type;
    using reference       = typename Traits::reference;
    using const_reference = typename Traits::const_reference;
    using pointer         = typename Traits::pointer;
    using const_pointer   = typename Traits::const_pointer;
    using iterator_category = std::forward_iterator_tag;
    using Parent::is_valid;

    _concatenating_iterator() = default;
    _concatenating_iterator(_concatenating_iterator&&) = default;
    _concatenating_iterator(const _concatenating_iterator&) = default;

    // construct the container auto_iter from anything (could be a container of containers or a compatible auto_iter or 2 ContainerIter, etc)
    template<class First, class... Args> requires (!std::is_same_v<_concatenating_iterator, std::remove_cvref_t<First>>)
    _concatenating_iterator(First&& first, Args&&... args): Parent(std::forward<First>(first), std::forward<Args>(args)...) {
      if(is_valid()) it = get_container().begin(); // if the given list of containers is not empty, get the first item of the first container
    }


    _concatenating_iterator& operator=(const _concatenating_iterator&) = default;
    _concatenating_iterator& operator=(_concatenating_iterator&&) = default;

    bool operator==(const _concatenating_iterator& other) const {
      return is_valid() ? (it == other.it) : !other.is_valid();
    }

    // NOTE: do not attempt to call ++ on the end-iterator lest you see segfaults
    _concatenating_iterator& operator++() {
      ++it;
      if(it == get_container().end()) {
        do{
          Parent::operator++();
        } while(is_valid() && get_container().empty());
        if(is_valid()) it = get_container().begin();
      }
      return *this;
    }

    _concatenating_iterator& operator++(int) { _concatenating_iterator result(*this); ++(*this); return result; }

    reference operator*() const { return *it; }
    pointer operator->() const { return &(*it); }
  };

  // if the first template argument is iterable, then derive the iterator type from it
  template<class T,
           class Factory = void,
           class ItemIter = ItemIterFromContainerAndFactory<iterator_of_t<T>, Factory>>
  using concatenating_iterator = _concatenating_iterator<iterator_of_t<T>, Factory, ItemIter>;

  // factories
  template<class T,
           class Factory = void,
           class ItemIter = ItemIterFromContainerAndFactory<iterator_of_t<T>, Factory>,
           class BeginEndTransformation = void>
  using ConcatenatingIterFactory = IterFactory<concatenating_iterator<T, Factory, ItemIter>, BeginEndTransformation>;
}
