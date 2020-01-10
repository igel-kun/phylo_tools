
#pragma once

namespace PT{


  // classes to extract second or first from an edge
  template<class _Pair, class _ItemRef = typename _Pair::second_type&>
  struct extract_second{
    using reference = _ItemRef;
    const _ItemRef operator()(const _Pair& e) const { return e.second; }
    _ItemRef operator()(_Pair& e) const { return e.second; }
  };
  template<class _Pair, class _ItemRef = typename _Pair::first_type&>
  struct extract_first{
    using reference = _ItemRef;
    const _ItemRef operator()(const _Pair& e) const { return e.first; }
    _ItemRef operator()(_Pair& e) const { return e.first; }
  };
  template<class _Pair>
  using const_extract_second = extract_second<_Pair, const typename _Pair::second_type&>;
  template<class _Pair>
  using const_extract_first = extract_first<_Pair, const typename _Pair::first_type&>;



  // spew out all first/second in a list of pairs
  template<class _PairContainer,
           class _Iterator = typename _PairContainer::iterator,
           class _Pair = typename _PairContainer::value_type>
  class PairIterator
  {
  protected:
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

  template<class _PairContainer,
           class _Iterator = typename _PairContainer::iterator,
           class _Selector = extract_second<typename _PairContainer::value_type>,
           class _Deref = typename _Selector::reference>
  class SelectingIterator: public PairIterator<_PairContainer, _Iterator>
  {
    using Parent = PairIterator<_PairContainer, _Iterator>;
    using Parent::pair_it;

    // _declaration_ of selector; remember to define it too
    static const _Selector select;
  public:
    using Parent::Parent;
    using value_type = _Deref;
    using reference = value_type&;
    using const_reference = const reference;

    _Deref operator*() const { return select(*pair_it); }
    SelectingIterator& operator++() { ++pair_it; return *this; }
    SelectingIterator operator++(int) { SelectingIterator it(*this); ++(*this); return it; }
  };
  
  // definition of selector
  template<class _PairContainer, class _Iterator, class _Selector, typename _Dref>
  const _Selector SelectingIterator<_PairContainer, _Iterator, _Selector, _Dref>::select;


  template<class _PairContainer>
  using SecondIterator = SelectingIterator<_PairContainer, typename _PairContainer::iterator, extract_second<typename _PairContainer::value_type>>;
  template<class _PairContainer>
  using FirstIterator = SelectingIterator<_PairContainer, typename _PairContainer::iterator, extract_first<typename _PairContainer::value_type>>;
  template<class _PairContainer>
  using ConstSecondIterator = SelectingIterator<_PairContainer, typename _PairContainer::const_iterator, const_extract_second<typename _PairContainer::value_type>>;
  template<class _PairContainer>
  using ConstFirstIterator = SelectingIterator<_PairContainer, typename _PairContainer::const_iterator, const_extract_first<typename _PairContainer::value_type>>;





  // factories
  template<class _PairContainer,
           class _Iterator,
           class _ConstIterator>
  class PairItemIterFactory
  {
  protected:
    _PairContainer& c;

  public:
    using value_type = typename std::iterator_traits<_Iterator>::value_type;
    using reference = typename std::iterator_traits<_Iterator>::reference;
    //using const_reference = typename std::iterator_traits<_Iterator>::const_reference;
    using const_reference = typename _Iterator::const_reference;

    using iterator = _Iterator;
    using const_iterator = _ConstIterator;

    PairItemIterFactory(_PairContainer& _c):
      c(_c)
    {}
    iterator begin() { return c.begin(); }
    iterator end() { return c.end(); }
    const_iterator begin() const { return c.begin(); }
    const_iterator end() const { return c.end(); }
    const_iterator cbegin() const { return c.begin(); }
    const_iterator cend() const { return c.end(); }
    bool empty() const { return c.empty(); }
    bool size() const { return c.size(); }

    template<class __PairContainer, class __Iterator, class __ConstIterator>
    operator PairItemIterFactory<__PairContainer, __Iterator, __ConstIterator>() const
    {
      return PairItemIterFactory<__PairContainer, __Iterator, __ConstIterator>(c);
    }

  };

  template<class _PairContainer>
  using SecondFactory = PairItemIterFactory<_PairContainer, SecondIterator<_PairContainer>, ConstSecondIterator<_PairContainer>>;
  template<class _PairContainer>
  using FirstFactory = PairItemIterFactory<_PairContainer, FirstIterator<_PairContainer>, ConstFirstIterator<_PairContainer>>;
  template<class _PairContainer>
  using ConstSecondFactory = PairItemIterFactory<const _PairContainer, ConstSecondIterator<const _PairContainer>, ConstSecondIterator<const _PairContainer>>;
  template<class _PairContainer>
  using ConstFirstFactory = PairItemIterFactory<const _PairContainer, ConstFirstIterator<const _PairContainer>, ConstFirstIterator<const _PairContainer>>;

  template<class _PairContainer>
  SecondFactory<_PairContainer> seconds(_PairContainer& c) { return SecondFactory<_PairContainer>(c); }
  template<class _PairContainer>
  FirstFactory<_PairContainer> firsts(_PairContainer& c) { return FirstFactory<_PairContainer>(c); }
  template<class _PairContainer>
  ConstSecondFactory<_PairContainer> const_seconds(const _PairContainer& c) { return ConstSecondFactory<_PairContainer>(c); }
  template<class _PairContainer>
  ConstFirstFactory<_PairContainer> const_firsts(const _PairContainer& c) { return ConstFirstFactory<_PairContainer>(c); }


  // return a FirstFactory M if N is a pair; otherwise, just return M
  template<class N, class M>
  struct GetFirstFactory{ using Factory = M&; };
  template<class N, class M>
  struct GetConstFirstFactory{ using Factory = const M&; };
  template<class P, class Q, class M>
  struct GetFirstFactory<std::pair<P,Q>, M>{ using Factory = FirstFactory<M>; };
  template<class P, class Q, class M>
  struct GetConstFirstFactory<std::pair<P,Q>, M>{ using Factory = ConstFirstFactory<M>; };

  template<class N, class M>
  struct GetSecondFactory{ using Factory = M&; };
  template<class N, class M>
  struct GetConstSecondFactory{ using Factory = const M&; };
  template<class P, class Q, class M>
  struct GetSecondFactory<std::pair<P,Q>, M>{ using Factory = SecondFactory<M>&; };
  template<class P, class Q, class M>
  struct GetConstSecondFactory<std::pair<P,Q>, M>{ using Factory = ConstSecondFactory<M>&; };

  template<class _PairContainer,
           class _Iterator,
           class _ConstIterator>
  std::ostream& operator<<(std::ostream& os, const PairItemIterFactory<_PairContainer, _Iterator, _ConstIterator>& fac)
  {
    for(const auto& i: fac) os << i << ' ';
    return os;
  }

}


namespace std{

  template<class _PairContainer,
           class _Iterator,
           class _Pair>
  struct iterator_traits<PT::PairIterator<_PairContainer, _Iterator, _Pair> >: public iterator_traits<_Iterator>
  {
    using const_reference = const typename iterator_traits<_Iterator>::reference;
  };

  template<class _PairContainer,
           class _Iterator,
           class _Selector,
           class _Deref>
  struct iterator_traits<PT::SelectingIterator<_PairContainer, _Iterator, _Selector, _Deref> >: public iterator_traits<_Iterator>
  {
    using const_reference = const typename iterator_traits<_Iterator>::reference;
  };

}


