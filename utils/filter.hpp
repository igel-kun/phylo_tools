
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "auto_iter.hpp"
#include "iter_factory.hpp"

namespace std{

  // construct a filtered_iterator with this tag in order to avoid the initial "fix" (skipping of invalid entries)
  struct do_not_fix_index_tag {};

  // skip all items in a container for which the predicate is false (that is, list all items for which the predicate is true)
  // NOTE: while we could just always use lambda functions as predicate, the current way is more flexible
  //       since it allows default initializing the filtered_iterator if our Predicate is static
  // NOTE: filtered_iterators cannot be replaced by C++20's filtered_view because one has to derive a filtered_view object from the container and one can then iterate this filtered view object
  //       while, here, we want the iterator to do the filtering!

  template<IterableType Container,
           class Predicate = std::function<bool(const typename Container::value_type&)>,
           bool reverse = false,
           class NormalIterator = conditional_t<reverse, reverse_iterator_of_t<Container>, iterator_of_t<Container>>,
           class Category = typename NormalIterator::iterator_category>
  class filtered_iterator: public auto_iter<NormalIterator>
  {
    using Parent = auto_iter<NormalIterator>;
    Predicate pred;

    template<bool rev = false>
    void fix_index() {
      while(is_valid() && !pred(**this))
        if constexpr (rev) Parent::operator--(); else Parent::operator++();
    }
  public:
    using typename Parent::value_type;
    using typename Parent::reference;
    using typename Parent::pointer;
    using Parent::is_valid;

    // the IterFactory expects us to tell it what we want for construction besides begin/end: we want a NormalIterator (the end) and a predicate
    struct IteratorData {
      NormalIterator first_invalid;
      Predicate pred;

      template<class... Args>
      IteratorData(const NormalIterator& fi, Args&&... args): first_invalid(fi), pred(forward<Args>(args)...) {}
    };

    // make a filtered iterator from a start, an end, and arguments to build the predicate
    //NOTE: this will always fix the index (doing nothing if _i == _first_invalid),
    //      if you're sure this isn't necessary, call with do_not_fix_index as first argument (see below)
    template<class... Args>
    filtered_iterator(const NormalIterator& _i, const NormalIterator& _first_invalid, Args&&... args):
      Parent(_i, _first_invalid),
      pred(forward<Args>(args)...)
    { fix_index(); }

    template<class... Args>
    filtered_iterator(const do_not_fix_index_tag, const NormalIterator& _i, const NormalIterator& _first_invalid, Args&&... args):
      Parent(_i, _first_invalid),
      pred(forward<Args>(args)...)
    {}

    // make a filtered iterator from a start iterator and an Iteratordata, that is { end iterator, predicate }
    template<class _IteratorData> requires is_convertible_v<_IteratorData, IteratorData>
    filtered_iterator(const NormalIterator& _i, _IteratorData&& data):
      filtered_iterator(_i, forward<_IteratorData>(data).first_invalid, forward<_IteratorData>(data).pred)
    {}

    // make a filtered iterator from a start iterator and an Iteratordata, that is { end iterator, predicate }
    template<class _IteratorData> requires is_convertible_v<_IteratorData, IteratorData>
    filtered_iterator(const do_not_fix_index_tag, const NormalIterator& _i, _IteratorData&& data):
      filtered_iterator(do_not_fix_index_tag(), _i, forward<_IteratorData>(data).first_invalid, forward<_IteratorData>(data).pred)
    {}

    filtered_iterator(const filtered_iterator& iter) = default;
    filtered_iterator(filtered_iterator&& iter) = default;
    filtered_iterator& operator=(const filtered_iterator& iter) = default;
    filtered_iterator& operator=(filtered_iterator&& iter) = default;

    filtered_iterator& operator++()    { if(is_valid()) {Parent::operator++(); fix_index();} return *this; }
    filtered_iterator  operator++(int) { filtered_iterator result(*this); ++(*this); return result; }
    filtered_iterator& operator--()    { if(is_valid()) {Parent::operator--(); fix_index<true>();} return *this; }
    filtered_iterator  operator--(int) { filtered_iterator result(*this); --(*this); return result; }

    template<IterableType, class, bool, class, class>
    friend class filtered_iterator;
  };

  template<IterableType Container,
           class Predicate,
           bool reverse,
           class NormalIterator>
  class filtered_iterator<Container, Predicate, reverse, NormalIterator, random_access_iterator_tag>:
    public filtered_iterator<Container, Predicate, reverse, NormalIterator, bidirectional_iterator_tag>
  { using filtered_iterator<Container, Predicate, reverse, NormalIterator, bidirectional_iterator_tag>::filtered_iterator; };

  // std::BeginEndIters wants an iterator that can be instanciated with only a container, so here we go...
  template<class Predicate, bool reverse>
  struct FilteredIterFor{ template<class Container> using type = std::filtered_iterator<Container, Predicate, reverse>; };

  // as a factory, use an IterFactory with the predicate and end-iterator as factory data
  template<IterableType Container,
           class Predicate = std::function<bool(const typename Container::value_type&)>,
           bool reverse = false>
  using FilteredIterFactory = IterFactory<Container,
            const typename filtered_iterator<Container, Predicate, reverse>::IteratorData,
            BeginEndIters<Container, reverse, FilteredIterFor<Predicate, reverse>::template type>>;

} // namespace std

