
// this defines storages for edges to be used in trees & networks

#pragma once

#include "edge.hpp"

namespace PT{

  template<class _Edge, class _EdgeContainer, class _NodeContainer>
  class EdgeStorage 
  {
  public:
    //! initialization
    template<class EdgeContainer>
    EdgeStorage(const EdgeContainer& edges);
    _NodeContainer successors(const uint32_t u) const;
    _NodeContainer predecessors(const uint32_t u) const;
    _EdgeContainer in_edges(const uint32_t u) const;
    _EdgeContainer out_edges(const uint32_t u) const;

  };


  template<class _Edge, class _EdgeContainer, class _NodeContainer>
  class ImmutableEdgeStorage: public EdgeStorage<_Edge, _EdgeContainer, _NodeContainer>
  {
    _NodeContainer successors(const uint32_t u) const
    {
    }

    _NodeContainer predecessors(const uint32_t u) const
    {
    }

    _EdgeContainer in_edges(const uint32_t u) const
    {
    }

    _EdgeContainer out_edges(const uint32_t u) const
    {
    }

  };

  template<class _Edge, class _EdgeContainer, class _NodeContainer>
  class MutableEdgeStorage: public EdgeStorage<_Edge, _EdgeContainer, _NodeContainer>
  {
  };

}

