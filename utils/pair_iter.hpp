
#pragma once

#include <memory>
#include "stl_utils.hpp"

namespace PT{

  // spew out all first/second in a container of pairs
  template<class _PairContainer>
  class PairIterator
  {
  protected:
    using _Iterator = std::IteratorOf_t<_PairContainer>;
    using _Pair = typename _PairContainer::value_type;

    _Iterator pair_it;
  public:
    using value_type = typename std::iterator_traits<_Iterator>::value_type;
    using reference = typename std::iterator_traits<_Iterator>::reference;
    //using const_reference = typename std::iterator_traits<_Iterator>::const_reference; // why oh why, STL, do you not define that for pointers???
    using const_reference = const reference;

    PairIterator(const _Iterator& _it): 
      pair_it(_it)
    {}

    PairIterator(_PairContainer& c):
      pair_it(c.begin())
    {}

    //! increment operator
    PairIterator& operator++()
    {
      ++pair_it;
      return *this;
    }

    //! post-increment
    PairIterator operator++(int)
    {
      PairIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    bool operator==(const PairIterator& it) const { return pair_it == it.pair_it; }
    bool operator!=(const PairIterator& it) const { return !operator==(it); }

    bool operator==(const _Iterator& it) const { return pair_it == it; }
    bool operator!=(const _Iterator& it) const { return !operator==(it); }

  };

  template<class _PairContainer, template<class> class _Selector>
  class SelectingIterator: public PairIterator<_PairContainer>
  {
    using Parent = PairIterator<_PairContainer>;
    using _Iterator = std::IteratorOf_t<_PairContainer>;
    using PairType = typename _PairContainer::value_type;
    using Selector = _Selector<PairType>;

  public:
    // _declaration_ of selector; remember to define it too
    static const Selector select;

    using _Deref = typename Selector::reference;
    using value_type  = typename Selector::value_type;
    using reference   = typename Selector::reference;
    using pointer     = value_type*;
    using difference_type = ptrdiff_t;
    using iterator_category = typename std::iterator_traits<_Iterator>::iterator_category;
    
    using Parent::pair_it;
    using Parent::Parent;
    
    reference operator*() const { return select(*pair_it); }
    SelectingIterator& operator++() { ++pair_it; return *this; }
    SelectingIterator operator++(int) { SelectingIterator it(*this); ++(*this); return it; }
  };
  
  // definition of selector
  template<class _PairContainer, template<class> class _Selector>
  const typename SelectingIterator<_PairContainer, _Selector>::Selector SelectingIterator<_PairContainer, _Selector>::select;

  template<class _PairContainer>
  using SecondIterator = SelectingIterator<_PairContainer, std::extract_second>;
  template<class _PairContainer>
  using FirstIterator = SelectingIterator<_PairContainer, std::extract_first>;


  // factories
  template<class _PairContainer, template<class> class _Iterator>
  class PairItemIterFactory
  {
  protected:
    std::shared_ptr<_PairContainer> c;

  public:
    using iterator =       _Iterator<_PairContainer>;
    using const_iterator = _Iterator<const _PairContainer>;
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using reference =  typename std::iterator_traits<iterator>::reference;

    PairItemIterFactory(const std::shared_ptr<_PairContainer>& _c): c(_c) {}
    PairItemIterFactory(_PairContainer* _c): c(_c) {}

    iterator begin() { return c->begin(); }
    iterator end() { return c->end(); }
    const_iterator begin() const { return c->begin(); }
    const_iterator end() const { return c->end(); }
    bool empty() const { return c->empty(); }
    bool size() const { return c->size(); }
  };

  template<class _PairContainer>
  using SecondFactory = PairItemIterFactory<_PairContainer, SecondIterator>;
  template<class _PairContainer>
  using FirstFactory = PairItemIterFactory<_PairContainer, FirstIterator>;

  template<class _PairContainer>
  SecondFactory<_PairContainer> seconds(std::shared_ptr<_PairContainer>& c) { return {c}; }
  template<class _PairContainer>
  FirstFactory<_PairContainer> firsts(std::shared_ptr<_PairContainer>& c) { return {c}; }
  template<class _PairContainer>
  SecondFactory<_PairContainer> seconds(_PairContainer* c) { return {c}; }
  template<class _PairContainer>
  FirstFactory<_PairContainer> firsts(_PairContainer* c) { return {c}; }
  template<class _PairContainer>
  SecondFactory<_PairContainer> seconds(_PairContainer&& c) { return {new _PairContainer(c)}; }
  template<class _PairContainer>
  FirstFactory<_PairContainer> firsts(_PairContainer&& c) { return {new _PairContainer(c)}; }


  template<class _PairContainer,
           template<class> class _Iterator>
  std::ostream& operator<<(std::ostream& os, const PairItemIterFactory<_PairContainer, _Iterator>& fac)
  {
    for(const auto& i: fac) os << i << ' ';
    return os;
  }

}

