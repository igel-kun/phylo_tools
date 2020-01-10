
#pragma once

#include "storage_edge_common.hpp"

namespace PT{

  template<class _Edge = Edge<> >
  class NonGrowingRootedEdgeStorage: public RootedEdgeStorage<ConsecutiveStorageNoMem<_Edge> >
  {
    using Parent = RootedEdgeStorage<ConsecutiveStorageNoMem<_Edge> >;
  public:
    using typename Parent::EdgeContainer;
    using typename Parent::Edge;
    using typename Parent::Node;
    using typename Parent::OutEdgeMap;

    using Edges = ConsecutiveStorage<_Edge>;
    using iterator = typename Edges::iterator;
    using const_iterator = typename Edges::const_iterator;
  protected:
    Edges _edges;

  public:
    
    size_t size() const { return _edges.size(); }
    const Edges& edges() const { return _edges; }

    // ================= modification =================

    bool add_edge(const _Edge& e) { throw(std::logic_error("cannot add an edge to a non-growing storage")); }

    // =============== iteration =====================

    iterator begin() { return _edges.begin(); }
    iterator end() { return _edges.end(); }
    const_iterator begin() const { return _edges.begin(); }
    const_iterator end() const { return _edges.end(); }

  };

  
  template<class _Edge = Edge<> >
  class NonGrowingTreeEdgeStorage: public NonGrowingRootedEdgeStorage<_Edge>
  {
    using Parent = NonGrowingRootedEdgeStorage<_Edge>;
  public:
    using Parent::Parent;
    using typename Parent::EdgeContainer;
    using typename Parent::Node;
    using typename Parent::Edge;

    using RevEdge = std::reference_wrapper<Edge>;
    using ConstRevEdge = std::reference_wrapper<const Edge>;

    using InEdgeContainer = std::list<RevEdge>;
    using ConstInEdgeContainer = std::list<ConstRevEdge>;
    using ConstPredContainer = std::list<const Node>;
    
  protected:
    using Parent::_no_edges;
    using Parent::_out_edges;
    using Parent::_edges;
    using Parent::_root;
    using Parent::_size;
    
    std::unordered_map<Node, RevEdge> _in_edges;

  protected:

    // prepare the container and insert a list of edges with given degrees
    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& deg)
    {
      // compute children
      auto nh_start = _edges.begin();
      for(const auto& u_deg: deg){
        const auto u = u_deg.first;
        const uint32_t u_outdeg = u_deg.second.second;
        _out_edges.DEEP_EMPLACE_TWO(u, nh_start, u_outdeg);
        nh_start += u_outdeg;
      }
      // put the edges
      for(const auto &uv: given_edges){
        const auto* position = &(_out_edges[uv.tail()]) + --(deg.at(uv.tail()).second);
        new(position) Edge(uv);

        if(!append(_in_edges, uv.head(), *position).second)
          throw std::logic_error("cannot create tree with reticulations");
      }
      _size = given_edges.size();
    }

  public:

    uint32_t num_nodes() const { return _in_edges.size() + 1; }

    void remove_node(const Node u)
    {
      Parent::remove_node(u);
      _out_edges.erase(u);
      _in_edges.erase(u);
    }

    InEdgeContainer in_edges(const Node u)
    {
      return (u == _root) ? EdgeContainer({}) : EdgeContainer({_in_edges.at(u)});
    }

    ConstInEdgeContainer in_edges(const Node u) const
    {
      return (u == _root) ? EdgeContainer({}) : EdgeContainer({_in_edges.at(u)});
    }

    ConstPredContainer predecessors(const Node u) const { return (u == _root) ? ConstPredContainer({}) : ConstPredContainer({_in_edges.at(u).tail()}); }

    //! initialization from edgelist with consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingTreeEdgeStorage(const consecutive_edgelist_tag& tag,
                              const GivenEdgeContainer& given_edges,
                              const uint32_t _num_nodes,
                              LeafContainer* leaves = nullptr)
    {
      std::vector_map<UIntPair> deg(_num_nodes);
      compute_degrees(given_edges, deg, _num_nodes);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingTreeEdgeStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr)
    {
      std::unordered_map<Node, UIntPair> deg;
      compute_degrees_and_nodes(given_edges, deg, nodes);
      _root = compute_root_and_leaves(deg, leaves);
      append(nodes, _root); // don't forget to append the _root
      insert_edges(given_edges, deg);
    }

  };


  template<class _Edge = Edge<> >
  class NonGrowingNetworkEdgeStorage: public NonGrowingRootedEdgeStorage<_Edge>
  {
    using Parent = NonGrowingRootedEdgeStorage<_Edge>;
  public:
    using Parent::Parent;
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::EdgeContainer;

    using RevEdge = std::reference_wrapper<Edge>;
    using ConstRevEdge = std::reference_wrapper<const Edge>;

    using InEdgeContainer = ConsecutiveStorage<RevEdge>;
    using ConstInEdgeContainer = ConsecutiveStorage<ConstRevEdge>;

    using InEdges   = ConsecutiveStorageNoMem<RevEdge>;
    using ConstInEdges = ConsecutiveStorageNoMem<ConstRevEdge>;

    using InEdgeMap = std::unordered_map<Node, InEdges>;
    using ConstInEdgeMap = const std::unordered_map<Node, ConstInEdges>;
    
    using PredContainer = FirstFactory<EdgeContainer>;
    using ConstPredContainer = ConstFirstFactory<EdgeContainer>;
   
  protected:
    using Parent::_edges;
    using Parent::_out_edges;
    using Parent::_root;
    using Parent::_size;

    InEdgeContainer _rev_edges;
    InEdgeMap _in_edges;

    // prepare the container and insert a list of edges with given degrees
    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& deg)
    {
     // reserve space for children
      auto nh_start = _edges.begin();
      auto rev_nh_start = _rev_edges.begin();
      for(const auto& u_deg: deg){
        const auto u = u_deg.first;
        const uint32_t u_indeg = u_deg.second.first;
        const uint32_t u_outdeg = u_deg.second.second;
        _out_edges.DEEP_EMPLACE_TWO(u, nh_start, u_outdeg);
        _in_edges.DEEP_EMPLACE_TWO(u, rev_nh_start, u_indeg);
        nh_start += u_outdeg;
        rev_nh_start += u_indeg;
      }
      // put the in- and out-edges 
      for(const auto &uv: given_edges){
        const Edge* position = &(_out_edges[uv.tail()][--deg.at(uv.tail()).second]);
        new(position) Edge(uv);
        
        const Edge* rev_position = &(_in_edges[uv.head()][--deg.at(uv.head()).first]);
        new(rev_position) RevEdge(*position);
      }
      _size = given_edges.size();
    }

  public:
    
    uint32_t num_nodes() const { return _in_edges.size() + 1; }

    PredContainer predecessors(const Node u) { return PredContainer(in_edges(u)); }
    ConstPredContainer predecessors(const Node u) const { return ConstPredContainer(in_edges(u)); }

    InEdgeContainer in_edges(const Node u) { return map_lookup(_in_edges, u); }
    ConstInEdgeContainer in_edges(const Node u) const { return map_lookup(_in_edges, u); }

    size_t in_degree(const Node u) const { return in_edges(u).size(); }
 
    void remove_node(const Node u)
    {
      Parent::remove_node(u);
      predecessors(u).clear();
    }

    //! initialization from edgelist with consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingNetworkEdgeStorage(const consecutive_edgelist_tag& tag,
                              const GivenEdgeContainer& given_edges,
                              const uint32_t _num_nodes,
                              LeafContainer* leaves = nullptr)
    {
      std::vector_map<UIntPair> deg(_num_nodes);
      compute_degrees(given_edges, deg, _num_nodes);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingNetworkEdgeStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr)
    {
      std::unordered_map<Node, UIntPair> deg;
      compute_degrees_and_nodes(given_edges, deg, nodes);
      _root = compute_root_and_leaves(deg, leaves);
      append(nodes, _root); // don't forget to append the _root
      insert_edges(given_edges, deg);
    }

  };



}// namespace

