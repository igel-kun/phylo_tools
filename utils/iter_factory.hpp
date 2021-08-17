
#pragma once

#include <memory>
#include "stl_utils.hpp"
#include "auto_iter.hpp"

namespace std {
  // an IterFactory stores an auto_iter and can return begin()/end() iterators of the class Iterator,
  //PARAMETERS:
  // Iterator = the internal iterator type
  // BeginEndTransformation = a transformation function transforming the internal iterator to the result of begin() / end()
  template<class Iterator>
  class ProtoIterFactory: public my_iterator_traits<Iterator>
  {
  protected:
    using InternalIter = auto_iter<Iterator>;
    InternalIter it;
  public:
    using iterator = Iterator;
    using const_iterator = Iterator;

    template<class... Args>
    ProtoIterFactory(Args&&... args): it(forward<Args>(args)...) {}

    auto begin() const { return it.get_iter(); }
    auto end() const { return it.get_end(); }

    bool empty() const { return !(it.is_valid()); }
    // even if we know the size of the container, this may not be equal to the size of the IterFactory (think of using skipping_iterator for example)
    size_t size() const { return distance(*it, it.get_end()); }

    // copy the elements in the traversal to a container using the 'append()'-function
    template<class _Container>
    _Container& append_to(_Container& c) const { append(c, *this); }
    template<class _Container = vector<remove_cvref_t<value_type_of_t<Iterator>>>>
    _Container to_container() const { _Container result; append(result, *this); return result; }
  };


  template<class Iterator, class BeginEndTransformation = void>
  class _IterFactory: public ProtoIterFactory<Iterator> {
    using Parent = ProtoIterFactory<Iterator>;
    BeginEndTransformation trans;
  public:
    // construct from an auto_iter and a BeginEndTransformation
    template<class T, class Q>
      requires (is_same_v<remove_cvref_t<T>, typename Parent::InternalIter> && is_same_v<remove_cvref_t<Q>, BeginEndTransformation>)
    _IterFactory(T&& _iter, Q&& _trans): Parent(forward<T>(_iter)), trans(forward<Q>(_trans)) {}
    // construct our internal auto_iter from anything and our transformation from a given Transformation function
    template<class PTuple, class TTuple>
    _IterFactory(const piecewise_construct_t, PTuple&& parent_init, TTuple&& trans_init):
      Parent(forward<PTuple>(parent_init)),
      trans(make_from_tuple<BeginEndTransformation>(forward<TTuple>(trans_init)))
    {}

    auto begin() const { return it_trans(Parent::begin()); }
    auto end() const { return it_trans(Parent::end()); }

  };

  template<class Iterator, class BeginEndTransformation = void>
  struct __IterFactory { using type = _IterFactory<Iterator, BeginEndTransformation>; };
  template<class Iterator>
  struct __IterFactory<Iterator, void> { using type = ProtoIterFactory<Iterator>; };

  template<class T, class BeginEndTransformation = void>
  using IterFactory = typename __IterFactory<iterator_of_t<T>, BeginEndTransformation>::type;

}
