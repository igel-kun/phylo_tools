
// this defines storages for edges to be used in trees & networks
// immutable edge storage can be initialized and queried, but not changed
// mutable edge storage can also be changed, but are slower


#pragma once

#include "iter_bitset.hpp"
#include "set_interface.hpp"
#include "edge.hpp"
#include "pair_iter.hpp"
#include "storage.hpp"
#include "storage_common.hpp"

namespace PT{

  template<class _EdgeContainer>
  class RootedEdgeStorage
  {
  public:
    using EdgeContainer = _EdgeContainer;
    using Edge = typename _EdgeContainer::value_type;
    using Node = typename Edge::Node;
    using OutEdgeMap = std::unordered_map<Node, EdgeContainer>;

    using OutEdgeContainer = EdgeContainer&;
    using ConstOutEdgeContainer = const EdgeContainer&;

    using SuccContainer = SecondFactory<EdgeContainer>;
    using ConstSuccContainer = ConstSecondFactory<EdgeContainer>;

  protected:
    OutEdgeMap _out_edges;
    Node _root;
    size_t _size;

  public:

    // =============== query ===================

    size_t size() const { return _size; }
    size_t num_edges() const { return size(); }
    Node root() const { return _root; }  
    size_t out_degree(const Node u) const { return out_edges(u).size(); }

    ConstSuccContainer successors(const Node u) const { return const_heads(out_edges(u)); }
    ConstSuccContainer const_successors(const Node u) const { return const_heads(out_edges(u)); }

    OutEdgeContainer out_edges(const Node u) { return map_lookup(_out_edges, u); }
    ConstOutEdgeContainer out_edges(const Node u) const { return map_lookup(_out_edges, u); }
    ConstOutEdgeContainer const_out_edges(const Node u) const { return map_lookup(_out_edges, u); }

    
    // ================= modification =================

  };

}// namespace
