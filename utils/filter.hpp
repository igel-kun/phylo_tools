
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "auto_iter.hpp"
#include "iter_factory.hpp"

namespace std{

  // construct a _filtered_iterator with this tag in order to avoid the initial "fix" (skipping of invalid entries)
  struct do_not_fix_index_tag {};

  // skip all items in a container for which the predicate is false (that is, list all items for which the predicate is true)
  // NOTE: while we could just always use lambda functions as predicate, the current way is more flexible
  //       since it allows default initializing the _filtered_iterator if our Predicate is static
  // NOTE: _filtered_iterators cannot be replaced by C++20's filtered_view because one has to derive a filtered_view object from the container and one can then iterate this filtered view object
  //       while, here, we want the iterator to do the filtering!
  template<class NormalIterator, class Predicate = std::function<bool(const value_type_of_t<NormalIterator>&)>> // std::function is slower than lambdas
  class _filtered_iterator: public auto_iter<NormalIterator>
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
    // a filtered iterator cannot be random access, so random_access iterators will become bidirectional iterators instead
    using iterator_category = conditional_t<is_same_v<typename Parent::iterator_category, random_access_iterator_tag>,
                                            bidirectional_iterator_tag,
                                            typename Parent::iterator_category>;

    // make a filtered iterator
    //NOTE: this will always fix the index (doing nothing if _i == _first_invalid),
    //      if you're sure this isn't necessary, call with do_not_fix_index as first argument (see below)
    template<class _Parent, class _Pred = Predicate>
      requires (is_constructible_v<Parent, _Parent> && is_constructible_v<Predicate, _Pred>)
    _filtered_iterator(_Parent&& parent_init, _Pred&& pred_init = _Pred()):
      Parent(forward<_Parent>(parent_init)),
      pred(forward<_Pred>(pred_init))
    { fix_index(); }

    template<class _Parent, class _Pred = Predicate>
      requires (is_constructible_v<Parent, _Parent> && is_constructible_v<Predicate, _Pred>)
    _filtered_iterator(const do_not_fix_index_tag, _Parent&& parent_init, _Pred&& pred_init = _Pred()):
      Parent(forward<_Parent>(parent_init)),
      pred(forward<_Pred>(pred_init))
    {}

    // piecewise construction of the auto_iter and the predicate
    template<class ParentTuple, class PredicateTuple>
    constexpr _filtered_iterator(const piecewise_construct_t, ParentTuple&& parent_init, PredicateTuple&& pred_init):
      Parent(forward<ParentTuple>(parent_init)),
      pred(make_from_tuple<Predicate>(forward<PredicateTuple>(pred_init)))
    { fix_index(); }

    template<class ParentTuple, class PredicateTuple>
    constexpr _filtered_iterator(const do_not_fix_index_tag, const piecewise_construct_t, ParentTuple&& parent_init, PredicateTuple&& pred_init):
      Parent(forward<ParentTuple>(parent_init)),
      pred(make_from_tuple<Predicate>(forward<PredicateTuple>(pred_init)))
    {}


    constexpr _filtered_iterator() = default;
    _filtered_iterator(const _filtered_iterator& iter) = default;
    _filtered_iterator(_filtered_iterator&& iter) = default;
    _filtered_iterator& operator=(const _filtered_iterator& iter) = default;
    _filtered_iterator& operator=(_filtered_iterator&& iter) = default;

    _filtered_iterator& operator++()    { if(is_valid()) {Parent::operator++(); fix_index();} return *this; }
    _filtered_iterator  operator++(int) { _filtered_iterator result(*this); ++(*this); return result; }
    _filtered_iterator& operator--()    { if(is_valid()) {Parent::operator--(); fix_index<true>();} return *this; }
    _filtered_iterator  operator--(int) { _filtered_iterator result(*this); --(*this); return result; }
  };

  // if the first template argument is iterable, then take the Iterator type from that
  template<class Iterator, class Predicate>
  struct __filtered_iterator { using type = _filtered_iterator<Iterator, Predicate>; };
  template<IterableType Container, class Predicate>
  struct __filtered_iterator<Container, Predicate> { using type = _filtered_iterator<iterator_of_t<Container>, Predicate>; };

  template<class T, class Predicate = std::function<bool(const value_type_of_t<T>&)>>
  using filtered_iterator = typename __filtered_iterator<T, Predicate>::type;

  template<class T, class Predicate>
  auto make_filtered_iterator(T&& iter, Predicate&& pred) { return filtered_iterator<T,Predicate>(forward<T>(iter), forward<Predicate>(pred)); }



  template<class T,
           class Predicate = std::function<bool(const value_type_of_t<T>&)>,
           class IteratorTransformation = void>
  using FilteredIterFactory = IterFactory<filtered_iterator<T, Predicate>, IteratorTransformation>;

  template<class T, class Predicate, class IteratorTransformation>
  auto make_filtered_factory(T&& _iter, Predicate&& _pred, IteratorTransformation&& trans) {
    return  FilteredIterFactory<T, Predicate, IteratorTransformation>(piecewise_construct,
                                                                      forward_as_tuple(forward<T>(_iter),
                                                                      forward<Predicate>(_pred)),
                                                                      forward_as_tuple(trans));
  }
  template<class T, class Predicate>
  auto make_filtered_factory(T&& _iter, Predicate&& _pred) {
    return FilteredIterFactory<T, Predicate, void>(forward<T>(_iter), forward<Predicate>(_pred));
  }


} // namespace std

