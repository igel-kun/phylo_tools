
#pragma once

#include <memory>
#include "stl_utils.hpp"

namespace std{

  // skip all items in a container for which the predicate is true
  // NOTE: while we could just always use lambda functions as predicate, the current way is more flexible
  //       since it allows default initializing the filtered_iterator if our Predicate is static
  template<class Container, class NormalIterator>
  class _filtered_iterator: public std::my_iterator_traits<NormalIterator>
  {
    using Parent = std::my_iterator_traits<NormalIterator>;
  protected:
    NormalIterator i;
    NormalIterator first_invalid;

    inline bool is_invalid() const { return i == first_invalid; }

  public:
    using IteratorData = NormalIterator; // the IterFactory expects us to tell it what we want for construction besides begin/end: we want a NormalIterator
    using typename Parent::value_type;
    using typename Parent::reference;
    using typename Parent::pointer;

    _filtered_iterator(const NormalIterator& _i, const NormalIterator& _first_invalid): i(_i), first_invalid(_first_invalid) {}
 
    // copy construction, also construct const_iterators from iterators
    template<class _Container, class _NormalIterator>
    _filtered_iterator(const _filtered_iterator<_Container, _NormalIterator>& other):
      _filtered_iterator(other.i, other.first_invalid)
    {}

    operator NormalIterator() const { return i; }

    template<class _Container, class _NormalIterator>
    inline bool operator==(const _filtered_iterator<_Container, _NormalIterator>& it) const { return i == it.i; }
    inline bool operator==(const NormalIterator& j) const { return i == j; }

    template<class _Container, class _NormalIterator>
    bool operator!=(const _filtered_iterator<_Container, _NormalIterator>& it) const { return !operator==(it); }
    bool operator!=(const NormalIterator& j) const { return !operator==(j); }

    reference operator*() const { return *i; }
    pointer operator->() const { return *i; }

    template<class, class>
    friend class _filtered_iterator;
  };

  template<class Container,
           class Predicate,
           bool reverse,
           class NormalIterator = conditional_t<reverse, reverse_iterator_of_t<Container>, iterator_of_t<Container>>,
           class Category = typename NormalIterator::iterator_category>
  class filtered_iterator: public _filtered_iterator<Container, NormalIterator>
  {
    using Parent = _filtered_iterator<Container, NormalIterator>;
  protected:
    using Parent::is_invalid;
    using Parent::i;

    Predicate pred;

    inline void fix_index() { while(!is_invalid() && pred.value(i)) ++i; }
  public:
    
    struct IteratorData {
      const NormalIterator& first_invalid;
      Predicate pred;
      template<class... Args>
      IteratorData(const NormalIterator& fi, Args&&... args): first_invalid(fi), pred(forward<Args>(args)...) {}
    };
    
    template<class... Args>
    filtered_iterator(const NormalIterator& _i, const NormalIterator& _first_invalid, Args&&... args):
      Parent(_i, _first_invalid),
      pred(forward<Args>(args)...)
    { fix_index(); }
 
    template<class _IteratorData, class = enable_if<is_same_v<remove_cvref_t<_IteratorData>, IteratorData>>>
    filtered_iterator(const NormalIterator& _i, _IteratorData&& data):
      filtered_iterator(_i, data.first_invalid, move(data.pred))
    {}

    // copy construction, also construct const_iterators from iterators (same Category required though)
    template<class _Container, class _Predicate, class _NormalIterator>
    filtered_iterator(const filtered_iterator<_Container, _Predicate, reverse, _NormalIterator, Category>& other):
      Parent(other), pred(other.pred) {}

    filtered_iterator& operator++()    { if(!is_invalid()) {++i; fix_index();} return *this; }
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
    using Parent::i;
    using Parent::is_invalid;

    inline void rev_fix_index() { while(!is_invalid() && pred.value(i)) --i; }
  public:
    using Parent::Parent;

    filtered_iterator& operator--()    { if(!is_invalid()) {--i; rev_fix_index();} return *this; }
    filtered_iterator  operator--(int) { _filtered_iterator result(*this); --(*this); return result; }

    template<class, class, bool, class, class>
    friend class filtered_iterator;
  };

  // std::BeginEndIters wants an iterator that can be instanciated with only a container, so here we go...
  template<class Predicate, bool reverse>
  struct ConstOrNoConst{
    template<class Container>
    using filtered_iter = std::filtered_iterator<Container, Predicate, reverse>;
  };

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

  // as a factory, use an IterFactory with the predicate as local data
  template<class Container,
           class Predicate,
           bool reverse = false>
  using FilteredIterFactory = IterFactory<Container,
                                          typename filtered_iterator<Container, Predicate, reverse>::IteratorData,
                                          BeginEndIters<Container, reverse, ConstOrNoConst<Predicate, reverse>::template filtered_iter>>;

  template<class Container, class Predicate, bool reverse = false>
  FilteredIterFactory<Container, Predicate, reverse> filtered_iter(Container* c, const bool del_on_exit, Predicate& p) { return {c, del_on_exit, p}; }

} // namespace std

