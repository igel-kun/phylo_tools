

#pragma once

#include "storage_adj_common.hpp"

namespace PT{

  // ============ non growing storages =====================


  template<class _Edge = Edge<> >
  class NonGrowingRootedAdjacencyStorage:
    public RootedAdjacencyStorage<_Edge, ConsecutiveStorageNoMem<typename _Edge::Adjacency>>
  {
    using Parent = RootedAdjacencyStorage<_Edge, ConsecutiveStorageNoMem<typename _Edge::Adjacency>>;
  public:
    using Edge = _Edge;
    using typename Parent::AdjContainer;

    using AdjContainerWithMem = ConsecutiveStorage<typename _Edge::Adjacency>;

  protected:
    // _neighbors contains a list of all out-neighbors into which _successors will point
    AdjContainerWithMem _neighbors;

  public:

    template<class EdgeContainer>
    NonGrowingRootedAdjacencyStorage(const EdgeContainer& given_edges):
      _neighbors(given_edges.size())
    {
      DEBUG3(std::cout << "initializing NonGrowingRootedAdjacencyStorage with "<<given_edges.size()<<" edges"<<std::endl);
    }

    // ================= modification =================

    bool add_edge(const Edge& e) { throw(std::logic_error("cannot add an edge to a non-growing storage")); }

    // =============== init =====================

  };

  
  template<class _Edge = Edge<> >
  class NonGrowingTreeAdjacencyStorage: public NonGrowingRootedAdjacencyStorage<_Edge>
  {
    using Parent = NonGrowingRootedAdjacencyStorage<_Edge>;
  public:
    using Parent::Parent;
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using typename Parent::AdjContainer;

    using InEdgeContainer = InEdgeFactory<Edge, AdjContainer>;
    using ConstInEdgeContainer = InEdgeConstFactory<Edge, const AdjContainer>;
    using PredContainer = AdjContainer;
    using ConstPredContainer = const AdjContainer;


  protected:
    using Parent::_neighbors;
    using Parent::_successors;
    using Parent::_root;
    using Parent::_size;

    std::unordered_map<Node, Node> _predecessors;

    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& degrees)
    {
      auto nh_start = _neighbors.begin();
      for(const auto& u_deg: degrees){
        const auto u = u_deg.first;
        const uint32_t u_outdeg = u_deg.second.second;
        _successors.DEEP_EMPLACE_TWO(u, nh_start, u_outdeg);
        nh_start += u_outdeg;
      }
      // put the edges
      for(const auto &uv: given_edges){
        //*(&(_successors[tail(edge)]) + (--out_deg[tail(edge)])) = head(edge);
        const auto* position = &(_successors[uv.tail()]) + --(degrees.at(uv.tail()).second);
        new(position) Adjacency(uv.get_adjacency());

        if(!append(_predecessors, uv.head(), *position).second)
          throw std::logic_error("cannot create tree with reticulations");
      }
      _size = given_edges.size();
    }

  public:

    uint32_t num_nodes() const { return _predecessors.size() + 1; }

    uint32_t in_degree(const Node u) const { return (u == _root) ? 0 : 1; }

    ConstPredContainer predecessors(const Node u) const
    {
      return (u == _root) ? ConstPredContainer() : ConstPredContainer(&(_predecessors.at(u)));
    }
    PredContainer predecessors(const Node u)
    {
      return (u == _root) ? PredContainer() : PredContainer(&(_predecessors.at(u)));
    }

    ConstInEdgeContainer in_edges(const Node u) const { return {_predecessors.at(u), u}; }
    InEdgeContainer in_edges(const Node u) { return {_predecessors.at(u), u}; }


    //! initialization; 
    template<class EdgeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingTreeAdjacencyStorage(const EdgeContainer& given_edges, const uint32_t num_nodes, LeafContainer* leaves = nullptr):
      Parent()
    {
      DEBUG3(std::cout << "initializing NonGrowingTreeAdjacencyStorage with "<<given_edges.size()<<" edges & leaf storage "<<leaves<<std::endl);
      std::vector_map<UIntPair> deg(num_nodes);
      compute_degrees(given_edges, deg);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingTreeAdjacencyStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr):
      Parent()
    {
      std::unordered_map<Node, UIntPair> deg;
      compute_degrees_and_nodes(given_edges, nodes, deg);
      _root = compute_root_and_leaves(deg, leaves);
      append(nodes, _root); // don't forget to append the _root
      insert_edges(given_edges, deg);
    }

  };


  template<class _Edge = Edge<> >
  class NonGrowingNetworkAdjacencyStorage: public NonGrowingRootedAdjacencyStorage<_Edge>
  {
    using Parent = NonGrowingRootedAdjacencyStorage<_Edge>;
  public:
    using typename Parent::AdjContainer;
    using typename Parent::SuccessorMap;
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::Adjacency;

    using RevAdjContainer = ConsecutiveStorage<Node>;
    using ConstRevAdjContainer = const ConsecutiveStorage<Node>;

    using PredContainer = ConsecutiveStorageNoMem<Node>;
    using ConstPredContainer = const ConsecutiveStorageNoMem<Node>;
    
    using PredecessorMap = std::unordered_map<Node, PredContainer>;
    using ConstPredecessorMap = std::unordered_map<Node, ConstPredContainer>;

    using InEdgeContainer = InEdgeFactory<Edge, PredContainer>;
    using ConstInEdgeContainer = InEdgeConstFactory<Edge, ConstPredContainer>;

  protected:
    using Parent::_neighbors;
    using Parent::_successors;
    using Parent::_root;
    using Parent::_size;

    RevAdjContainer _in_neighbors;
    PredecessorMap _predecessors;

    // prepare the container and insert a list of edges with given degrees
    template<class GivenEdgeContainer, class DegMap>
    void insert_edges(const GivenEdgeContainer& given_edges, DegMap& deg)
    {
     // reserve space for children
      auto nh_start = _neighbors.begin();
      auto rev_nh_start = _in_neighbors.begin();
      for(const auto& u_deg: deg){
        const auto u = u_deg.first;
        const uint32_t u_indeg = u_deg.second.first;
        const uint32_t u_outdeg = u_deg.second.second;
        _successors.DEEP_EMPLACE_TWO(u, nh_start, u_outdeg);
        _predecessors.DEEP_EMPLACE_TWO(u, rev_nh_start, u_indeg);
        nh_start += u_outdeg;
        rev_nh_start += u_indeg;
      }
      // put the in- and out-edges 
      for(const auto &uv: given_edges){
        Adjacency* const position = &(_successors.at(uv.tail())[--deg.at(uv.tail()).second]);
        new(position) Adjacency(uv.get_adjacency());
        
        Node* const rev_position = &(_predecessors.at(uv.head())[--deg.at(uv.head()).first]);
        new(rev_position) Node(uv.tail());
      }
      _size = given_edges.size();
    }

  public:
    
    uint32_t num_nodes() const { return _predecessors.size() + 1; }
    size_t in_degree(const Node u) const { return predecessors(u).size(); }

    ConstPredContainer& predecessors(const Node u) const {
      static const PredContainer no_predecessors;
      return_map_lookup(_predecessors, u, no_predecessors);
    }

    //InEdgeContainer      in_edges(const Node u) { return InEdgeContainer(u, predecessors(u)); }
    ConstInEdgeContainer in_edges(const Node u) const { return ConstInEdgeContainer(u, predecessors(u)); }

    //! initialization; 
    template<class EdgeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingNetworkAdjacencyStorage(const EdgeContainer& given_edges, const uint32_t num_nodes, LeafContainer* leaves = nullptr):
      Parent(given_edges, num_nodes),
      _in_neighbors(given_edges.size())
    {
      DEBUG3(std::cout << "initializing NonGrowingNetworkAdjacencyStorage with "<<given_edges.size()<<" edges & leaf storage "<<leaves<<std::endl);
      std::vector_map<UIntPair> deg(num_nodes);
      compute_degrees(given_edges, deg);
      _root = compute_root_and_leaves(deg, leaves);
      insert_edges(given_edges, deg);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    NonGrowingNetworkAdjacencyStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr):
      Parent(given_edges),
      _in_neighbors(given_edges.size())
    {
      DEBUG3(std::cout << "initializing NonGrowingNetworkAdjacencyStorage with "<<given_edges.size()<<" edges & leaf storage "<<leaves<<" (no nodes)"<<std::endl);
      std::unordered_map<Node, UIntPair> deg;
      compute_degrees_and_nodes(given_edges, nodes, deg);
      DEBUG5(std::cout << "degrees: "<<deg<<std::endl);
      _root = compute_root_and_leaves(deg, leaves);
      append(nodes, _root); // don't forget to append the _root
      insert_edges(given_edges, deg);
    }


  };

}

