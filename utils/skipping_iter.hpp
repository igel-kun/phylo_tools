
#pragma once

#include <memory>
#include "stl_utils.hpp"

namespace std{

  // skip all items in a container for which the predicate is true
  template<class Container,
           class Predicate,
           bool reverse = false,
           class NormalIterator = std::IteratorOf_t<Container, reverse>,
           class BeginEnd = BeginEndIters<Container, reverse>>
  class skipping_iterator
  {
  protected:
    Container& c;
    NormalIterator i;
    Predicate pred;

    inline void increment_index() { ++i; fix_index(); }
    inline void decrement_index() { --i; fix_index_rev(); }
    inline void fix_index() { while(!is_end() && pred.value(i)) ++i; }
    inline void fix_index_rev() { while(!is_past_begin() && pred.value(i)) --i; } 
    inline bool is_end() const { return i == BeginEnd::end(c); }
    inline bool is_past_begin() const { return i == std::prev(BeginEnd::begin(c)); }
  public:
    using value_type      = typename std::iterator_traits<NormalIterator>::value_type;
    using reference       = typename std::iterator_traits<NormalIterator>::reference;
    using pointer         = typename std::iterator_traits<NormalIterator>::pointer;
    using difference_type = typename std::iterator_traits<NormalIterator>::difference_type;
	  using iterator_category = std::bidirectional_iterator_tag;

    template<class... Args>
    skipping_iterator(Container& _c, const NormalIterator& _i, const bool _fix_index, Args... args):
      c(_c), i(_i), pred(args...)
    { if(_fix_index) fix_index(); }
 
    // copy construction, also construct const_iterators from iterators
    template<class _Container, class _Predicate, bool _reverse>
    skipping_iterator(const skipping_iterator<_Container, _Predicate, _reverse>& other):
      c(other.c),
      i((reverse == _reverse) ? other.i : std::prev(other.i)),
      pred(other.pred)
    {}

    skipping_iterator& operator++() { if(!is_end()) increment_index(); return *this; }
    skipping_iterator& operator++(int) { skipping_iterator result(*this); ++(*this); return result; }
    skipping_iterator& operator--() { if(!is_past_begin()) decrement_index(); return *this; }
    skipping_iterator& operator--(int) { skipping_iterator result(*this); --(*this); return result; }

    operator NormalIterator() const { return i; }

    template<class _Container, class _Predicate, bool _reverse>
    inline bool operator==(const skipping_iterator<_Container, _Predicate, _reverse>& it) const
    {
      assert(&c == &(it.c));
      return i == ((reverse == _reverse) ? it.i : std::prev(it.i));
    }
    inline bool operator==(const NormalIterator& j) const { return i == j; }

    template<class _Container, class _Predicate, bool _reverse>
    bool operator!=(const skipping_iterator<_Container, _Predicate, _reverse>& it) const { return !operator==(it); }
    bool operator!=(const NormalIterator& j) const { return !operator==(j); }

    reference operator*() const { return *i; }
    pointer operator->() const { return *i; }
 
    template<class _Container, class _Predicate, bool _reverse, class _NormalIterator, class _BeginEnd>
    friend class skipping_iterator;
  };


  template<class Container,
           class Predicate,
           bool reverse = false>
  class SkippingIterFactory
  {
    std::shared_ptr<Container> c;
    Predicate p;
  public:
    using iterator = skipping_iterator<Container, Predicate, reverse>;
    using const_iterator = skipping_iterator<const Container, Predicate, reverse>;
    using BeginEnd = BeginEndIters<Container, reverse>;
    using value_type  = typename Container::value_type;
    using reference   = typename Container::reference;

    template<class... Args>
    SkippingIterFactory(const std::shared_ptr<Container>& _c, Args... args): c(_c), p(args...) {}
    template<class... Args>
    SkippingIterFactory(Container* _c, Args... args): c(_c), p(args...) {}

    iterator begin() { return {*c, BeginEnd::begin(*c), true, p}; }
    iterator end() { return {*c, BeginEnd::end(*c), false, p}; }
    const_iterator begin() const { return {*c, BeginEnd::begin(*c), true, p}; }
    const_iterator end() const { return {*c, BeginEnd::end(*c), false, p}; }
  };

  template<class Container, class Predicate, bool reverse = false>
  SkippingIterFactory<Container, Predicate, reverse> skipping(const std::shared_ptr<Container>& c, Predicate& p) { return {c, p}; }
  template<class Container, class Predicate, bool reverse = false>
  SkippingIterFactory<Container, Predicate, reverse> skipping(Container* c, Predicate& p) { return {c, p}; }

} // namespace std

