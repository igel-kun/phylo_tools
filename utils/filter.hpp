
#pragma once

#include <memory>
#include "stl_utils.hpp"

namespace std{

  // skip all items in a container for which the predicate is true
  // NOTE: while we could just always use lambda functions as predicate, the current way is more flexible
  //       since it allows default initializing the filtered_iterator if our Predicate is static
  template<class Container,
           bool reverse = false,
           class NormalIterator = std::conditional_t<reverse, std::reverse_iterator_of_t<Container>, std::iterator_of_t<Container>>,
           class BeginEnd = BeginEndIters<Container, reverse>>
  class _filtered_iterator
  {
  protected:
    Container& c;
    NormalIterator i;

    inline bool is_end() const { return i == BeginEnd::end(c); }
    inline bool is_past_begin() const { return i == std::prev(BeginEnd::begin(c)); }

  public:
    using value_type      = typename std::my_iterator_traits<NormalIterator>::value_type;
    using reference       = typename std::my_iterator_traits<NormalIterator>::reference;
    using pointer         = typename std::my_iterator_traits<NormalIterator>::pointer;
    using difference_type = typename std::my_iterator_traits<NormalIterator>::difference_type;
	  using iterator_category = std::bidirectional_iterator_tag;

    _filtered_iterator(Container& _c, const NormalIterator& _i): c(_c), i(_i) {}
 
    operator NormalIterator() const { return i; }

    template<class _Container, bool _reverse>
    inline bool operator==(const _filtered_iterator<_Container, _reverse>& it) const
    {
      assert(&c == &(it.c));
      return i == ((reverse == _reverse) ? it.i : std::prev(it.i));
    }
    inline bool operator==(const NormalIterator& j) const { return i == j; }

    template<class _Container, bool _reverse>
    bool operator!=(const _filtered_iterator<_Container, _reverse>& it) const { return !operator==(it); }
    bool operator!=(const NormalIterator& j) const { return !operator==(j); }

    reference operator*() const { return *i; }
    pointer operator->() const { return *i; }

    template<class, bool, class, class>
    friend class _filtered_iterator;
  };

  template<class Container,
           class Predicate,
           bool reverse = false,
           class NormalIterator = std::conditional_t<reverse, std::reverse_iterator_of_t<Container>, std::iterator_of_t<Container>>,
           class BeginEnd = BeginEndIters<Container, reverse>,
           class = void>
  class filtered_iterator: public _filtered_iterator<Container, reverse, NormalIterator, BeginEnd>{
    using Parent = _filtered_iterator<Container, reverse, NormalIterator, BeginEnd>;
  protected:
    using Parent::is_end;
    using Parent::is_past_begin;
    using Parent::i;

    Predicate pred;
    
    inline void fix_index() { while(!is_end() && pred.value(i)) ++i; }
    inline void fix_index_rev() { while(!is_past_begin() && pred.value(i)) --i; } 
  public:
    
    template<class... Args>
    filtered_iterator(Container& _c, const NormalIterator& _i, const bool _fix_index, Args&&... args):
      Parent(_c, _i),
      pred(forward<Args>(args)...)
    { if(_fix_index) fix_index(); }
 
    // copy construction, also construct const_iterators from iterators
    template<class _Container, class _Predicate, bool _reverse>
    filtered_iterator(const filtered_iterator<_Container, _Predicate, _reverse>& other):
      Parent(other.c, ((reverse == _reverse) ? other.i : std::prev(other.i))),
      pred(other.pred)
    {}

    filtered_iterator& operator++()    { if(!is_end()) {++i; fix_index();} return *this; }
    filtered_iterator  operator++(int) { _filtered_iterator result(*this); ++(*this); return result; }
    filtered_iterator& operator--()    { if(!is_past_begin()) {--i; fix_index_rev();} return *this; }
    filtered_iterator  operator--(int) { _filtered_iterator result(*this); --(*this); return result; }

    template<class, class, bool, class, class, class>
    friend class filtered_iterator;
  };

  /* while the check for static predicates doesn't work, this should be commented out
  template<class Container,
           class Predicate,
           bool reverse,
           class NormalIterator,
           class BeginEnd>
  class filtered_iterator<Container, Predicate, reverse, NormalIterator, BeginEnd, void_t<std::enable_if_t<is_static_predicate_v<Predicate>>>>{
  };
  */

  template<class Container,
           class Predicate,
           bool reverse = false>
  class FilteredIterFactory
  {
    std::shared_ptr<Container> c;
    Predicate p;
  public:
    using iterator = filtered_iterator<Container, Predicate, reverse>;
    using const_iterator = filtered_iterator<const Container, Predicate, reverse>;
    using BeginEnd = BeginEndIters<Container, reverse>;
    using value_type  = typename Container::value_type;
    using reference   = typename Container::reference;

    template<class... Args>
    FilteredIterFactory(Container& _c, Args&&... args): c(&_c, std::NoDeleter()), p(forward<Args>(args)...) {}
    template<class... Args>
    FilteredIterFactory(Container* const _c, Args&&... args): c(_c), p(forward<Args>(args)...) {}
    template<class... Args>
    FilteredIterFactory(Container&& _c, Args&&... args): c(new Container(std::forward<Container>(_c))), p(forward<Args>(args)...) {}

    iterator begin() { return {*c, BeginEnd::begin(*c), true, p}; }
    iterator end() { return {*c, BeginEnd::end(*c), false, p}; }
    const_iterator begin() const { return {*c, BeginEnd::begin(*c), true, p}; }
    const_iterator end() const { return {*c, BeginEnd::end(*c), false, p}; }
  };

  template<class Container, class Predicate, bool reverse = false>
  FilteredIterFactory<Container, Predicate, reverse> filtered_iter(Container* c, const bool del_on_exit, Predicate& p) { return {c, del_on_exit, p}; }

} // namespace std

