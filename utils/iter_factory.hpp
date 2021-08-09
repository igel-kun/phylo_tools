
#pragma once

#include <memory>
#include "stl_utils.hpp"

namespace std {
  // an IterFactory stores a reference to an Iterable object and can return begin()/end() iterators of the class Iterator,
  // each constructed with begin()/end() of the container object; in this way, a factory is a kind of "VIEW" on the container through the Iterator
  //PARAMETERS:
  //  Container = the container over which to iterate
  //  BeginEnd = a struct providing static begin(Container&) and end(Container&)
  //  (Const)Iterator = iterator type to iterate over the container, (Const)Iterator must be constructible from the result of begin((const) Container&)
  //NOTE: you can construct factories by passing
  // (1) a reference to an existing container, in which case this reference is used to access the container
  // (2) a shared_ptr to a container
  // (3) an rvalue reference to a container, in which case the factory moves out of that container and manages its own copy
  template<IterableType Container,
           class BeginEnd = BeginEndIters<Container>,  // I'm terribly sorry about the bulky BeginEnd stuff, but C++17 doesn't let me use lambdas here :(
           class Iterator = typename BeginEnd::iterator,
           class ConstIterator = typename BeginEnd::const_iterator>
  class _IterFactory: public my_iterator_traits<Iterator>
  {
  protected:
    shared_ptr<Container> container;
  public:
    using NonConstContainer = remove_cv_t<Container>;
    using iterator = Iterator;
    using const_iterator = ConstIterator;

    // allow move-construction from a container
    // however since C++ has no way of verifying if a container truely is move constructible (is_move_constructible is true for copy constructibles),
    //    the user better be sure that the Container has a move constructor!!!
    _IterFactory(NonConstContainer&& _c): container(make_shared<Container>(forward<NonConstContainer>(_c))) {}

    _IterFactory(Container& _c): container(&_c, NoDeleter()) {}
    _IterFactory(shared_ptr<Container> _c): container(move(_c)) {}
    _IterFactory(unique_ptr<Container> _c): container(move(_c)) {}

    iterator begin() { return BeginEnd::begin(*container); }
    iterator end() { return BeginEnd::end(*container); }
    const_iterator begin() const { return BeginEnd::begin(static_cast<const Container&>(*container)); }
    const_iterator end() const { return BeginEnd::end(static_cast<const Container&>(*container)); }
    const_iterator cbegin() const { return BeginEnd::begin(static_cast<const Container&>(*container)); }
    const_iterator cend() const { return BeginEnd::end(static_cast<const Container&>(*container)); }

    bool empty() const { return container->empty(); }
    size_t size() const { return container->size(); }

    // copy the elements in the traversal to a container using the 'append()'-function
    template<class _Container>
    _Container& append_to(_Container& c) const { append(c, *this); }
    template<class _Container = vector<remove_cvref_t<typename Iterator::value_type>>>
    _Container to_container() const { _Container result; append(result, *this); return result; }
  };

  // same as above, but we allow storing local data with the factory that is passed to each created iterator
  template<IterableType Container,
           class FactoryData = void,
           class BeginEnd = BeginEndIters<Container>,
           class Iterator = typename BeginEnd::iterator,
           class ConstIterator = typename BeginEnd::const_iterator>
  class IterFactory: public _IterFactory<Container, BeginEnd, Iterator, ConstIterator>
  {
    using Parent = _IterFactory<Container, BeginEnd, Iterator, ConstIterator>;
  protected:
    using Parent::container;
    FactoryData _data;
  public:
    using iterator = Iterator;
    using const_iterator = ConstIterator;

    template<class... Args>
    IterFactory(Container& _c, Args&&... args): Parent(_c), _data(forward<Args>(args)...) {}
    template<class... Args>
    IterFactory(shared_ptr<Container> _c, Args&&... args): Parent(move(_c)), _data(forward<Args>(args)...) {}
    template<class... Args>
    IterFactory(unique_ptr<Container> _c, Args&&... args): Parent(move(_c)), _data(forward<Args>(args)...) {}

    // see comments for the corresponding constructor in _IterFactory; in short: make sure Container has a move-constructor
    template<class... Args>
    IterFactory(remove_cv_t<Container>&& _c, Args&&... args): Parent(move(_c)), _data(forward<Args>(args)...) {}

    iterator begin() { return BeginEnd::begin(*container, _data); }
    iterator end()   { return BeginEnd::end(*container, _data); }
    const_iterator begin()  const { return BeginEnd::begin(*container, _data); }
    const_iterator end()    const { return BeginEnd::end(*container, _data); }
    const_iterator cbegin() const { return BeginEnd::begin(*container, _data); }
    const_iterator cend()   const { return BeginEnd::end(*container, _data); }
  };

  // the factory specialization for void local data just falls back to _IterFactory
  template<IterableType Container,
           class BeginEnd,
           class Iterator,
           class ConstIterator>
  struct IterFactory<Container, void, BeginEnd, Iterator, ConstIterator>: public _IterFactory<Container, BeginEnd, Iterator, ConstIterator>
  { using _IterFactory<Container, BeginEnd, Iterator, ConstIterator>::_IterFactory; };

  // a convenience alias taking an iterator template mask instead of 2 iterator types
  template<IterableType Container,
           class FactoryData,
           class BeginEnd,
           template<class> class IteratorTpl>
  using IterTplIterFactory = IterFactory<Container, FactoryData, BeginEnd, IteratorTpl<Container>, IteratorTpl<const Container>>;

}
