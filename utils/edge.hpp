#pragma once

#include "utils.hpp"
#include "pair_iter.hpp"
#include <queue>

namespace PT{

  // an edge is a node (head) with an adjacency (->tail), the latter might have a weight and more information attached to it
  template<typename _Node = uint32_t>
  class Edge: public std::pair<_Node, _Node>
  {
    using Parent = std::pair<_Node, _Node>;
  public:
    using Node = _Node;
    using Adjacency = Node;
    using Parent::Parent;

    Node& head() { return this->second; }
    Node& tail() { return this->first; }
    const Node& head() const { return this->second; }
    const Node& tail() const { return this->first; }

    bool operator<(const Edge& e) const
    {
      if(this->second == e.second)
        return this->first < e.first;
      else 
        return this->second < e.second;
    }

    Adjacency get_adjacency() const
    {
      return head();
    }
  };

  template<class _Data = void*, typename _Node = uint32_t>
  class EdgeWithData: public Edge<_Node>
  {
    using Parent = Edge<_Node>;
  public:
    using Adjacency = std::pair<_Node, _Data>;
    using Parent::Parent;
    using Parent::head;
    using Parent::tail;
    using typename Parent::Node;
    _Data data;
    
    EdgeWithData(const Edge<_Node>& e, const _Data& _data):
      Parent(e),
      data(_data)
    {}
    EdgeWithData(const Node u, const Adjacency& adj):
      Parent(u, adj.first),
      data(adj.second)
    {}
    EdgeWithData(const Adjacency& adj, const Node v):
      Parent(adj.first, v),
      data(adj.second)
    {}
    EdgeWithData(const std::pair<const Node, const Adjacency>& p):
      EdgeWithData(p.first, p.second)
    {}
    EdgeWithData(const std::pair<const Adjacency, const Node>& p):
      EdgeWithData(p.first, p.second)
    {}
    EdgeWithData(const std::pair<const Node, const Adjacency&>& p):
      EdgeWithData(p.first, p.second)
    {}
    EdgeWithData(const std::pair<const Adjacency&, const Node>& p):
      EdgeWithData(p.first, p.second)
    {}

    EdgeWithData():
      Parent()
    {}

    const Adjacency get_adjacency() const
    {
      return {head(), data};
    }
  };

  template<class _Node>
  _Node head(const Edge<_Node>& e) { return e.head(); }
  template<class _Node>
  _Node tail(const Edge<_Node>& e) { return e.tail(); }
  template<class _Node>
  _Node& head(Edge<_Node>& e) { return e.head(); }
  template<class _Node>
  _Node& tail(Edge<_Node>& e) { return e.tail(); }


  // predefined standards
  using EdgeList = std::list<Edge<> >;
  using EdgeVec = std::vector<Edge<> >;
  using EdgeQueue = std::deque<Edge<> >;
  using EdgeSet = std::unordered_set<Edge<> >;

  // an adjacency for storing weights on edges
  using WEdge = EdgeWithData<float>;
  using WEdgeList = std::list<WEdge>;
  using WEdgeVec = std::vector<WEdge>;
  using WEdgeQueue = std::deque<WEdge>;
  using WEdgeSet = std::unordered_set<WEdge>;

}

#define heads(x) seconds(x)
#define tails(x) firsts(x)
#define const_heads(x) const_seconds(x)
#define const_tails(x) const_firsts(x)


namespace std{
  template<class _Data, typename _Node>
  struct hash<PT::EdgeWithData<_Data, _Node> >{
    size_t operator()(const PT::EdgeWithData<_Data, _Node>& e) const{
      const hash<pair<_Node, _Node> > hasher;
      return hasher(e);
    }
  };
  template<typename _Node>
  struct hash<PT::Edge<_Node> >{
    size_t operator()(const PT::Edge<_Node>& e) const{
      const hash<pair<_Node, _Node> > hasher;
      return hasher(e);
    }
  };

}


