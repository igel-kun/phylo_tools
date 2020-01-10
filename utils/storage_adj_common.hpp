
#pragma once

#include "edge.hpp"
#include "edge_iter.hpp"
#include "storage.hpp"
#include "storage_common.hpp"
#include "pair_iter.hpp"
#include "adj_iter.hpp"
#include "set_interface.hpp"

#warning TODO: implement move constructors for edge storages and adjacency storages!!!
namespace PT{

  template<class _Edge, class _AdjContainer>
  class RootedAdjacencyStorage
  {
  public:
    using AdjContainer = _AdjContainer;
    using Edge = _Edge;
    using Node = typename _Edge::Node;
    using Adjacency = typename _AdjContainer::value_type;
    using SuccessorMap = std::unordered_map<Node, AdjContainer>;

    using OutEdgeContainer =      OutEdgeFactory<Edge, AdjContainer>;
    using ConstOutEdgeContainer = OutEdgeConstFactory<Edge, const AdjContainer>;
    using SuccContainer =      typename GetFirstFactory<typename AdjContainer::value_type, AdjContainer>::Factory;
    using ConstSuccContainer = typename GetConstFirstFactory<typename AdjContainer::value_type, AdjContainer>::Factory;
    using EdgeContainer =      AdjacencyIterFactory<SuccessorMap, AdjacencyConstIterator<SuccessorMap> >;
    using ConstEdgeContainer = AdjacencyIterFactory<const SuccessorMap, AdjacencyConstIterator<const SuccessorMap> >;

  protected:
    SuccessorMap _successors;
    Node _root;
    size_t _size;

    static const AdjContainer no_successors;
  public:
  
    RootedAdjacencyStorage(): _size(0) {}

    // =============== query ===================

    size_t size() const { return _size; }
    size_t num_edges() const { return size(); }
    Node root() const { return _root; }

    size_t out_degree(const Node u) const { return successors(u).size(); }

    ConstSuccContainer successors(const Node u) const { return_map_lookup(_successors, u, no_successors); }
    ConstSuccContainer const_successors(const Node u) const { return_map_lookup(_successors, u, no_successors); }
    SuccContainer successors(const Node u) { return_map_lookup(_successors, u, SuccContainer()); }

    ConstOutEdgeContainer out_edges(const Node u) const { return {u, successors(u)}; }
    ConstOutEdgeContainer const_out_edges(const Node u) const { return {u, successors(u)}; }
    OutEdgeContainer out_edges(const Node u) { return {u, successors(u)}; }

    ConstEdgeContainer edges() const { return ConstEdges(_successors); }
    EdgeContainer edges() { return Edges(_successors); }

    // ================= modification =================
  };

  template<class _Edge, class _AdjContainer>
  const typename RootedAdjacencyStorage<_Edge, _AdjContainer>::AdjContainer RootedAdjacencyStorage<_Edge, _AdjContainer>::no_successors;


}// namespace

