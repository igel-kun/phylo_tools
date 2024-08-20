#pragma once

#include "types.hpp"
#include "tags.hpp"
#include "adjacency.hpp"

namespace PT{

  // an edge is a node (head) with another node (->tail)
  template<class EdgeData = void> //requires std::is_default_constructible_v<PT::Adjacency<EdgeData>>
  struct ProtoEdge: public std::pair<NodeDesc, PT::Adjacency<EdgeData>> {
    using Data = EdgeData;
    using Adjacency = PT::Adjacency<EdgeData>;
    using Parent = std::pair<NodeDesc, Adjacency>;
    using Parent::Parent;
    static constexpr bool has_data = Adjacency::has_data;

    ProtoEdge(const reverse_edge_tag, const NodeDesc u, const Adjacency& v):
      Parent(v.get_desc(), Adjacency{u, v})
    {}

    const Adjacency& head() const & { return this->second; }
    Adjacency& head() & { return this->second; }
    Adjacency&& head() && { return std::move(this->second); }
    NodeDesc tail() const { return this->first; }
    NodePair as_pair() const { return { this->first, this->second }; }
    bool is_invalid() const { return tail() == NoNode; }
  };

  template<class EdgeData>
  struct Edge: public ProtoEdge<EdgeData> {
    using Parent = ProtoEdge<EdgeData>;
    using Parent::Parent;
    EdgeData& data() const & { return this->second.data(); }
    EdgeData&& data() && { return std::move(this->second.data()); }
  };
  template<>
  struct Edge<void>: public ProtoEdge<void> {
    using Parent = ProtoEdge<void>;
    using Parent::Parent;
  };

  template<NodeType Node>
  using EdgeFromNode = Edge<typename Node::EdgeData>;
  template<class EdgeData = void>
  using EdgeVec = std::vector<Edge<EdgeData>>;
  template<class EdgeData = void>
  using EdgeSet = std::unordered_set<Edge<EdgeData>>;

/*
  // an Edge is something whose head and tail are Nodes
  template<class T>
  concept EdgeType = requires(T e) {
    { e.head() } -> std::convertible_to<NodeDesc>;
    { e.tail() } -> std::convertible_to<NodeDesc>;
  };
*/
  template<class T>
  concept StrictEdgeType = mstd::is_derived_from_template_v<T, ProtoEdge>;
  template<class T>
  concept EdgeType = StrictEdgeType<std::remove_cvref_t<T>>;

  // a 'loose' edge type is either an edge or a pair of AdjacencyTypes
  template<class T>
  concept LooseEdgeType = EdgeType<T> || AdjPairType<T>;

  template<class F, class Edge>
  concept EdgeFunctionType = LooseEdgeType<Edge> && std::invocable<F, Edge>;
  template<class F, class Edge>
  concept OptionalEdgeFunctionType = EdgeFunctionType<F, Edge> || std::is_void_v<F>;

  template<class T>
  concept EdgeIterableType = (mstd::IterableType<T> && EdgeType<typename T::value_type>);
  template<class T>
  concept EdgeContainerType = (mstd::ContainerType<T> && EdgeType<typename T::value_type>);


}

namespace std{
  template<class EdgeData>
  struct hash<PT::Edge<EdgeData>> {
    hash<pair<uintptr_t, uintptr_t>> pair_hash;
    size_t operator()(const PT::Edge<EdgeData>& edge) const { return pair_hash(edge.as_pair()); }
  };
}


