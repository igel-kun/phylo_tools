
#pragma once

#include "auto_iter.hpp"

namespace std {

  template<class Iterator, class _EndIter = CorrespondingEndIter<Iterator>>
  class ProtoIterFactory: public auto_iter<Iterator, _EndIter> {
    using Parent = auto_iter<Iterator, _EndIter>;
    using EndIter = typename Parent::EndIterator;
  public:
    using Parent::Parent;

    bool empty() const { return begin() == end(); }
    size_t size() const { return std::distance(begin(), end()); }

    Iterator begin() const & { return static_cast<const Iterator&>(*this); }
    Iterator begin() & { return static_cast<Iterator&>(*this); }
    Iterator begin() && { return static_cast<Iterator&&>(*this); }
    EndIter end() const & { return static_cast<const Parent&>(*this).get_end(); }
    EndIter end() & { return static_cast<Parent&>(*this).get_end(); }
    EndIter end() && { return static_cast<Parent&&>(*this).get_end(); }
  };

  template<class Iterator, class BeginEndTransformation, class EndIter = CorrespondingEndIter<Iterator>> requires (!is_void_v<BeginEndTransformation>)
  class IterFactoryWithBeginEnd: public ProtoIterFactory<Iterator, EndIter> {
    using Parent = auto_iter<Iterator, EndIter>;
    BeginEndTransformation trans;    
  public:
    IterFactoryWithBeginEnd() = default;
    // construct from an auto_iter and a BeginEndTransformation
    template<class T, class... Args> requires is_same_v<decay_t<T>, decay_t<BeginEndTransformation>>
    IterFactoryWithBeginEnd(T&& _trans, Args&&... args): Parent(forward<Args>(args)...), trans(forward<T>(_trans)) {}
    // construct our internal auto_iter and our transformation from two tuples
    template<class PTuple, class TTuple>
    IterFactoryWithBeginEnd(const piecewise_construct_t, TTuple&& trans_init, PTuple&& parent_init):
      Parent(make_from_tuple<Parent>(forward<PTuple>(parent_init))),
      trans(make_from_tuple<BeginEndTransformation>(forward<TTuple>(trans_init)))
    {}
    IterFactoryWithBeginEnd(const IterFactoryWithBeginEnd&) = default;
    IterFactoryWithBeginEnd(IterFactoryWithBeginEnd&&) = default;

    auto begin() const & { return trans(static_cast<const Parent&>(*this).begin()); }
    auto begin() & { return trans(static_cast<Parent&>(*this).begin()); }
    auto begin() && { return trans(static_cast<Parent&&>(*this).begin()); }
    auto end() const & { return trans(static_cast<const Parent&>(*this).end()); }
    auto end() & { return trans(static_cast<Parent&>(*this).end()); }
    auto end() && { return trans(static_cast<Parent&&>(*this).end()); }
  };

  template<class Iterator, class BeginEndTransformation = void, class EndIter = CorrespondingEndIter<Iterator>>
  struct _IterFactory { using type = IterFactoryWithBeginEnd<Iterator, BeginEndTransformation, EndIter>; };
  template<class Iterator, class EndIter>
  struct _IterFactory<Iterator, void, EndIter> { using type = ProtoIterFactory<Iterator, EndIter>; };

  template<class Iter, class BeginEndTransformation = void, class EndIter = CorrespondingEndIter<std::iterator_of_t<Iter>>>
  using IterFactory = typename _IterFactory<iterator_of_t<Iter>, BeginEndTransformation, EndIter>::type;
}
