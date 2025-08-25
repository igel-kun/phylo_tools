
#pragma once

#include "stl_utils.hpp"
#include "append.hpp"

namespace mstd {

  // a forward iterator that knows the end of the container & converts to false if it's at the end
  //NOTE: this also supports that the end iterator has a different type than the iterator, as long as they can be compared with "!="
  template<class Iterator, class EndIterator_ = CorrespondingEndIter<Iterator>>
  class _auto_iter: public InheritableIter<Iterator>
  {
    static_assert(!VerifyableIter<Iterator>);
    static_assert(!std::is_void_v<EndIterator_>);

    EndIterator_ end_it;
   public:
    using Parent = InheritableIter<Iterator>;
    using UnderlyingIterator = Iterator;
    using EndIterator = EndIterator_;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using typename Parent::difference_type;


    // --------------------- Construction & Assignment ---------------------------
    static constexpr bool reverse = mstd::is_derived_from_template_v<Iterator, std::reverse_iterator>;

    // when default-constructed, end_it == *this, so is_valid() will be false
    _auto_iter() requires (std::is_default_constructible_v<Iterator>):
      Parent(), end_it{*this}
    {}

    // construct from a container, if Iterator is an std::reverse_iterator, then construct using std::rbegin
    template<IterableType Container, class... Args>
    constexpr _auto_iter(Container&& c, Args&&... args) requires (not reverse):
      _auto_iter(mstd::begin(std::forward<Container>(c)), std::end(c), std::forward<Args>(args)...)
    {}
    template<IterableType Container, class... Args>
    constexpr _auto_iter(Container&& c, Args&&... args) requires (reverse):
      _auto_iter(mstd::rbegin(std::forward<Container>(c)), std::rend(c), std::forward<Args>(args)...)
    {}

    // construct from two iterators (begin and end)
    template<class Iterator_, class EndIter_, class... Args>
      requires (mstd::is_constructible_v<Iterator, Iterator_&&, Args&&...> && mstd::is_convertible_v<EndIter_, EndIterator>)
    constexpr _auto_iter(Iterator_&& _it, EndIter_&& _end, Args&&... args):
      Parent{std::forward<Iterator_>(_it), std::forward<Args>(args)...},
      end_it{std::forward<EndIter_>(_end)} 
    {
      //std::cout << "\t\tmade auto iter with\n Iterator: "<<type_name<Iterator>()<<"\nEnd-Iter: "<<type_name<EndIterator_>()<<"\n";
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
    template<class T> 
    bool operator!=(const T& other) const { return not operator==(other); }

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

    template<class Container_>
    void append_to(Container_& C) const { mstd::append(C, *this); }

    template<class Container_ = std::vector<typename Parent::value_type>>
    auto to_container() const { Container_ result; append_to(result); return result; }

    template<ContainerType Container_> requires std::is_convertible_v<value_type_of_t<Iterator>, value_type_of_t<Container_>>
    explicit operator Container_() const { return to_container<Container_>(); }
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
    _auto_iter() = default;
    INHERIT_ALL_CONSTRUCTORS(_auto_iter, Iterator)
    INHERIT_ASSIGNMENT(_auto_iter, Iterator)

    // --------------------- Increment & Decrement ---------------------------
    _auto_iter& operator++() { ++static_cast<Iterator&>(*this); return *this; }
    _auto_iter operator++(int) { _auto_iter result = *this; ++(*this); return result; }
    _auto_iter& operator--() { --static_cast<Iterator&>(*this); return *this; }
    _auto_iter operator--(int) { _auto_iter result = *this; --(*this); return result; }
    _auto_iter& operator+=(const ptrdiff_t x) { static_cast<Iterator&>(*this) += x; return *this; }
    _auto_iter& operator-=(const ptrdiff_t x) { static_cast<Iterator&>(*this) -= x; return *this; }
    _auto_iter operator+(const ptrdiff_t x) const { _auto_iter result = *this; result += x; return result; }
    _auto_iter operator-(const difference_type& x) const { _auto_iter result = *this; result -= x; return result; }
    difference_type operator-(const _auto_iter& it) const { return operator-(static_cast<const Iterator&>(it)); }
    difference_type operator-(const Iterator& it) const { static_cast<const Iterator&>(*this) - it; }

    // --------------------- Query ---------------------------
    Iterator& get_iter() & { return *this; }
    const Iterator& get_iter() const &{ return *this; }
    Iterator&& get_iter() && { return *this; }
    static EndIterator get_end() { return GenericEndIterator(); }

    template<class Container_>
    void append_to(Container_& C) const { mstd::append(C, *this); }

    template<class Container_ = std::vector<std::remove_cvref_t<value_type_of_t<Iterator>>>>
    auto to_container() const { Container_ result; append_to(result); return result; }

    template<ContainerType Container_> requires std::is_convertible_v<value_type_of_t<Iterator>, value_type_of_t<Container_>>
    explicit operator Container_() const { return to_container<Container_>(); }
  };


  template<class... Args> struct proto_auto_iter {};

  template<HasIterCategory Iter, class... Args> requires (not is_derived_from_template_v<Iter, _auto_iter>)
  struct proto_auto_iter<Iter, Args...> { using type = _auto_iter<Iter, Args...>; };
  // forbid making auto_iters of auto_iters
  //template<class Iter, class End> struct proto_auto_iter<_auto_iter<Iter, End>> { using type = _auto_iter<Iter, End>; };
  template<HasIterCategory Iter, class... Args> requires (is_derived_from_template_v<Iter, _auto_iter>)
  struct proto_auto_iter<Iter, Args...> { using type = Iter; };

  // auto_iter for containers
  template<IterableType<TR_ConstRefOK> Container, class... Args> requires (not HasIterCategory<Container, TR_ConstRefOK>)
  struct proto_auto_iter<Container, Args...> { using type = _auto_iter<mstd::iterator_of_t<std::remove_reference_t<Container>>>; };

  template<class... Args> 
  using auto_iter = proto_auto_iter<Args...>::type;

  // --------------------- deduction guides ---------------------
  template<IterableType Container>
  _auto_iter(Container&& c) -> _auto_iter<iterator_of_t<Container&&>>;


  template<class Iter, class... Args>
  using MakeVerifyable = std::conditional_t<VerifyableIter<Iter>, Iter, auto_iter<Iter, Args...>>;

  template<IterableType Container,
           class KeyType,
           class Iterator = iterator_of_t<Container>>
  auto_iter<Iterator> auto_find(Container&& c, const KeyType& key) { return {c.find(key), end(c)}; }

} //namespace
