
#pragma once

#include "edge.hpp"

namespace PT{

  // spew out all heads in a list of edges
  template<class EdgeContainer = EdgeVec, class _Iterator = typename EdgeContainer::iterator>
  class EdgeIterator
  {
  protected:
    _Iterator edge_it;
  public:

    EdgeIterator(const EdgeIterator& _it):
      edge_it(_it.edge_it)
    {}

    EdgeIterator(const _Iterator& _it):
      edge_it(_it)
    {}

    //! increment operator
    EdgeIterator& operator++()
    {
      ++edge_it;
      return *this;
    }

    //! post-increment
    EdgeIterator operator++(int)
    {
      EdgeIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    bool operator==(const EdgeIterator& it) const
    {
      return edge_it == it.edge_it;
    }
    bool operator!=(const EdgeIterator& it) const
    {
      return !operator==(it);
    }
  };

  template<class EdgeContainer = EdgeVec, class _Iterator = typename EdgeContainer::iterator>
  class TailIterator: public EdgeIterator<EdgeContainer, _Iterator>
  {
    using Parent = EdgeIterator<EdgeContainer, _Iterator>;
    using Parent::edge_it;
  public:
    using Parent::EdgeIterator;

    uint32_t operator*() const
    {
      return tail(*edge_it);
    }
  };

  template<class EdgeContainer = EdgeVec, class _Iterator = typename EdgeContainer::iterator>
  class HeadIterator: public EdgeIterator<EdgeContainer, _Iterator>
  {
    using Parent = EdgeIterator<EdgeContainer, _Iterator>;
    using Parent::edge_it;
  public:
    using Parent::EdgeIterator;

    uint32_t operator*() const
    {
      return head(*edge_it);
    }
  };

  template<class EdgeContainer = EdgeVec>
  using HeadConstIterator = HeadIterator<EdgeContainer, typename EdgeContainer::const_iterator>;
  template<class EdgeContainer = EdgeVec>
  using TailConstIterator = TailIterator<EdgeContainer, typename EdgeContainer::const_iterator>;


  template<class _Iterator, class EdgeContainer = EdgeVec>
  class IteratorFactory
  {
  protected:
    EdgeContainer& edges;

  public:
    IteratorFactory(EdgeContainer& _edges):
      edges(_edges)
    {}
    _Iterator begin()
    {
      return edges.begin();
    }
    _Iterator end()
    {
      return edges.end();
    }
  };


  template<class EdgeContainer = EdgeVec>
  using HeadFactory = IteratorFactory<HeadIterator<EdgeContainer>, EdgeContainer>;
  template<class EdgeContainer = EdgeVec>
  using TailFactory = IteratorFactory<TailIterator<EdgeContainer>, EdgeContainer>;
  template<class EdgeContainer = EdgeVec>
  using HeadConstFactory = IteratorFactory<HeadConstIterator<const EdgeContainer>, const EdgeContainer>;
  template<class EdgeContainer = EdgeVec>
  using TailConstFactory = IteratorFactory<TailConstIterator<const EdgeContainer>, const EdgeContainer>;

  template<class EdgeContainer = EdgeVec>
  HeadFactory<EdgeContainer> heads(EdgeContainer& c)
  {
    return HeadFactory<EdgeContainer>(c);
  }
  template<class EdgeContainer = EdgeVec>
  TailFactory<EdgeContainer> tails(EdgeContainer& c)
  {
    return TailFactory<EdgeContainer>(c);
  }
   template<class EdgeContainer = EdgeVec>
  HeadConstFactory<EdgeContainer> heads(const EdgeContainer& c)
  {
    return HeadConstFactory<EdgeContainer>(c);
  }
  template<class EdgeContainer = EdgeVec>
  TailConstFactory<EdgeContainer> tails(const EdgeContainer& c)
  {
    return TailConstFactory<EdgeContainer>(c);
  }


}
