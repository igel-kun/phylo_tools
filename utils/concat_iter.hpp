
#pragma once

#include "stl_utils.hpp"
#include "iter_factory.hpp"

namespace mstd {
  // this is an iterator over multiple iterable objects, passing over each of them in turn, effectively concatenating them
  // NOTE: all iterable objects must admit the same iterator type 'ItemIter'
  // NOTE: if your container does not contain iteratables, you can use the last template argument to pass an iterator factory
  //       that will build an iterable from the value_type of your container
  template<class ContainerIter, class ItemIter = iterator_of_t<value_type_of_t<ContainerIter>>>
  class _concatenating_iterator: public MakeVerifyable<ContainerIter>
  {
    using Parent = MakeVerifyable<ContainerIter>;
    using Container = value_type_of_t<ContainerIter>;
    ItemIter it;

    decltype(auto) get_container() { return Parent::operator*(); }
    decltype(auto) get_container() const { return Parent::operator*(); }

    bool end_of_current_container() const {
      if constexpr (VerifyableIter<ItemIter>)
        return not it.is_valid();
      else return (it == get_container().end());
    }

    void fix_container_iter() {
      while(end_of_current_container()) {
        DEBUG5(std::cout << "concat-iter fixing container-iter...\n");
        Parent::operator++();
        if(not is_valid()) return;
        it = get_container().begin();
      }
    }

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
    _concatenating_iterator(_concatenating_iterator&& other) = default;
    _concatenating_iterator(const _concatenating_iterator& other) = default;

    _concatenating_iterator& operator=(_concatenating_iterator&& other) = default;
    _concatenating_iterator& operator=(const _concatenating_iterator& other) =default;

    // construct the container auto_iter from anything (could be a container of containers or a compatible auto_iter or 2 ContainerIter, etc)
    template<class First, class... Args> requires (not mstd::is_same_v<First, _concatenating_iterator>)
    _concatenating_iterator(First&& first, Args&&... args):
      Parent(std::forward<First>(first), std::forward<Args>(args)...)
    {
      if(is_valid()) {
        it = get_container().begin(); // if the given list of containers is not empty, get the first item of the first container
        fix_container_iter();
      }
    }

    bool operator==(const _concatenating_iterator& other) const {
      return is_valid() ? (it == other.it) : !other.is_valid();
    }

    // NOTE: do not attempt to call ++ on the end-iterator lest you see segfaults
    auto& operator++() {
      ++it;
      fix_container_iter();
      return *this;
    }

    auto& operator++(int) { _concatenating_iterator result(*this); ++(*this); return result; }

    reference operator*() const { return *it; }
    pointer operator->() const {
      if constexpr (std::is_reference_v<reference>) {
        return &(*it);
      } else return *it;
    }

    const auto& get_item_iter() const { return it; }
  };

  // if the first template argument is iterable, then derive the iterator type from it
  template<class T, class ItemIter = iterator_of_t<value_type_of_t<T>>>
  using concatenating_iterator = _concatenating_iterator<iterator_of_t<T>, ItemIter>;

  // factories
  template<class T, class ItemIter = iterator_of_t<value_type_of_t<T>>, class BeginEndTransformation = void>
  using ConcatenatingIterFactory = IterFactory<concatenating_iterator<T, ItemIter>, BeginEndTransformation>;

  template<IterableType Container>
  auto get_concatenating(Container&& c) { return ConcatenatingIterFactory<Container>(std::forward<Container>(c)); }

  template<IterableType Container, class Trans, bool pass_iterator = false>
  auto get_concatenating(Container&& c, Trans&& trans) {
    using TransFactory = TransformingIterFactory<iterator_of_t<Container>, Trans, pass_iterator>;
    //using Factory = ConcatenatingIterFactory<TransFactory>;
    //return Factory(std::forward<Container>(c), std::forward<Trans>(trans));
    return get_concatenating(TransFactory(std::forward<Container>(c), std::forward<Trans>(trans)));
  }

}
