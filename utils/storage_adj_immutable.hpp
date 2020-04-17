

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
    using MutabilityTag = mutable_tag;
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
      const size_t num_nodes = deg.size();
     // reserve space for children
      auto nh_start = _succ_storage.begin();
      auto rev_nh_start = _pred_storage.begin();
      for(uintptr_t u = 0; u != num_nodes; ++u) {
        // note: it is important for the initialization of _predecessors & _successors that we go through the nodes in sorted order here
        const auto u_deg = deg.at((Node)u);
        const Degree u_indeg = u_deg.first;
        const Degree u_outdeg = u_deg.second;
        _successors.emplace((Node)u, nh_start, u_outdeg);
        _predecessors.emplace((Node)u, rev_nh_start, u_indeg);
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
      
      std::cout << "computed degrees:\n" << deg << '\n';

      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);

      std::cout << "computed   successors:\n" << _successors <<'\n';
      std::cout << "computed predecessors:\n" << _predecessors <<'\n';

    }

  };


  // tree storages may assume that each node has at most one predecessor, so we can use vector_map<Node> as _PredecessorMap
  template<class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutiveTreePredecessorMap<_EdgeData>>
  class _ConsecutiveTreeAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using MutabilityTag = immutable_tag;
    using Parent::Parent;
    using typename Parent::Edge;
    using typename Parent::Adjacency;

    template<class T>
    using NodeMap = ConsecutiveMap<Node,T>;

  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

    // storage for the adjacencies; _successors will point into this
    ConsecutiveStorage<Adjacency> _succ_storage;


    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& deg)
    {
      auto nh_start = _succ_storage.begin();
      for(const auto& u_deg: deg){
        const auto u = u_deg.first;
        const uint32_t u_outdeg = u_deg.second.second;
        _successors.emplace(u, nh_start, u_outdeg);
        nh_start += u_outdeg;
      }
      // put the edges
      for(const auto &uv: given_edges){
        const Node u = uv.tail();
        const Node v = uv.head();

        //*(&(_successors[tail(edge)]) + (--out_deg[tail(edge)])) = head(edge);
        Adjacency* const position = _successors.at(u).begin() + (--deg.at(u).second);
        new(position) Adjacency(uv.get_adjacency());

        if(!append(_predecessors, v, get_reverse_adjacency(u, *position)).second)
          throw std::logic_error("cannot create tree with reticulations");
      }
      _size = given_edges.size();
    }

  public:
    size_t in_degree(const Node u) const { return (u == _root) ? 0 : 1; }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    _ConsecutiveTreeAdjacencyStorage(const GivenEdgeContainer& given_edges,
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
           class _EdgeStorage>
  class AddConsecutiveNodeData: public _EdgeStorage
  {
    using Parent = _EdgeStorage;
    ConsecutiveMap<Node, _NodeData> node_data;
  public:
    using NodeData = _NodeData;
    using Parent::Parent;

    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    AddConsecutiveNodeData(const GivenEdgeContainer& given_edges,
                           NodeTranslation* old_to_new = nullptr,
                           LeafContainer* leaves = nullptr):
      Parent(given_edges, old_to_new, leaves)
    {
      node_data.resize(Parent::num_nodes());
    }

    const NodeData& get_node_data(const Node u) const { return node_data[u]; }
    NodeData& get_node_data(const Node u) { return node_data[u]; }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    NodeData& operator[](const Node u) { return get_node_data(u); }
  };

  template<class _EdgeStorage>
  class AddConsecutiveNodeData<void, _EdgeStorage>: public _EdgeStorage
  {
    public:
      using _EdgeStorage::_EdgeStorage;
  };



  template<class _NodeData = void,
           class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutivePredecessorMap<_EdgeData>>
  using ConsecutiveNetworkAdjacencyStorage = AddConsecutiveNodeData<_NodeData, _ConsecutiveNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>>;

  template<class _NodeData = void,
           class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutiveTreePredecessorMap<_EdgeData>>
  using ConsecutiveTreeAdjacencyStorage = AddConsecutiveNodeData<_NodeData, _ConsecutiveTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>>;


}

