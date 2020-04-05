

#pragma once

#include "storage_adj_common.hpp"

/*
 * in mutable edge storages, we DO NOT assume consecutivity.
 * Thus, most stuff is stored in hash-maps
 */

namespace PT{
  
  // by default, save the edge data in the successor map and provide a reference in each "reverse adjacency" of the predecessor map
  // note: the default maps map Nodes to sets of adjacencies in which 2 adjacencies are considered equal if their nodes are equal (ignoring data)
  template<class _EdgeData>
  using DefaultMutableSuccessorMap = HashMap<Node, HashSet<typename Edge<_EdgeData>::Adjacency, std::hash<Node>, std::equal_to<Node>>>;
  template<class _EdgeData>
  using DefaultMutablePredecessorMap = DefaultMutableSuccessorMap<DataReference<_EdgeData>>;
  template<class _EdgeData>
  using DefaultMutableTreePredecessorMap = HashMap<Node, std::SingletonSet<ReverseAdjacencyFromData<_EdgeData>>>;



  template<class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutablePredecessorMap<_EdgeData>>
  class _MutableNetworkAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:

    using Mutability_Tag = mutable_tag;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using typename Parent::RevEdge;
    using typename Parent::RevAdjacency;

    template<class T>
    using NodeMap = HashMap<Node, T>;
  protected:
    using Parent::_successors;
    using Parent::successors;
    using Parent::_predecessors;
    using Parent::predecessors;
    using Parent::_root;
    using Parent::_size;

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


    //! initialization from edgelist with consecutive nodes
    // actually, we don't care about consecutiveness for growing storages...
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    _MutableNetworkAdjacencyStorage(const consecutive_edgelist_tag& tag, const GivenEdgeContainer& given_edges, LeafContainer* leaves = nullptr):
      _MutableNetworkAdjacencyStorage(given_edges, leaves)
    {}

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = NodeVec>
    _MutableNetworkAdjacencyStorage(const GivenEdgeContainer& given_edges, LeafContainer* leaves)
    {
      for(const auto& uv: given_edges) add_edge(uv);
      _root = compute_root_and_leaves(*this, leaves);
    }

  };

  template<class _NodeData,
           class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutablePredecessorMap<_EdgeData>>
  class MutableNetworkAdjacencyStorage: public _MutableNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _MutableNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using NodeData = _NodeData;
    using Parent::Parent;

    const NodeData& get_node_data(const Node u) const { return *(static_cast<NodeData*>(u)); }
    NodeData& get_node_data(const Node u) { return *(static_cast<NodeData*>(u)); }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    NodeData& operator[](const Node u) { return get_node_data(u); }
  };

  template<class _EdgeData,
           class _SuccessorMap,
           class _PredecessorMap>
  class MutableNetworkAdjacencyStorage<void, _EdgeData, _SuccessorMap, _PredecessorMap>:
      public _MutableNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _MutableNetworkAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Parent::Parent;
  };



  template<class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutableTreePredecessorMap<_EdgeData>>
  class _MutableTreeAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Mutability_Tag = mutable_tag;
    using Parent::Parent;
    using typename Parent::Edge;
    using typename Parent::Adjacency;

    template<class T>
    using NodeMap = HashMap<Node, T>;
  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

  public:

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
    // actually, we don't care about consecutiveness for mutable storages, but this is in for compatibility with immutable storages
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    _MutableTreeAdjacencyStorage(const GivenEdgeContainer& given_edges, LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges) add_edge(uv);
      init(leaves);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    _MutableTreeAdjacencyStorage(const GivenEdgeContainer& given_edges,
                                NodeTranslation* old_to_new = nullptr,
                                LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      init(leaves);
    }


  };



  template<class _NodeData,
           class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutablePredecessorMap<_EdgeData>>
  class MutableTreeAdjacencyStorage: public _MutableTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _MutableTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using NodeData = _NodeData;
    using Parent::Parent;

    const NodeData& get_node_data(const Node u) const { return *(static_cast<NodeData*>(u)); }
    NodeData& get_node_data(const Node u) { return *(static_cast<NodeData*>(u)); }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    NodeData& operator[](const Node u) { return get_node_data(u); }
  };

  template<class _EdgeData,
           class _SuccessorMap,
           class _PredecessorMap>
  class MutableTreeAdjacencyStorage<void, _EdgeData, _SuccessorMap, _PredecessorMap>:
      public _MutableTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = _MutableTreeAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using Parent::Parent;
  };



}

