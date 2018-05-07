#pragma once

#include "utils.hpp"
#include <queue>

namespace PT{

  // an edge is a node (head) with an adjacency (->tail), the latter might have a weight and more information attached to it
  class Edge: public std::pair<uint32_t, uint32_t>
  {
    using Parent = std::pair<uint32_t, uint32_t>;
  public:
    using Parent::pair;

    uint32_t& head() { return this->second; }
    uint32_t& tail() { return this->first; }
    const uint32_t& head() const { return this->second; }
    const uint32_t& tail() const { return this->first; }

    bool operator<(const Edge& e) const
    {
      if(this->second == e.second)
        return this->first < e.first;
      else 
        return this->second < e.second;
    }
  };

  template<class _Data>
  class EdgeWithData: public Edge
  {
  public:
    _Data data;
    
    using Edge::Edge;

    EdgeWithData(const Edge& e, const _Data& _data):
      Edge(e),
      data(_data)
    {}
  };


  const uint32_t& head(const Edge& e) { return e.head(); }
  const uint32_t& tail(const Edge& e) { return e.tail(); }
  uint32_t& head(Edge& e) { return e.head(); }
  uint32_t& tail(Edge& e) { return e.tail(); }


  // predefined standards
  typedef std::list<Edge> EdgeList;
  typedef std::vector<Edge> EdgeVec;
  typedef std::deque<Edge> EdgeQueue;

  // an adjacency for storing weights on edges
  //NOTE: when cast to uint32_t, Adjacencies must yield their vertex index
  typedef EdgeWithData<float> WEdge;
  typedef std::list<WEdge> WEdgeList;
  typedef std::vector<WEdge> WEdgeVec;
  typedef std::deque<WEdge> WEdgeQueue;

  template<class _Data>
  std::ostream& operator<<(std::ostream& os, const EdgeWithData<_Data>& e)
  {
    return os << Edge(e) << "{"<<e.data<<"}";
  }

}
