#pragma once

#include "adjacency.hpp"

namespace PT{

  // an edge is a node (head) with another node (->tail)
  template<class EdgeData = void>
  struct ProtoEdge: public std::pair<NodeDesc, PT::Adjacency<EdgeData>> {
    using Data = EdgeData;
    using Adjacency = PT::Adjacency<EdgeData>;
    using Parent = std::pair<NodeDesc, Adjacency>;
    using Parent::Parent;
    static constexpr bool has_data = Adjacency::has_data;

    ProtoEdge(const reverse_edge_t, const NodeDesc u, const Adjacency& v):
      Parent(v.nd, {u, v})
    {}

    const Adjacency& head() const { return this->second; }
    Adjacency& head() { return this->second; }
    NodeDesc tail() const { return this->first; }
    std::pair<NodeDesc, NodeDesc> as_pair() const { return { this->first, this->second }; }
  };

  template<class EdgeData>
  struct Edge: public ProtoEdge<EdgeData> {
    using Parent = ProtoEdge<EdgeData>;
    using Parent::Parent;
    EdgeData& data() const { return this->second.data(); }
  };
  template<>
  struct Edge<void>: public ProtoEdge<void>
  {
    using Parent = ProtoEdge<void>;
    using Parent::Parent;
  };

  template<NodeType Node>
  using EdgeFromNode = Edge<typename Node::EdgeData>;
  template<class EdgeData = void>
  using EdgeVec = std::vector<Edge<EdgeData>>;
  template<class EdgeData = void>
  using EdgeSet = std::unordered_set<Edge<EdgeData>>;

  template<class F, class Edge>
  concept EdgeFunctionType = std::invocable<F, Edge>;

}

namespace std{
  template<class EdgeData>
  struct hash<PT::Edge<EdgeData>> {
    hash<pair<uintptr_t, uintptr_t>> pair_hash;
    size_t operator()(const PT::Edge<EdgeData>& edge) const { return pair_hash(edge.as_pair()); }
  };
}


