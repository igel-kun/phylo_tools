
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "auto_iter.hpp"
#include "iter_factory.hpp"

namespace std{

  struct do_not_fix_index_tag {};
  constexpr do_not_fix_index_tag do_not_fix_index;

  // skip all items in a container for which the predicate is true
  // NOTE: while we could just always use lambda functions as predicate, the current way is more flexible
  //       since it allows default initializing the filtered_iterator if our Predicate is static
  template<class Container, class NormalIterator>
  class _filtered_iterator: public std::my_iterator_traits<NormalIterator>
  {
    using Parent = std::my_iterator_traits<NormalIterator>;
  protected:
    auto_iter<NormalIterator> it;

  public:
    using IteratorData = NormalIterator; // the IterFactory expects us to tell it what we want for construction besides begin/end: we want a NormalIterator
    using typename Parent::value_type;
    using typename Parent::reference;
    using typename Parent::pointer;

    _filtered_iterator(const NormalIterator& _it, const NormalIterator& _first_invalid): it(_it, _first_invalid) {}
    _filtered_iterator(const auto_iter<NormalIterator>& _it): it(_it) {}
 
    // copy construction, also construct const_iterators from iterators
    template<class _Container, class _NormalIterator>
    _filtered_iterator(const _filtered_iterator<_Container, _NormalIterator>& other):
      _filtered_iterator(other.it)
    {}

    operator NormalIterator() const { return it; }

    template<class _Container, class _NormalIterator>
    inline bool operator==(const _filtered_iterator<_Container, _NormalIterator>& other) const { return it == other.it; }
    inline bool operator==(const NormalIterator& j) const { return it.i == j; }

    template<class _Container, class _NormalIterator>
    bool operator!=(const _filtered_iterator<_Container, _NormalIterator>& other) const { return !operator==(other); }
    bool operator!=(const NormalIterator& j) const { return !operator==(j); }

    reference operator*() const { return *it; }
    pointer operator->() const { return *it; }

    template<class, class>
    friend class _filtered_iterator;
  };

  template<class Container,
           class Predicate,
           bool reverse = false,
           class NormalIterator = conditional_t<reverse, reverse_iterator_of_t<Container>, iterator_of_t<Container>>,
           class Category = typename NormalIterator::iterator_category>
  class filtered_iterator: public _filtered_iterator<Container, NormalIterator>
  {
    using Parent = _filtered_iterator<Container, NormalIterator>;
  protected:
    using Parent::it;

    Predicate pred;

    inline void fix_index() { while(!it.is_invalid() && pred.value(it)) ++it; }
  public:
    
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

    // make a filtered iterator from a start iterator and an Iteratordata, that is { end iterator, arguments to build the predicate }
    template<class _IteratorData, class = enable_if<is_same_v<remove_cvref_t<_IteratorData>, IteratorData>>>
    filtered_iterator(const NormalIterator& _i, _IteratorData&& data):
      filtered_iterator(_i, data.first_invalid, move(data.pred))
    {}

    // make a filtered iterator from a start iterator and an Iteratordata, that is { end iterator, arguments to build the predicate }
    template<class _IteratorData, class = enable_if<is_same_v<remove_cvref_t<_IteratorData>, IteratorData>>>
    filtered_iterator(const do_not_fix_index_tag, const NormalIterator& _i, _IteratorData&& data):
      filtered_iterator(do_not_fix_index, _i, data.first_invalid, move(data.pred))
    {}

    // copy construction, also construct const_iterators from iterators (same Category required though)
    template<class _Container, class _Predicate, class _NormalIterator>
    filtered_iterator(const filtered_iterator<_Container, _Predicate, reverse, _NormalIterator, Category>& other):
      Parent(other), pred(other.pred) {}

    filtered_iterator& operator++()    { if(!it.is_invalid()) {++it; fix_index();} return *this; }
    filtered_iterator  operator++(int) { _filtered_iterator result(*this); ++(*this); return result; }

    template<class, class, bool, class, class>
    friend class filtered_iterator;
  };

  // make a specialization for bidirectional iterators which also provides operator--
  template<class Container,
           class Predicate,
           bool reverse,
           class NormalIterator>
  class filtered_iterator<Container, Predicate, reverse, NormalIterator, bidirectional_iterator_tag>:
    public filtered_iterator<Container, Predicate, reverse, NormalIterator, forward_iterator_tag> //NOTE: be explicit about the tag to avoid circular deps
  {
    using Parent = filtered_iterator<Container, Predicate, reverse, NormalIterator, forward_iterator_tag>;
  protected:
    using Parent::pred;
    using Parent::it;

    inline void rev_fix_index() { while(!it.is_invalid() && pred.value(it)) --it; }
  public:
    using Parent::Parent;

    filtered_iterator& operator--()    { if(!it.is_invalid()) {--it; rev_fix_index();} return *this; }
    filtered_iterator  operator--(int) { _filtered_iterator result(*this); --(*this); return result; }

    template<class, class, bool, class, class>
    friend class filtered_iterator;
  };

  // std::BeginEndIters wants an iterator that can be instanciated with only a container, so here we go...
  template<class Predicate, bool reverse>
  struct FilteredIterFor{ template<class Container> using type = std::filtered_iterator<Container, Predicate, reverse>; };

  // We want a specialization for static predicates in order to save the instanciation...
  /* while the check for static predicates doesn't work, this should be commented out
  template<class Container,
           class Predicate,
           bool reverse,
           class NormalIterator,
           class BeginEnd>
  class filtered_iterator<Container, Predicate, reverse, NormalIterator, BeginEnd, void_t<std::enable_if_t<is_static_predicate_v<Predicate>>>>{
  };
  */

  // as a factory, use an IterFactory with the predicate and end-iterator as factory data
  template<class Container, class Predicate, bool reverse = false>
  using FilteredIterFactory = IterFactory<Container,
            const typename filtered_iterator<Container, Predicate, reverse>::IteratorData,
            BeginEndIters<Container, reverse, FilteredIterFor<Predicate, reverse>::template type>>;


} // namespace std

