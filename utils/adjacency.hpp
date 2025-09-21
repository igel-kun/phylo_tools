
#pragma once

#include "types.hpp"

namespace PT {

  struct ProtoAdjacency {
    NodeDesc nd = NoNode;

    const NodeDesc& get_desc() const { return nd; }
    operator const NodeDesc&() const { return nd; }  // one should never change the node of an adjacency
    bool operator==(const ProtoAdjacency& other) const { return nd == other.nd; }
    bool operator==(const NodeDesc other) const { return nd == other; }
  };

  template<class EdgeData> struct Edge;

  // an adjacency is a node-description with edgedata
  // NOTE: the adjacency's edge data can be either constructed from arguments (new() will be called by the constructor) or from another adjacency (shallow copy of the pointer)
  template<class EdgeData_>
  struct Adjacency: public ProtoAdjacency {
    using Parent = ProtoAdjacency;
    using EdgeData = EdgeData_;
    using Edge = PT::Edge<EdgeData_>;
    static constexpr bool has_data = true;
  
  protected:
#warning "TODO: check if shared_ptr performs"
    std::shared_ptr<EdgeData> data_ptr = nullptr;
  public:
    Adjacency() requires (mstd::is_constructible_v<EdgeData>):
      Parent{NoNode}, data_ptr(std::make_shared<EdgeData>()) {}
 
    Adjacency() requires (not mstd::is_constructible_v<EdgeData>):
      Parent{NoNode}, data_ptr() {}
   
    // make an Adjacency from a different Adjacency by (possibly move-) constructing our data from theirs
    template<class EData> requires (!std::is_void_v<EData> && !std::is_same_v<EdgeData, EData>)
    Adjacency(const Adjacency<EData>& adj):
      Parent{adj},
      data_ptr(std::make_shared<EdgeData>(*(adj.data_ptr)))
    {}
    
    template<class EData> requires (!std::is_void_v<EData> && !std::is_same_v<EdgeData, EData>)
    Adjacency(Adjacency<EData>&& adj):
      Parent{adj},
      data_ptr(std::make_shared<EdgeData>(std::move(*(adj.data_ptr))))
    {}
    
    // make from an iterator to an adjacency
    template<class AdjIter> requires requires(AdjIter i) { { *i } -> std::convertible_to<Adjacency>; }
    Adjacency(const AdjIter& iter): Adjacency(*iter) {}

    template<class First, class... Args> requires (not mstd::is_same_v<First, Adjacency>)
    Adjacency(const NodeDesc _nd, First&& first, Args&&... args):
      Parent(_nd),
      data_ptr(std::make_shared<EdgeData>(std::forward<First>(first), std::forward<Args>(args)...))
    {}
    
    Adjacency(const NodeDesc _nd, const Adjacency& adj):
      Parent{_nd}, data_ptr(adj.data_ptr) {}
    
    Adjacency(const NodeDesc _nd) requires (std::is_constructible_v<EdgeData>):
      Parent{_nd}, data_ptr{std::make_shared<EdgeData>()}
    {}

    EdgeData& data() const { assert(data_ptr); return *data_ptr; }

    friend std::ostream& operator<<(std::ostream& os, const Adjacency& a) {
      if constexpr (mstd::Printable<EdgeData_>) {
        if(a.data_ptr)
          return os << a.nd << '[' << *(a.data_ptr) << ']';
        else
          return os << a.nd << "[@NULL]";
      } else return os << a.nd << '[' << '@' << a.data_ptr << ']';
    }

    // NodeAccess must be able to construct invalid adjacencies in order to report failure when trying to find an edge given by its endpoints
    template<StrictNodeType> friend struct NodeAccess;
  };

  template<>
  struct Adjacency<void>: public ProtoAdjacency {
    using Parent = ProtoAdjacency;
    using Edge = PT::Edge<void>;
    static constexpr bool has_data = false;

    Adjacency() = default;

    // make from an iterator to an adjacency
    template<class AdjIter> requires requires(AdjIter i) { { *i } -> std::convertible_to<Adjacency>; }
    Adjacency(const AdjIter& iter): Adjacency(*iter) {}

    template<class... Args>
    Adjacency(const NodeDesc _nd, Args&&... args): Parent{_nd} {}

    friend std::ostream& operator<<(std::ostream& os, const Adjacency<void>& a) { return os << a.get_desc(); }
  };

  // NOTE: an AdjAdapter can be used to merge edge-data when contracting edges
  //       for example, if we contract an edge uv, and v has a child w,
  //       then the data of uv is "merged with" the data of vw via an AdjAdapter
  template<class T, class Adj, class Phylo>
  concept AdjAdapterType = std::invocable<T, const Adj&, typename Phylo::Adjacency&>;
}

namespace std{
  template<class EdgeData> requires (!std::is_void_v<EdgeData>)
  struct hash<PT::Adjacency<EdgeData>> {
    hash<PT::NodeDesc> node_hash;
    size_t operator()(const PT::Adjacency<EdgeData>& adj) const { return node_hash(adj.get_desc()); }
  };
}


