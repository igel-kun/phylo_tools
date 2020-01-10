

#pragma once

#include "storage_adj_common.hpp"

namespace PT{
  // =============== growing storages =====================
  
  template<class _Edge = Edge<>,
           class _AdjContainer = std::unordered_set<typename _Edge::Adjacency>,
           class _PredecessorSet = std::unordered_set<typename _Edge::Node>>
  class GrowingRootedAdjacencyStorage: public RootedAdjacencyStorage<_Edge, _AdjContainer>
  {
    using Parent = RootedAdjacencyStorage<_Edge, _AdjContainer>;
  public:
    using typename Parent::AdjContainer;
    using typename Parent::SuccessorMap;
    using typename Parent::Node;
    using Edge = _Edge;

    using PredecessorMap = std::unordered_map<Node, _PredecessorSet>;
  protected:
    using Parent::_successors;
    using Parent::_root;

    PredecessorMap _predecessors;

    // compute the leaves from the _in_edges and _out_edges mappings
    template<class LeafContainer>
    void compute_leaves(LeafContainer& leaves) const
    {
      for(const auto& uv: _predecessors){
        const Node v = uv.first;
        if(!contains(_successors, v))
          append(leaves, v);
      }
    }
    // compute the root from the _in_edges and _out_edges mappings
    void compute_root()
    {
      char roots = 0;
      std::cout << "checking successors...\n";
      for(const auto& uv: _successors){
        const Node u = uv.first;
        std::cout << u << "\n";
        if(!contains(_predecessors, u)){
          if(roots++ == 0){
            _root = u;
          } else throw std::logic_error("cannot create tree/network with multiple roots ("+std::to_string(_root)+" & "+std::to_string(u)+")");
        }
      }
    }

    template<class NodeContainer>
    void compute_nodes(NodeContainer& nodes) const
    {
      for(const auto& u: const_firsts(_predecessors))
        append(nodes, u);
      // note that the root is missing from the _in_edge map
      append(nodes, _root);
    }

    // init the storage, computing leaves and nodes if wanted
    template<class LeafContainer, class NodeContainer>
    void init(LeafContainer* leaves, NodeContainer* nodes = nullptr)
    {
      compute_root();
      if(nodes) compute_nodes(*nodes);
      if(leaves) compute_leaves(*leaves);
    }
  };

  template<class _Edge = Edge<>,
           class _AdjContainer = std::unordered_set<typename _Edge::Node>>
  class GrowingTreeAdjacencyStorage: public GrowingRootedAdjacencyStorage<_Edge, _AdjContainer, typename _Edge::Node>
  {
    using Parent = GrowingRootedAdjacencyStorage<_Edge, _AdjContainer, typename _Edge::Node>;
  public:
    using Parent::Parent;
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::Adjacency;

    // if predecessors are requested, we should serve them in some container...
    using NodeRef = std::reference_wrapper<Node>;
    using ConstNodeRef = std::reference_wrapper<const Node>;
    using PredContainer = std::unordered_set<NodeRef>;
    using ConstPredContainer = std::unordered_set<ConstNodeRef>;

    using typename Parent::AdjContainer;
    using InEdgeContainer = InEdgeFactory<Edge, const AdjContainer>;
    using ConstInEdgeContainer = InEdgeConstFactory<Edge, const AdjContainer>;

  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;
    using Parent::init;

  public:

    Node parent(const Node u) const {return (u == _root) ? u : _predecessors.at(u); }
    uint32_t in_degree(const Node u) const { return (u == _root) ? 0 : 1; }

    ConstPredContainer predecessors(const Node u) const
      { return (u == _root) ? ConstPredContainer({}) : ConstPredContainer({_predecessors.at(u)}); }

    ConstInEdgeContainer const_in_edges(const Node u) const
      { return (u == _root) ? ConstInEdgeContainer({}) : ConstInEdgeContainer(u, {_predecessors.at(u)}, true); }
    ConstInEdgeContainer in_edges(const Node u) const { return const_in_edges(u); }

    Edge& in_edge(const Node u) {return {_predecessors.at(u), u}; }
    const Edge& in_edge(const Node u) const {return {_predecessors.at(u), u}; }


    bool add_edge(const Edge& uv) { return add_edge(uv.tail(), uv.get_adjacency()); }
    bool add_edge(const Node u, const Adjacency& v)
    {
      if(!append(_predecessors, v, u).second)
        throw std::logic_error("cannot create reticulation in tree adjacency storage");
      if(append(_successors[u], v).second){
        ++_size;
        return true;
      } else return false;
    }

    bool remove_edge(const Edge& uv) { return remove_edge(uv.tail(), uv.head()); }
    bool remove_edge(const Node u, const Node v)
    {
      const auto uv_in = _predecessors.find(v);
      if(uv_in != _predecessors.end()){
        // the data structure better be consistent
        assert(*uv_in == u);
        // remove uv from both containers
        _successors.at(u).erase(v);
        _predecessors.erase(uv_in);
        --_size;
        return true;
      } else return false; // edge is not in here, so nothing to delete
    }

    bool remove_node(const Node v)
    {
      if((v == _root) && (_size != 0)) throw(std::logic_error("cannot remove the root from a non-empty rooted storage"));
      const auto v_pre = _predecessors.find(v);
      if(v_pre != _predecessors.end()){
        _successors.at(*v_pre).erase(v);
        _predecessors.erase(v);
        _successors.erase(v);
        return true;
      } else return false;
    }

    //! initialization from edgelist with consecutive nodes
    // actually, we don't care about consecutiveness for growing storages...
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    GrowingTreeAdjacencyStorage(const consecutive_edgelist_tag& tag,
                           const GivenEdgeContainer& given_edges,
                           const uint32_t num_nodes, // although we don't really need to know how many nodes there are, we keep this for compatibility
                           LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      init(leaves);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    GrowingTreeAdjacencyStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      init(leaves, &nodes);
    }


  };


  template<class _Edge = Edge<>,
           class _SuccAdjContainer = std::unordered_set<typename _Edge::Adjacency>,
           class _PredAdjContainer = std::unordered_set<typename _Edge::Node>>
  class GrowingNetworkAdjacencyStorage: public GrowingRootedAdjacencyStorage<_Edge, _SuccAdjContainer, _PredAdjContainer>
  {
    using Parent = GrowingRootedAdjacencyStorage<_Edge, _SuccAdjContainer>;
  public:
    using Parent::Parent;
    using typename Parent::AdjContainer;
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using PredAdjContainer = _PredAdjContainer;

    using PredContainer = PredAdjContainer&;
    using ConstPredContainer = const PredAdjContainer&;

    // NOTE: InEdges will not contain the weight (only OutEdges do) since otherwise, I'd have to store data twice which may be huge
    using InEdgeContainer = InEdgeFactory<Edge, PredAdjContainer>;
    using ConstInEdgeContainer = InEdgeConstFactory<Edge, PredAdjContainer>;
 
    static const PredAdjContainer no_predecessors;
  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;
    using Parent::init;

  public:


    bool add_edge(const Edge& uv){ return add_edge(uv.tail(), uv.get_adjacency()); }

    bool add_edge(const Node u, const Adjacency& v)
    {
      if(append(_predecessors[v], u).second){
        const bool uv_app = append(_successors[u], v).second;
        assert(uv_app);
        ++_size;
        return true;
      } else return false;
    }
  
    bool remove_edge(const Edge& uv) { return remove_edge(uv.tail(), uv.head()); }
    bool remove_edge(const Node u, const Node v)
    {
      const auto v_pre = _predecessors.find(v);
      if(v_pre != _predecessors.end()){
        // NOTE: make sure two edges with equal head & tail compare true wrt. operator==() (they should if they are derived from std::pair<Node, Node>
        const auto uv_pre = v_pre.first->find(u);
        if(uv_pre != v_pre->end()){
          // the data structure better be consistent
          assert(*uv_pre == u);
          // remove uv from both containers
          _successors.at(u).erase(u);
          v_pre->erase(uv_pre);
          --_size;
          return true;
        } else return false;
      } else return false; // edge is not in here, so nothing to delete
    }

    bool remove_node(const Node v)
    {
      if((v == _root) && (_size != 0)) throw(std::logic_error("cannot remove the root from a non-empty rooted storage"));
      const auto v_pre = _predecessors.find(v);
      if(v_pre != _predecessors.end()){
        for(const auto& u: *v_pre)
          _successors.at(u).erase(v);
        _predecessors.erase(v);
        _successors.erase(v);
        return true;
      } else return false;
    }



    uint32_t in_degree(const Node u) const { return predecessors(u).size(); }
    
    PredContainer      predecessors(const Node u) { return_map_lookup(_predecessors, u, no_predecessors); }
    ConstPredContainer predecessors(const Node u) const { return_map_lookup(_predecessors, u, no_predecessors); }

    InEdgeContainer      in_edges(const Node u) { return {u, predecessors(u)}; }
    ConstInEdgeContainer in_edges(const Node u) const { return {u, predecessors(u)}; }


    //! initialization from edgelist with consecutive nodes
    // actually, we don't care about consecutiveness for growing storages...
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    GrowingNetworkAdjacencyStorage(const consecutive_edgelist_tag& tag,
                           const GivenEdgeContainer& given_edges,
                           const uint32_t num_nodes, // although we don't really need to know how many nodes there are, we keep this for compatibility
                           LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      init(leaves);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    GrowingNetworkAdjacencyStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      init(leaves, &nodes);
    }

  };

  template<class _Edge, class _SC, class _PC>
  const typename GrowingNetworkAdjacencyStorage<_Edge, _SC, _PC>::PredAdjContainer GrowingNetworkAdjacencyStorage<_Edge, _SC, _PC>::no_predecessors;



}

