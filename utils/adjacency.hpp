
#pragma once

#include "types.hpp"

namespace PT {

  struct ProtoAdjacency {
    NodeDesc nd;

    operator NodeDesc&() { return nd; }
    operator const NodeDesc&() const { return nd; }
    bool operator==(const NodeDesc& other) const { return nd == other; }
    bool operator!=(const NodeDesc& other) const { return nd != other; }
  };

  template<class EdgeData> struct Edge;

  // an adjacency is a node-description with edgedata
  // NOTE: the adjacency's edge data can be either constructed from arguments (new() will be called by the constructor) or from another adjacency (shallow copy of the pointer)
  template<class _EdgeData>
  struct Adjacency: public ProtoAdjacency {
    using Parent = ProtoAdjacency;
    using EdgeData = _EdgeData;
    using Edge = PT::Edge<_EdgeData>;
    static constexpr bool has_data = true;

    // edge data is stored in memory and both u-->v and v-->u point to the same edge data. This pointer cannot change during the lifetime of the adjacency!
    // IMPORTANT: this also means that we cannot be responsable for destroying the EdgeData! This responsability falls to whoever removes the edge
    EdgeData* data_ptr = nullptr;
    
    void free_edge_data() { delete data_ptr; }

    Adjacency(const Adjacency&) = default;
    Adjacency(Adjacency&&) = default;

    template<class __EdgeData> requires (!std::is_void_v<__EdgeData> && !std::is_same_v<EdgeData,__EdgeData>)
    Adjacency(const Adjacency<__EdgeData>& adj): Parent(adj), data_ptr(new EdgeData(*(adj.data_ptr))) {}
    // make from an iterator to an adjacency
    template<class AdjIter> requires requires(AdjIter i) { { *i } -> std::convertible_to<Adjacency>; }
    Adjacency(const AdjIter& iter): Adjacency(*iter) {}

    template<class First, class... Args> requires (!std::is_same_v<std::remove_cvref_t<First>, Adjacency>)
    Adjacency(const NodeDesc _nd, First&& first, Args&&... args): Parent(_nd), data_ptr(new EdgeData(first, std::forward<Args>(args)...)) {}
    Adjacency(const NodeDesc _nd, const Adjacency& adj): Parent(_nd), data_ptr(adj.data_ptr) {}
    Adjacency(const NodeDesc _nd): Parent(_nd) {}

    Adjacency& operator=(const Adjacency&) = default;
    Adjacency& operator=(Adjacency&&) = default;

    EdgeData& data() const { assert(data_ptr); return *data_ptr; }
  };

  template<>
  struct Adjacency<void>: public ProtoAdjacency {
    using Parent = ProtoAdjacency;
    using Parent::Parent;
    using Edge = PT::Edge<void>;
    static constexpr bool has_data = false;

    Adjacency(const Adjacency&) = default;
    Adjacency(Adjacency&&) = default;

    // make from an iterator to an adjacency
    template<class AdjIter> requires requires(AdjIter i) { { *i } -> std::convertible_to<Adjacency>; }
    Adjacency(const AdjIter& iter): Adjacency(*iter) {}

    template<class... Args>
    Adjacency(const NodeDesc& _nd, Args&&... args): Parent(_nd) {}

    Adjacency& operator=(const Adjacency&) = default;
    Adjacency& operator=(Adjacency&&) = default;

    void free_edge_data() const {}
  };



  template<std::Printable T>
  std::ostream& operator<<(std::ostream& os, const Adjacency<T>& a) {
    return os << a.nd << '[' << *(a.data_ptr) << ']';
  }
  template<class T> requires (!std::Printable<T>)
  std::ostream& operator<<(std::ostream& os, const Adjacency<T>& a) {
    if constexpr(Adjacency<T>::has_data) {
      return os << a.nd << '[' << '@' << a.data_ptr << ']';
    } else return os << a.nd;
  }

}

