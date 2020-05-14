

#pragma once

#include <cassert>
#include "storage_adj_common.hpp"

namespace PT{
  // by default, save the edge data in the successor map and provide a reference in each "reverse adjacency" of the predecessor map
  template<class EdgeData>
  using DefaultConsecutiveSuccessorMap = RawConsecutiveMap<Node, ConsecutiveStorageNoMem<AdjacencyFromData<EdgeData>>>;
  template<class EdgeData>
  using DefaultConsecutivePredecessorMap = DefaultConsecutiveSuccessorMap<DataReference<EdgeData>>;
  template<class EdgeData>
  using DefaultConsecutiveTreePredecessorMap = RawConsecutiveMap<Node, std::singleton_set<ReverseAdjacencyFromData<EdgeData>, std::invalid_fac<Node>>>;


  template<class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutivePredecessorMap<_EdgeData>>
  class ConsecutiveNetworkAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using MutabilityTag = mutable_tag;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using typename Parent::RevEdge;
    using typename Parent::RevAdjacency;

    template<class T, class InvalidElement = void>
    using NodeMap = ConsecutiveMap<Node, T, InvalidElement>;

    using Translation   = NodeMap<Node, std::invalid_fac<Node>>;
    using InOutDegreeMap= NodeMap<InOutDegree, std::constexpr_fac<InOutDegree, Degree, Degree>::factory<-1u, -1u>>;
    using DegreeMap     = NodeMap<Degree, std::invalid_fac<Degree>>;
    using NodeSet = ConsecutiveNodeSet;

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
    // NOTE: this effectively destroys (not destructs) the DegreeMap
    template<class GivenEdgeContainer, class NodeTranslation = Translation>
    void setup_edges(GivenEdgeContainer&& given_edges, RawConsecutiveMap<Node, InOutDegree>& deg, const NodeTranslation* const old_to_new)
    {
      const size_t num_nodes = deg.size();
     // reserve space for children
      auto nh_start = _succ_storage.begin();
      auto rev_nh_start = _pred_storage.begin();
      for(Node u = 0; u != num_nodes; ++u) {
        // note: it is important for the initialization of _predecessors & _successors that we go through the nodes in sorted order here
        const auto u_deg = deg.at(u);
        const Degree u_indeg = u_deg.first;
        const Degree u_outdeg = u_deg.second;
        _successors.try_emplace(u, nh_start, u_outdeg);
        _predecessors.try_emplace(u, rev_nh_start, u_indeg);
        nh_start += u_outdeg;
        rev_nh_start += u_indeg;
      }
      // put the in- and out-edges in their pre-allocated storage
      for(auto&& uv: given_edges){
        const Node u = old_to_new ? (*old_to_new).at(uv.tail()) : uv.tail();
        const Node v = old_to_new ? (*old_to_new).at(uv.head()) : uv.head();
        Adjacency* const position = _successors.at(u).begin() + (--deg.at(u).second);
        DEBUG5(std::cout << "constructing adjacency "<<uv.get_adjacency()<<" at "<<position<<std::endl);
        emplace_new_adjacency(position, std::move(uv.get_adjacency()), v);

        RevAdjacency* const rev_position = &(_predecessors.at(v)[--deg.at(v).first]);
        DEBUG5(std::cout<<"constructing rev adjacency "<<get_reverse_adjacency(u, *position)<<" at "<<rev_position<<" from adjacency "<<*position<<std::endl);
        emplace_new_adjacency(rev_position, std::move(get_reverse_adjacency(u, *position)));
      }
      _size = given_edges.size();
    }

  public:

    //! initialization from edgelist
    //NOTE: if old_to_new == nullptr, we will assume that the edgelist is already consecutive, otherwise, we will translate to consecutive vertices
    template<class GivenEdgeContainer, class LeafContainer = NodeVec, class NodeTranslation = Translation>
    ConsecutiveNetworkAdjacencyStorage(GivenEdgeContainer&& given_edges, // universal reference
                                       NodeTranslation* old_to_new = nullptr,
                                       LeafContainer* leaves = nullptr):
      _succ_storage(given_edges.size()),
      _pred_storage(given_edges.size())
    {
      DEBUG3(std::cout << "initializing ConsecutiveNetworkAdjacencyStorage with leaf storage at "<<leaves<<", translate at "<<old_to_new<<" and "<<given_edges.size()<<" consecutive edges:\n" << given_edges<<std::endl);
      RawConsecutiveMap<Node, InOutDegree> deg;
      compute_degrees(given_edges, deg, old_to_new);
      
      std::cout << "computed degrees:\n" << deg << '\n';
      _root = compute_root_and_leaves(deg, leaves);
      std::cout << "inserting edges...\n";
      setup_edges(std::forward<GivenEdgeContainer>(given_edges), deg, old_to_new);

      std::cout << "computed   successors:\n" << _successors <<'\n';
      std::cout << "computed predecessors:\n" << _predecessors <<'\n';
    }

    //! initialization from explicitly non-consecutive edgelist
    //NOTE: if old_to_new == nullptr, we will create a temporary translate map and delete it when finishing construction
    template<class GivenEdgeContainer, class LeafContainer = NodeVec, class NodeTranslation = Translation>
    ConsecutiveNetworkAdjacencyStorage(const non_consecutive_tag_t,
                                        GivenEdgeContainer&& given_edges,
                                        NodeTranslation* old_to_new = nullptr,
                                        LeafContainer* leaves = nullptr):
      ConsecutiveNetworkAdjacencyStorage(std::forward<GivenEdgeContainer>(given_edges),
                                         old_to_new ? old_to_new : std::make_unique<NodeTranslation>().get(),
                                         leaves)
    {}

  };


  // tree storages may assume that each node has at most one predecessor, so we can use vector_map<Node> as _PredecessorMap
  template<class _EdgeData = void,
           class _SuccessorMap = DefaultConsecutiveSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultConsecutiveTreePredecessorMap<_EdgeData>>
  class ConsecutiveTreeAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using MutabilityTag = immutable_tag;
    using Parent::Parent;
    using typename Parent::Edge;
    using typename Parent::Adjacency;

    template<class T, class InvalidElement = void>
    using NodeMap = ConsecutiveMap<Node, T, InvalidElement>;

    using Translation   = NodeMap<Node, std::invalid_fac<Node>>;
    using InOutDegreeMap= NodeMap<InOutDegree, std::constexpr_fac<InOutDegree, Degree, Degree>::factory<-1u, -1u>>;
    using DegreeMap     = NodeMap<Degree, std::invalid_fac<Degree>>;
    using NodeSet = ConsecutiveNodeSet;
  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

    // storage for the adjacencies; _successors will point into this
    ConsecutiveStorage<Adjacency> _succ_storage;


    template<class GivenEdgeContainer>
    void setup_edges(GivenEdgeContainer&& given_edges, RawConsecutiveMap<Node, InOutDegree>& deg)
    {
      const size_t num_nodes = deg.size();
      auto nh_start = _succ_storage.begin();
      for(Node u = 0; u != num_nodes; ++u){
        // note: it is important for the initialization of _successors that we go through the nodes in sorted order here
        const Degree u_outdeg = deg.at(u).second;
        _successors.try_emplace(u, nh_start, u_outdeg);
        nh_start += u_outdeg;
      }
      // put the edges
      for(auto&& uv: given_edges){
        const Node u = uv.tail();
        const Node v = uv.head();

        //*(&(_successors[tail(edge)]) + (--out_deg[tail(edge)])) = head(edge);
        Adjacency* const position = _successors.at(u).begin() + (--deg.at(u).second);
        new(position) Adjacency(std::move(uv.get_adjacency()));

        if(!append(_predecessors, v, get_reverse_adjacency(u, *position)).second)
          throw std::logic_error("cannot create tree with reticulation (" + std::to_string(v) + ")");
      }
      _size = given_edges.size();
    }

  public:
    

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec, class NodeTranslation = Translation>
    ConsecutiveTreeAdjacencyStorage(GivenEdgeContainer&& given_edges,
                                     NodeTranslation* old_to_new = nullptr,
                                     LeafContainer* leaves = nullptr):
      _succ_storage(given_edges.size())
    {
      DEBUG3(std::cout << "initializing ConsecutiveTreeAdjacencyStorage with "<<given_edges.size()<<" edges & leaf storage "<<leaves<<std::endl);
      RawConsecutiveMap<Node, InOutDegree> deg;
      compute_degrees(given_edges, deg, old_to_new);
      _root = compute_root_and_leaves(deg, leaves);
      setup_edges(std::forward<GivenEdgeContainer>(given_edges), deg);
    }

    //! initialization from explicitly non-consecutive edgelist
    //NOTE: if old_to_new == nullptr, we will create a temporary translate map and delete it when finishing construction
    template<class GivenEdgeContainer, class LeafContainer = NodeVec, class NodeTranslation = Translation>
    ConsecutiveTreeAdjacencyStorage(const non_consecutive_tag_t,
                                        GivenEdgeContainer&& given_edges,
                                        NodeTranslation* old_to_new = nullptr,
                                        LeafContainer* leaves = nullptr):
      ConsecutiveTreeAdjacencyStorage(std::forward<GivenEdgeContainer>(given_edges), old_to_new? old_to_new: std::unique_ptr<NodeTranslation>().get(), leaves)
    {}

    size_t in_degree(const Node u) const { return (u == _root) ? 0 : 1; }
  };


}

