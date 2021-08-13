#pragma once

#include "adjacency.hpp"

namespace PT{

  // this tag allows creating an edge u-->v from an existing adjacency v-->u
  struct reverse_edge_t {};

  // an edge is a node (head) with another node (->tail)
  template<class EdgeData = void>
  struct ProtoEdge: public std::pair<NodeDesc, PT::Adjacency<EdgeData>>
  {
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
  struct Edge: public ProtoEdge<EdgeData>
  {
    using Parent = ProtoEdge<EdgeData>;
    using Parent::Parent;
    EdgeData& get_data() const { return this->second.get_data(); }
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

}

/*
namespace std{
  template<class PS, class SS, class ND, class ED>
  struct hash<PT::Edge<PS, SS, ND, ED>>: public hash<pair<void*, void*>> {};
}
*/
/*
  struct hash<PT::Edge<PS, SS, ND, ED> >{
    size_t operator()(const PT::Edge<PS, SS, ND, ED>& e) const {
      const hash<PT::NodePair<PS,SS,ND,ED> hasher;
      return hasher(e);
    }
  };
}
*/

