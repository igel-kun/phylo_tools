
#pragma once

#include "stl_utils.hpp"
#include "append.hpp"

namespace mstd {

  // a forward iterator that knows the end of the container & converts to false if it's at the end
  //NOTE: this also supports that the end iterator has a different type than the iterator, as long as they can be compared with "!="
  template<class Iterator, class _EndIterator = CorrespondingEndIter<Iterator>>
  class _auto_iter: public InheritableIter<Iterator>
  {
    static_assert(!VerifyableIter<Iterator>);
    static_assert(!std::is_void_v<_EndIterator>);

    _EndIterator end_it;
   public:
    using Parent = InheritableIter<Iterator>;
    using UnderlyingIterator = Iterator;
    using EndIterator = _EndIterator;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using typename Parent::difference_type;

    // --------------------- Construction & Assignment ---------------------------
    // when default-constructed, end_it == *this, so is_valid() will be false
    _auto_iter() requires (std::is_default_constructible_v<Iterator>):
      Parent(), end_it{*this}
    {}

    // construct from a container
    template<IterableType Container, class... Args>
    constexpr _auto_iter(Container&& c, Args&&... args):
      _auto_iter(mstd::begin(std::forward<Container>(c)), std::end(c), std::forward<Args>(args)...)
    {}

    // construct from two iterators (begin and end)
    template<class _Iterator, class _EndIter, class... Args>
      requires (mstd::is_constructible_v<Iterator, _Iterator&&, Args&&...> && mstd::is_convertible_v<_EndIter, EndIterator>)
    constexpr _auto_iter(_Iterator&& _it, _EndIter&& _end, Args&&... args):
      Parent{std::forward<_Iterator>(_it), std::forward<Args>(args)...},
      end_it{std::forward<_EndIter>(_end)} 
    {
      //std::cout << "\t\tmade auto iter with\n Iterator: "<<type_name<Iterator>()<<"\nEnd-Iter: "<<type_name<_EndIterator>()<<"\n";
    }


    // piecewise construct
    template<class IterTuple, class EndTuple = std::tuple<>>
    _auto_iter(const std::piecewise_construct_t, IterTuple&& iter_init, EndTuple&& end_init = EndTuple{}):
      Parent{make_from_tuple<Iterator>(std::forward<IterTuple>(iter_init))},
      end_it{make_from_tuple<EndIterator>(std::forward<EndTuple>(end_init))}
    {}

    // copy and move-construction & assignment are default
    _auto_iter(const _auto_iter&) = default;
    _auto_iter(_auto_iter&&) = default;
    _auto_iter& operator=(const _auto_iter&) = default;
    _auto_iter& operator=(_auto_iter&&) = default;

    // --------------------- Comparison & Increment --------------------------
    template<class T> 
    bool operator==(const T& other) const {
      if constexpr (VerifyableIter<T>)
        return is_valid() ? (other == get_iter()) : other.is_invalid();
      else return (other == get_iter());
    }   

    _auto_iter& operator++() { ++static_cast<Parent&>(*this); return *this; }
    _auto_iter operator++(int) { _auto_iter result = *this; ++(*this); return result; }
    _auto_iter& operator--() { --static_cast<Parent&>(*this); return *this; }
    _auto_iter operator--(int) { _auto_iter result = *this; --(*this); return result; }
    _auto_iter& operator+=(const ptrdiff_t x) { static_cast<Parent&>(*this) += x; return *this; }
    _auto_iter& operator-=(const ptrdiff_t x) { static_cast<Parent&>(*this) -= x; return *this; }
    _auto_iter operator+(const ptrdiff_t x) const { _auto_iter result = *this; result += x; return result; }
    _auto_iter operator-(const difference_type& x) const { _auto_iter result = *this; result -= x; return result; }
    difference_type operator-(const _auto_iter it) const {
      if(is_valid()) {
        if(it.is_valid()) {
          return get_iter() - it.get_iter();
        } else return -(it.get_end() - it.get_iter());
      } else {
        if(it.is_valid()) {
          return get_end() - it.get_iter();
        } else return get_end() - it.get_end();
      }
    }

    // --------------------- Query ---------------------------
    bool is_valid() const { return get_iter() != end_it; }
    bool is_invalid() const { return !is_valid(); }
    explicit operator bool() const { return is_valid(); }

    Iterator& get_iter() & { return static_cast<Iterator&>(*this); }
    Iterator&& get_iter() && { return static_cast<Iterator&&>(*this); }
    const Iterator& get_iter() const & { return static_cast<const Iterator&>(*this); }

    EndIterator get_end() const & { return end_it; }
    EndIterator get_end() & { return end_it; }
    EndIterator get_end() && { return move(end_it); }

    template<class _Container = std::vector<typename Parent::value_type>>
    auto to_container() const { _Container result; mstd::append(result, *this); return result; }

    template<ContainerType _Container> requires std::is_convertible_v<value_type_of_t<Iterator>, value_type_of_t<_Container>>
    explicit operator _Container() const { return to_container<_Container>(); }
  };


  template<VerifyableIter Iterator>
  struct _auto_iter<Iterator, void>: public Iterator
  {
    using EndIterator = GenericEndIterator;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using Iterator::Iterator;
    using Iterator::iterator_category;
    using typename Iterator::difference_type;

    // --------------------- Construction & Assignment ---------------------------
    /*
    _auto_iter(const _auto_iter& other) = default;
    _auto_iter(_auto_iter&& other) = default;
    _auto_iter& operator=(const _auto_iter&) = default;
    _auto_iter& operator=(_auto_iter&&) = default;
    */

    _auto_iter() = default;
    INHERIT_ALL_CONSTRUCTORS(_auto_iter, Iterator)
    INHERIT_ASSIGNMENT(_auto_iter, Iterator)
    // inherit copy-constructors from Parent
    //template<class First, class... Args> requires (not mstd::is_same_v<First, _auto_iter>)
    //_auto_iter(First&& first, Args&&... args): Iterator(std::forward<First>(first), std::forward<Args>(args)...) {}

    // --------------------- Increment & Decrement ---------------------------
    _auto_iter& operator++() { ++static_cast<Iterator&>(*this); return *this; }
    _auto_iter operator++(int) { _auto_iter result = *this; ++(*this); return result; }
    _auto_iter& operator--() { --static_cast<Iterator&>(*this); return *this; }
    _auto_iter operator--(int) { _auto_iter result = *this; --(*this); return result; }
    _auto_iter& operator+=(const ptrdiff_t x) { static_cast<Iterator&>(*this) += x; return *this; }
    _auto_iter& operator-=(const ptrdiff_t x) { static_cast<Iterator&>(*this) -= x; return *this; }
    _auto_iter operator+(const ptrdiff_t x) const { _auto_iter result = *this; result += x; return result; }
    _auto_iter operator-(const difference_type& x) const { _auto_iter result = *this; result -= x; return result; }
    difference_type operator-(const _auto_iter it) const { static_cast<const Iterator&>(*this) - it; }

    // --------------------- Query ---------------------------
    Iterator& get_iter() & { return *this; }
    const Iterator& get_iter() const &{ return *this; }
    Iterator&& get_iter() && { return *this; }
    static EndIterator get_end() { return GenericEndIterator(); }

    template<class _Container = std::vector<std::remove_cvref_t<value_type_of_t<Iterator>>>>
    auto to_container() const { _Container result; mstd::append(result, *this); return result; }

    template<ContainerType _Container> requires std::is_convertible_v<value_type_of_t<Iterator>, value_type_of_t<_Container>>
    explicit operator _Container() const { return to_container<_Container>(); }
  };

  template<class Iter, class End = CorrespondingEndIter<Iter>> struct proto_auto_iter { using type = _auto_iter<Iter, End>; };
  // forbid making auto_iters of auto_iters
  template<class Iter, class End> struct proto_auto_iter<_auto_iter<Iter, End>> { using type = _auto_iter<Iter, End>; };
  // auto_iter for containers
  template<class Container> requires IterableType<Container>
  struct proto_auto_iter<Container, void> { using type = _auto_iter<mstd::iterator_of_t<Container>>; };

  template<class Iter, class End = CorrespondingEndIter<Iter>>
  using auto_iter = proto_auto_iter<Iter, End>::type;

  // --------------------- deduction guides ---------------------
  template<IterableType Container>
  _auto_iter(Container&& c) -> _auto_iter<iterator_of_t<Container&&>>;


  template<class Iter, class EndIter = CorrespondingEndIter<Iter>>
  using MakeVerifyable = std::conditional_t<VerifyableIter<Iter>, Iter, auto_iter<Iter, EndIter>>;

  template<IterableType Container,
           class KeyType,
           class Iterator = iterator_of_t<Container>>
  auto_iter<Iterator> auto_find(Container&& c, const KeyType& key) { return {c.find(key), end(c)}; }

} //namespace
