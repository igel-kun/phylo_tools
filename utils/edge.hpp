#pragma once

#include "types.hpp"
#include "tags.hpp"
#include "adjacency.hpp"

namespace PT{

  // an edge is a node (head) with another node (->tail)
  template<class EdgeData = void> //requires std::is_default_constructible_v<PT::Adjacency<EdgeData>>
  struct ProtoEdge:
    public std::pair<NodeDesc, PT::Adjacency<EdgeData>>
  {
    using Adjacency = PT::Adjacency<EdgeData>;
    using Data = EdgeData;
    static constexpr bool has_data = Adjacency::has_data;
    
    using Parent = std::pair<NodeDesc, Adjacency>;
    using Parent::Parent;

    ProtoEdge(const reverse_edge_tag, const NodeDesc u, const Adjacency& v):
      Parent(v.get_desc(), Adjacency{u, v})
    {}
    ProtoEdge(const reverse_edge_tag, const Parent& uv):
      Parent(uv.second.get_desc(), Adjacency{uv.first, uv.second})
    {}
    ProtoEdge(const Adjacency& u, const NodeDesc v):
      Parent(u.get_desc(), Adjacency{v, u})
    {}
    ProtoEdge(const NodeDesc u, const NodeDesc v) requires (std::is_default_constructible_v<EdgeData>):
      Parent(u, v)
    {}

    const Adjacency& head() const & { return this->second; }
    Adjacency& head() & { return this->second; }
    Adjacency&& head() && { return std::move(this->second); }
    NodeDesc tail() const { return this->first; }
    NodePair as_pair() const { return { this->first, this->second }; }
    bool is_invalid() const { return tail() == NoNode; }

    ProtoEdge get_reversed() const { return ProtoEdge{reverse_edge_tag{}, Parent::first, Parent::second}; }
    Adjacency tail_with_data() const { return Adjacency{Parent::first, Parent::second}; }
  };

  template<class EdgeData = void>
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
  template<class T, class EdgeData = void>
  using EdgeMap = std::unordered_map<Edge<EdgeData>, T>;

  template<class T>
  concept StrictEdgeType = mstd::is_derived_from_template_v<T, ProtoEdge>;
  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept EdgeType = mstd::apply_rune_v<T, rune> or StrictEdgeType<mstd::apply_rune_t<T, rune>>;

  // a 'loose' edge type is either an edge or a pair of AdjacencyTypes
  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept LooseEdgeType = EdgeType<T, rune> or AdjPairType<T, rune>;

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


