

#pragma once

#include "storage_adj_common.hpp"

namespace PT{
  // by default, save the edge data in the successor map and provide a reference in each "reverse adjacency" of the predecessor map
  template<class EdgeData>
  using DefaultConsecutiveSuccessorMap = ConsecutiveMap<Node, ConsecutiveStorageNoMem<Adjacency_t<EdgeData>>>;
  template<class EdgeData>
  using DefaultConsecutivePredecessorMap = DefaultConsecutiveSuccessorMap<DataReference<EdgeData>>;
  template<class EdgeData>
  using DefaultConsecutiveTreePredecessorMap = ConsecutiveMap<Node, std::SingletonSet<ReverseAdjacencyFromData<EdgeData>>>;


  template<class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutivePredecessorMap<_EdgeData>>
  class _ConsecutiveNetworkAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Mutability_Tag = mutable_tag;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using typename Parent::RevEdge;
    using typename Parent::RevAdjacency;

    template<class T>
    using NodeMap = ConsecutiveMap<Node,T>;

  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

    // storage for the successors; _successors will point into this
    ConsecutiveStorage<Adjacency> _succ_storage;
    // storage for the predecessors; _predecessors will point into this
    ConsecutiveStorage<RevAdjacency> _pred_storage;


    // prepare the container and insert a list of edges with given degrees
    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& deg)
    {
     // reserve space for children
      auto nh_start = _succ_storage.begin();
      auto rev_nh_start = _pred_storage.begin();
      std::cout << "inserting edges\n";
      for(const auto& u_deg: deg){
        const auto u = u_deg.first;
        const Degree u_indeg = u_deg.second.first;
        const Degree u_outdeg = u_deg.second.second;
        _successors.emplace(u, nh_start, u_outdeg);
        std::cout << "emplaced space for "<<u_outdeg<<" successors of "<<u<<" at "<<nh_start<<"\n";
        _predecessors.emplace(u, rev_nh_start, u_indeg);
        nh_start += u_outdeg;
        rev_nh_start += u_indeg;
      }
      // put the in- and out-edges
#warning TODO: here, the edge data is copied! Make a version where it is moved instead!
      for(const auto &uv: given_edges){
        const Node u = uv.tail();
        const Node v = uv.head();

        Adjacency* const position = _successors.at(u).begin() + (--deg.at(u).second);
        DEBUG5(std::cout << "constructing adjacency "<<uv.get_adjacency()<<" at "<<position<<std::endl);
        new(position) Adjacency(uv.get_adjacency());
        
        RevAdjacency* const rev_position = &(_predecessors.at(v)[--deg.at(v).first]);
        DEBUG5(std::cout << "constructing reverse adjacency "<< get_reverse_adjacency(u, *position)<<" at "<<rev_position<<" from adjacency "<<*position<<std::endl);
        new(rev_position) RevAdjacency(get_reverse_adjacency(u, *position));
      }
      _size = given_edges.size();
    }

  public:
    //! initialization from edgelist without consecutive nodes; as we are a consecutive storage, we will have to translate node indices, so use 'old_to_new'
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    _ConsecutiveNetworkAdjacencyStorage(const GivenEdgeContainer& given_edges,
                                        NodeTranslation* old_to_new = nullptr,
                                        LeafContainer* leaves = nullptr):
      _succ_storage(given_edges.size()),
      _pred_storage(given_edges.size())
    {
      DEBUG3(std::cout << "initializing ConsecutiveNetworkAdjacencyStorage with leaf storage at "<<leaves<<" and "<<given_edges.size()<<" consecutive edges:\n" << given_edges<<std::endl);
      InOutDegreeMap deg;
      compute_degrees(given_edges, deg, old_to_new);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

  };


  template<class _NodeData,
           class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutivePredecessorMap<_EdgeData>>
  class ConsecutiveNetworkAdjacencyStorage: public _ConsecutiveNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _ConsecutiveNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
    ConsecutiveMap<Node, _NodeData> node_data;
  public:
    using NodeData = _NodeData;

    using Parent::Parent;

    const NodeData& get_node_data(const Node u) const { return node_data[u]; }
    NodeData& get_node_data(const Node u) { return node_data[u]; }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    NodeData& operator[](const Node u) { return get_node_data(u); }
  };

  template<class _EdgeData,
           class _SuccessorMap,
           class _PredecessorMap>
  class ConsecutiveNetworkAdjacencyStorage<void, _EdgeData, _SuccessorMap, _PredecessorMap>:
      public _ConsecutiveNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _ConsecutiveNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Parent::Parent;
  };



  // tree storages may assume that each node has at most one predecessor, so we can use vector_map<Node> as _PredecessorMap
  template<class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutiveTreePredecessorMap<_EdgeData>>
  class _ConsecutiveTreeAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Mutability_Tag = immutable_tag;
    using Parent::Parent;
    using typename Parent::Edge;
    using typename Parent::Adjacency;

    template<class T>
    using NodeMap = std::vector_map<Node,T>;

  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

    // storage for the adjacencies; _successors will point into this
    ConsecutiveStorage<Adjacency> _succ_storage;


    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& degrees)
    {
      auto nh_start = _succ_storage.begin();
      for(const auto& u_deg: degrees){
        const auto u = u_deg.first;
        const uint32_t u_outdeg = u_deg.second.second;
        _successors.emplace(u, nh_start, u_outdeg);
        nh_start += u_outdeg;
      }
      // put the edges
      for(const auto &uv: given_edges){
        //*(&(_successors[tail(edge)]) + (--out_deg[tail(edge)])) = head(edge);
        auto* position = &(_successors[uv.tail()]) + --(degrees.at(uv.tail()).second);
        new(position) Adjacency(uv.get_adjacency());

        if(!append(_predecessors, uv.head(), *position).second)
          throw std::logic_error("cannot create tree with reticulations");
      }
      _size = given_edges.size();
    }

  public:
    size_t in_degree(const Node u) const { return (u == _root) ? 0 : 1; }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = NodeVec>
    _ConsecutiveTreeAdjacencyStorage(const consecutive_tag tag,
                                     const GivenEdgeContainer& given_edges,
                                     NodeTranslation* old_to_new = nullptr,
                                     LeafContainer* leaves = nullptr):
      _succ_storage(given_edges.size())
    {
      DEBUG3(std::cout << "initializing ConsecutiveTreeAdjacencyStorage with "<<given_edges.size()<<" edges & leaf storage "<<leaves<<std::endl);
      InOutDegreeMap deg;
      compute_degrees(given_edges, deg, old_to_new);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

  };


  template<class _NodeData,
           class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutivePredecessorMap<_EdgeData>>
  class ConsecutiveTreeAdjacencyStorage: public _ConsecutiveTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _ConsecutiveTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
    ConsecutiveMap<Node, _NodeData> node_data;
  public:
    using NodeData = _NodeData;

    using Parent::Parent;

    const NodeData& get_node_data(const Node u) const { return node_data[u]; }
    NodeData& get_node_data(const Node u) { return node_data[u]; }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    NodeData& operator[](const Node u) { return get_node_data(u); }
  };

  template<class _EdgeData,
           class _SuccessorMap,
           class _PredecessorMap>
  class ConsecutiveTreeAdjacencyStorage<void, _EdgeData, _SuccessorMap, _PredecessorMap>:
      public _ConsecutiveTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _ConsecutiveTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Parent::Parent;
  };


}

