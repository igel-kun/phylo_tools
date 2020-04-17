
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

    _Iterator pair_it;
  public:
    using value_type = typename std::iterator_traits<_Iterator>::value_type;
    using reference = typename std::iterator_traits<_Iterator>::reference;
    //using const_reference = typename std::iterator_traits<_Iterator>::const_reference; // why oh why, STL, do you not define that for pointers???
    using const_reference = const reference;

    PairIterator(const _Iterator& _it): pair_it(_it)
    {}

    PairIterator(_PairContainer& c): pair_it(c.begin())
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

  template<class _PairContainer, size_t get_num>
  class SelectingIterator: public std::IteratorOf_t<_PairContainer>
  {
    using Parent = std::IteratorOf_t<_PairContainer>;
  protected:
    using PairType = std::CopyConst_t<_PairContainer, typename Parent::value_type>;
    using PairRefType = std::CopyConst_t<_PairContainer, typename Parent::reference>;

    // normally, dereferencing the parent will give us an lvalue reference to a pair,
    // so our dereference can just return a reference to the correct item in that pair.
    // HOWEVER: we allow the parent to dereference to an rvalue (temporary pair constructed by the parent),
    // and the item in the temporary pair that we are interested in is also an rvalue.
    // In this case, we also have to return an rvalue...
    static constexpr bool dereferences_to_lvalref = std::is_lvalue_reference_v<PairRefType> || 
                                                    std::is_lvalue_reference_v<std::tuple_element_t<get_num, std::remove_reference_t<PairType>>>;


    PairRefType parent_deref() const { return Parent::operator*(); }
  public:
    using Parent::Parent;
    
    using value_type  = std::tuple_element_t<get_num, PairType>;
    using reference   = std::conditional_t<dereferences_to_lvalref, value_type&, value_type>;
    using pointer     = std::conditional_t<dereferences_to_lvalref, value_type*, std::self_deref<reference>>;
    using difference_type = ptrdiff_t;
    
    SelectingIterator(const Parent& p): Parent(p) {}

    reference operator*() const { return std::get<get_num>(parent_deref()); }
    pointer operator->() const { return operator*(); }
  };

  template<class _PairContainer>
  using FirstIterator = SelectingIterator<_PairContainer, 0>;
  template<class _PairContainer>
  using SecondIterator = SelectingIterator<_PairContainer, 1>;


  // factories
  template<class _PairContainer, size_t get_num>
  class PairItemIterFactory
  {
  protected:
    std::shared_ptr<_PairContainer> c;

  public:
    using iterator = SelectingIterator<_PairContainer, get_num>;
    using const_iterator = SelectingIterator<const _PairContainer, get_num>;
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using reference =  typename std::iterator_traits<iterator>::reference;

    // if constructed via a reference, do not destruct the object, if constructed via a pointer (à la "new _AdjContainer()"), do destruct after use
    PairItemIterFactory(_PairContainer* const _c): c(_c)  {}
    PairItemIterFactory(_PairContainer& _c): c(&_c, std::NoDeleter()) {}
    PairItemIterFactory(_PairContainer&& _c): c(new _PairContainer(std::forward<_PairContainer>(_c))) {}

    iterator begin() { return c->begin(); }
    iterator end() { return c->end(); }
    const_iterator begin() const { return const_iterator(c->begin()); }
    const_iterator end() const { return c->end(); }
    bool empty() const { return c->empty(); }
    bool size() const { return c->size(); }
  };

  template<class _PairContainer>
  using FirstFactory = PairItemIterFactory<_PairContainer, 0>;
  template<class _PairContainer>
  using SecondFactory = PairItemIterFactory<_PairContainer, 1>;

  template<class _PairContainer>
  constexpr FirstFactory<_PairContainer> firsts(_PairContainer& c) { return c; }
  template<class _PairContainer>
  constexpr SecondFactory<_PairContainer> seconds(_PairContainer& c) { return c; }

  template<class _PairContainer, size_t get_num>
  std::ostream& operator<<(std::ostream& os, const PairItemIterFactory<_PairContainer, get_num>& fac)
  {
    for(const auto& i: fac) os << i << ' ';
    return os;
  }

}

