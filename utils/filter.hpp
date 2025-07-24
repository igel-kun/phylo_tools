
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "auto_iter.hpp"
#include "iter_factory.hpp"

namespace mstd {

  // construct a _filtered_iterator with this tag in order to avoid the initial "fix" (skipping of invalid entries)
  struct do_not_fix_index_tag {};
  // construct only the filter of a filtered_iterator, but not the underlying iterator (use operator= later to assign the iterator)
  struct filter_only_tag {};

  // skip all items in a container for which the predicate is false (that is, list all items for which the predicate is true)
  // NOTE: when pass_iterator is true, then the predicate will get the iterator as argument, instead of the value that the iter points to
  // NOTE: while we could just always use lambda functions as predicate, the current way is more flexible
  //       since it allows default initializing the _filtered_iterator if our Predicate is static
  // NOTE: _filtered_iterators cannot be replaced by C++20's filtered_view because
  //       one has to derive a filtered_view object from the container and one can then iterate this filtered view object
  //       while, here, we want the iterator to do the filtering!
  // NOTE: the above doesn't seem to make sense, please re-evaluate
  template<HasIterCategory NormalIterator, class _Predicate, bool pass_iterator = false>
  class _filtered_iterator: public MakeVerifyable<NormalIterator> {
    using Parent = MakeVerifyable<NormalIterator>;
    [[no_unique_address]] _Predicate pred;

    bool apply_pred() const { if constexpr (pass_iterator) return access(pred)(*this); else return access(pred)(**this); }
    bool apply_pred() { if constexpr (pass_iterator) return access(pred)(*this); else return access(pred)(**this); }

    template<bool rev = false>
    void fix_index() {
      DEBUG6(std::cout << "fixing index...\n");
      while(is_valid() && !apply_pred())
        if constexpr (rev) Parent::operator--(); else Parent::operator++();
      DEBUG6(if(is_valid()) {
               std::cout << "after fixing, we're at "<<**this<<"\n";
             } else std::cout << "after fixing, we're invalid\n";);
    }
  public:
    using Predicate = _Predicate;
    using Iterator = Parent;
    using typename Parent::value_type;
    using typename Parent::reference;
    using typename Parent::pointer;
    using Parent::is_valid;
    // a filtered iterator cannot be random access, so random_access iterators will become std::bidirectional iterators instead
    using iterator_category = std::conditional_t<std::is_same_v<typename Parent::iterator_category, std::random_access_iterator_tag>,
                                            std::bidirectional_iterator_tag,
                                            typename Parent::iterator_category>;

    // make a filtered iterator
    //NOTE: this will always fix the index (doing nothing if _i == _first_invalid),
    //      if you're sure this isn't necessary, call with do_not_fix_index as first argument (see below)
    template<class ParentInit, class PredInit = Predicate>
      requires (not mstd::is_any_of<ParentInit, _filtered_iterator, std::piecewise_construct_t, do_not_fix_index_tag, filter_only_tag>)
    _filtered_iterator(ParentInit&& parent_init, PredInit&& pred_init = PredInit()):
      Parent(std::forward<ParentInit>(parent_init)),
      pred{std::forward<PredInit>(pred_init)}
    {
      DEBUG6(std::cout << "____________ created filtered-iter with parent "<<type_name<Parent>()<<"\n");
      fix_index();
    }

    template<class ParentInit, class PredInit = Predicate>
      requires (not mstd::is_same_v<ParentInit, std::piecewise_construct_t>)
    _filtered_iterator(const do_not_fix_index_tag, ParentInit&& parent_init, PredInit&& pred_init = PredInit()):
      Parent(std::forward<ParentInit>(parent_init)),
      pred{std::forward<PredInit>(pred_init)}
    {}

    template<class PredInit = Predicate>
    _filtered_iterator(const filter_only_tag, PredInit&& pred_init = PredInit()):
      Parent{},
      pred{std::forward<PredInit>(pred_init)}
    {}

    // piecewise construction of the auto_iter and the predicate
    template<class ParentTuple, class PredicateTuple = std::tuple<>>
    constexpr _filtered_iterator(const std::piecewise_construct_t, ParentTuple&& parent_init, PredicateTuple&& pred_init = PredicateTuple()):
      Parent(std::make_from_tuple<Parent>(std::forward<ParentTuple>(parent_init))),
      pred{std::make_from_tuple<Predicate>(std::forward<PredicateTuple>(pred_init))}
    { fix_index(); }

    template<class ParentTuple, class PredicateTuple = std::tuple<>>
    constexpr _filtered_iterator(const do_not_fix_index_tag,
                                 const std::piecewise_construct_t,
                                 ParentTuple&& parent_init,
                                 PredicateTuple&& pred_init = PredicateTuple()):
      Parent(std::make_from_tuple<Parent>(std::forward<ParentTuple>(parent_init))),
      pred{std::make_from_tuple<Predicate>(std::forward<PredicateTuple>(pred_init))}
    {}

    _filtered_iterator() = default;

    // copy construct
    _filtered_iterator(const _filtered_iterator& iter) = default;

    template<class PredInit = Predicate>
    _filtered_iterator(const _filtered_iterator& iter, PredInit&& pred_init):
      Parent{iter}, pred{std::forward<PredInit>(pred_init)} {}
    
    // move construct
    _filtered_iterator(_filtered_iterator&& iter) = default;
    template<class PredInit = Predicate>
    _filtered_iterator(_filtered_iterator&& iter, PredInit&& pred_init):
      Parent{std::move(iter)}, pred{std::forward<PredInit>(pred_init)} {}


    _filtered_iterator& operator=(const _filtered_iterator& iter) = default;
    _filtered_iterator& operator=(_filtered_iterator&& iter) = default;

    // enable operator= to work with the Parent, leaving the predicate as it is
    auto& operator=(const Parent& iter) { static_cast<Parent&>(*this) = iter; return *this; }
    auto& operator=(Parent&& iter) { static_cast<Parent&>(*this) = std::move(iter); return *this; }

    auto& operator++()    { if(is_valid()) {Parent::operator++(); fix_index();} return *this; }
    auto& operator--()    { if(is_valid()) {Parent::operator--(); fix_index<true>();} return *this; }
    auto  operator++(int) { _filtered_iterator result(*this); ++(*this); return result; }
    auto  operator--(int) { _filtered_iterator result(*this); --(*this); return result; }

    Predicate& get_predicate() { return pred; }
    const Predicate& get_predicate() const { return pred; }
  };

  // if the first template argument is iterable, then take the Iterator type from that
  template<class T, class Predicate, bool pass_iterator = false>
  using filtered_iterator = _filtered_iterator<iterator_of_t<T>, Predicate, pass_iterator>;

  template<bool pass_iterator = false, class T, class Predicate>
  auto make_filtered_iterator(T&& iter, Predicate&& pred) { return filtered_iterator<T,Predicate>(std::forward<T>(iter), std::forward<Predicate>(pred)); }

  template<class T,
           class Predicate = std::function<bool(const value_type_of_t<T>&)>,
           bool pass_iterator = false,
           class IteratorTransformation = void>
  using FilteredIterFactory = IterFactory<filtered_iterator<T, Predicate, pass_iterator>, IteratorTransformation>;

  template<bool pass_iterator = false, class Iter, class Predicate, class IteratorTransformation>
  auto make_filtered_factory(Iter&& _iter, Predicate&& _pred, IteratorTransformation&& trans) {
    return  FilteredIterFactory<Iter, Predicate, pass_iterator, IteratorTransformation>(std::piecewise_construct,
                                                                      forward_as_tuple(trans),
                                                                      forward_as_tuple(std::forward<Iter>(_iter), std::forward<Predicate>(_pred)));
  }
  template<bool pass_iterator = false, class Iter, class Predicate>
  auto make_filtered_factory(Iter&& _iter, Predicate&& _pred) {
    return FilteredIterFactory<Iter, Predicate, pass_iterator, void>(std::forward<Iter>(_iter), std::forward<Predicate>(_pred));
  }


}

