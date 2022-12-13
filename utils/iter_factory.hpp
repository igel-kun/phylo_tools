
#pragma once

#include "auto_iter.hpp"

namespace mstd {

  template<class Iterator, class _EndIter = CorrespondingEndIter<Iterator>>
  class ProtoIterFactory: public auto_iter<Iterator, _EndIter> {
    using Parent = auto_iter<Iterator, _EndIter>;
    using EndIter = typename Parent::EndIterator;
  public:
    using Parent::Parent;
    using Parent::get_iter;
    using Parent::get_end;

    bool empty() const { return begin() == end(); }
    size_t size() const { return std::distance(begin(), end()); }

    Iterator begin() const & { return get_iter(); }
    Iterator begin() & { return get_iter(); }
    Iterator begin() && { return get_iter(); }
    EndIter end() const & { return get_end(); }
    EndIter end() & { return get_end(); }
    EndIter end() && { return get_end(); }
  };

  template<class Iterator, class BeginEndTransformation, class EndIter = CorrespondingEndIter<Iterator>> requires (!std::is_void_v<BeginEndTransformation>)
  class IterFactoryWithBeginEnd: public ProtoIterFactory<Iterator, EndIter> {
    using Parent = ProtoIterFactory<Iterator, EndIter>;
    BeginEndTransformation trans;    
  public:
    BeginEndTransformation& get_begin_end_transformation() { return trans; }
    const BeginEndTransformation& get_begin_end_transformation() const { return trans; }

    IterFactoryWithBeginEnd() = default;
    // construct from a BeginEndTransformation and args for the auto_iter
    template<class T, class... Args> requires std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<BeginEndTransformation>>
    IterFactoryWithBeginEnd(T&& _trans, Args&&... args): Parent(std::forward<Args>(args)...), trans(forward<T>(_trans)) {}
    // if the first argument is not a BeginEndTransformation, not an IterFactoryWithBeginEnd, and not piecewise_construct,
    // then default-construct the transformation
    template<class First, class... Args>
      requires (!std::is_same_v<std::remove_cvref_t<First>, std::remove_cvref_t<BeginEndTransformation>> &&
                !std::is_same_v<std::remove_cvref_t<First>, IterFactoryWithBeginEnd> &&
                !std::is_same_v<std::remove_cvref_t<First>, std::piecewise_construct_t>)
    IterFactoryWithBeginEnd(First&& first, Args&&... args): Parent(std::forward<First>(first), std::forward<Args>(args)...), trans() {}

    // construct our internal auto_iter and our transformation from two tuples
    template<class PTuple, class TTuple>
    IterFactoryWithBeginEnd(const std::piecewise_construct_t, TTuple&& trans_init, PTuple&& parent_init):
      Parent(make_from_tuple<Parent>(forward<PTuple>(parent_init))),
      trans(make_from_tuple<BeginEndTransformation>(forward<TTuple>(trans_init)))
    {}
    IterFactoryWithBeginEnd(const IterFactoryWithBeginEnd&) = default;
    IterFactoryWithBeginEnd(IterFactoryWithBeginEnd&&) = default;

    auto begin() const & { return trans(static_cast<const Parent&>(*this).begin()); }
    auto begin() & { return trans(static_cast<Parent&>(*this).begin()); }
    auto begin() && { return std::move(trans)(static_cast<Parent&&>(*this).begin()); }
    auto end() const & { return trans(static_cast<const Parent&>(*this).end()); }
    auto end() & { return trans(static_cast<Parent&>(*this).end()); }
    auto end() && { return std::move(trans)(static_cast<Parent&&>(*this).end()); }
  };

  template<class Iterator, class BeginEndTransformation = void, class EndIter = CorrespondingEndIter<Iterator>>
  struct _IterFactory { using type = IterFactoryWithBeginEnd<Iterator, BeginEndTransformation, EndIter>; };
  template<class Iterator, class EndIter>
  struct _IterFactory<Iterator, void, EndIter> { using type = ProtoIterFactory<Iterator, EndIter>; };

  template<class Iter, class BeginEndTransformation = void, class EndIter = CorrespondingEndIter<iterator_of_t<Iter>>>
  using IterFactory = typename _IterFactory<iterator_of_t<Iter>, BeginEndTransformation, EndIter>::type;
}
