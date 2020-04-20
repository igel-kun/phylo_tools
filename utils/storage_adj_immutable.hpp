

#pragma once

#include <cassert>
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

    using DegreeMap = NodeMap<InOutDegree>;

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
    template<class GivenEdgeContainer>
    void insert_edges(const GivenEdgeContainer& given_edges, DegreeMap& deg, const NodeTranslation* const old_to_new)
    {
      const size_t num_nodes = deg.size();
     // reserve space for children
      auto nh_start = _succ_storage.begin();
      auto rev_nh_start = _pred_storage.begin();
      for(Node u = 0; u != num_nodes; ++u) {
        // note: it is important for the initialization of _predecessors & _successors that we go through the nodes in sorted order here
        const auto u_deg = deg.at(u);
        std::cout << "node "<<u<<" has deg "<<u_deg<<'\n';
        const Degree u_indeg = u_deg.first;
        const Degree u_outdeg = u_deg.second;
        _successors.try_emplace(u, nh_start, u_outdeg);
        _predecessors.try_emplace(u, rev_nh_start, u_indeg);
        nh_start += u_outdeg;
        rev_nh_start += u_indeg;
      }
      // put the in- and out-edges
#warning TODO: here, the edge data is copied! Make a version where it is moved instead!
      for(const auto &uv: given_edges){
        const Node u = old_to_new ? (*old_to_new).at(uv.tail()) : uv.tail();
        const Node v = old_to_new ? (*old_to_new).at(uv.head()) : uv.head();

        Adjacency* const position = _successors.at(u).begin() + (--deg.at(u).second);
        DEBUG5(std::cout << "constructing adjacency "<<uv.get_adjacency()<<" at "<<position<<std::endl);
        new(position) Adjacency(uv.get_adjacency());
        (Node&)(*position) = v;
        
        RevAdjacency* const rev_position = &(_predecessors.at(v)[--deg.at(v).first]);
        DEBUG5(std::cout<<"constructing rev adjacency "<<get_reverse_adjacency(u, *position)<<" at "<<rev_position<<" from adjacency "<<*position<<std::endl);
        new(rev_position) RevAdjacency(get_reverse_adjacency(u, *position));
      }
      _size = given_edges.size();
    }

  public:

    //! initialization from edgelist
    //NOTE: if old_to_new == nullptr, we will assume that the edgelist is already consecutive, otherwise, we will translate to consecutive vertices
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    _ConsecutiveNetworkAdjacencyStorage(const GivenEdgeContainer& given_edges,
                                        NodeTranslation* old_to_new = nullptr,
                                        LeafContainer* leaves = nullptr):
      _succ_storage(given_edges.size()),
      _pred_storage(given_edges.size())
    {
      DEBUG3(std::cout << "initializing ConsecutiveNetworkAdjacencyStorage with leaf storage at "<<leaves<<", translate at "<<old_to_new<<" and "<<given_edges.size()<<" consecutive edges:\n" << given_edges<<std::endl);
      ConsecutiveMap<Node, InOutDegree> deg;
      compute_degrees(given_edges, deg, old_to_new);
      
      std::cout << "computed degrees:\n" << deg << '\n';
      _root = compute_root_and_leaves(deg, leaves);
      std::cout << "inserting edges...\n";
      insert_edges(given_edges, deg, old_to_new);

      std::cout << "computed   successors:\n" << _successors <<'\n';
      std::cout << "computed predecessors:\n" << _predecessors <<'\n';
    }

    //! initialization from explicitly non-consecutive edgelist
    //NOTE: if old_to_new == nullptr, we will create a temporary translate map and delete it when finishing construction
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    _ConsecutiveNetworkAdjacencyStorage(const non_consecutive_tag_t,
                                        const GivenEdgeContainer& given_edges,
                                        NodeTranslation* old_to_new = nullptr,
                                        LeafContainer* leaves = nullptr):
      _ConsecutiveNetworkAdjacencyStorage(given_edges, old_to_new ? old_to_new : std::unique_ptr<NodeTranslation>(new NodeTranslation()).get(), leaves)
    {}

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

    using DegreeMap = NodeMap<InOutDegree>;
  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

    // storage for the adjacencies; _successors will point into this
    ConsecutiveStorage<Adjacency> _succ_storage;


    template<class GivenEdgeContainer>
    void insert_edges(const GivenEdgeContainer& given_edges, DegreeMap& deg)
    {
      const size_t num_nodes = deg.size();
      auto nh_start = _succ_storage.begin();
      for(Node u = 0; u != num_nodes; ++u){
        // note: it is important for the initialization of _successors that we go through the nodes in sorted order here
        const OutDegree u_outdeg = deg.at(u).second;
        _successors.try_emplace(u, nh_start, u_outdeg);
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
      DegreeMap deg;
      compute_degrees(given_edges, deg, old_to_new);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

    //! initialization from explicitly non-consecutive edgelist
    //NOTE: if old_to_new == nullptr, we will create a temporary translate map and delete it when finishing construction
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    _ConsecutiveTreeAdjacencyStorage(const non_consecutive_tag_t,
                                        const GivenEdgeContainer& given_edges,
                                        NodeTranslation* old_to_new = nullptr,
                                        LeafContainer* leaves = nullptr):
      _ConsecutiveTreeAdjacencyStorage(given_edges, old_to_new ? old_to_new : std::unique_ptr<NodeTranslation>().get(), leaves)
    {}
  };

  template<class _NodeData = void,
           class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutivePredecessorMap<_EdgeData>>
  using ConsecutiveNetworkAdjacencyStorage = AddNodeData<_NodeData, _ConsecutiveNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>>;

  template<class _NodeData = void,
           class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutiveTreePredecessorMap<_EdgeData>>
  using ConsecutiveTreeAdjacencyStorage = AddNodeData<_NodeData, _ConsecutiveTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>>;



}

