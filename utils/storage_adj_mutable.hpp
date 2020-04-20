

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
  class MutableNetworkAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:

    using MutabilityTag = mutable_tag;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using typename Parent::RevEdge;
    using typename Parent::RevAdjacency;

    template<class T>
    using NodeMap = HashMap<Node, T>;
  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

  public:
    using Parent::compute_root;
    using Parent::successors;
    using Parent::predecessors;

    // Note: we assume that the node u exists already
    bool add_edge(const Edge& uv){ return add_edge(uv.tail(), uv.get_adjacency()); }
    bool add_edge(const Node u, const Adjacency& v)
    {
      if(append(_predecessors, v, u).second){
#ifndef NDEBUG
        const bool uv_app = append(_successors, u, v).second;
        assert(uv_app);
#else
        append(_successors, u, v).second;
#endif
        ++_size;
        // make sure that both u and v have an entry in both _predecessors and _successors
        _successors.try_emplace(v);
        _predecessors.try_emplace(u);
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


    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    MutableNetworkAdjacencyStorage(const GivenEdgeContainer& given_edges,
                                    NodeTranslation* old_to_new = nullptr,
                                    LeafContainer* leaves = nullptr):
      Parent()
    {
      std::cout << "adding edges to the network...\n";
      for(const auto& uv: given_edges) add_edge(uv);
      std::cout << "computing the root...\n";
      compute_root();
      std::cout << "computing node translation and leaves...\n";
      compute_translate_and_leaves(*this, old_to_new, leaves);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    MutableNetworkAdjacencyStorage(const non_consecutive_tag_t,
                                    const GivenEdgeContainer& given_edges,
                                    NodeTranslation* old_to_new = nullptr,
                                    LeafContainer* leaves = nullptr):
      MutableNetworkAdjacencyStorage(given_edges, old_to_new, leaves)
    {}

  };


  template<class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutableTreePredecessorMap<_EdgeData>>
  class MutableTreeAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  public:
    using MutabilityTag = mutable_tag;
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
    using Parent::compute_root;

    // the non-secure versions allow any form of edge addition
    // ATTENTION: this will NOT update the root! use set_root() afterwards!
    bool add_edge(const Edge& uv) { return add_edge(uv.tail(), uv.get_adjacency()); }
    bool add_edge(const Node u, const Adjacency& v)
    {
      if(append(_successors, u, v).second){
        if(append(_predecessors, v, u).second){
          ++_size;
        } else throw std::logic_error("cannot create tree with reticulation (edges ("+std::to_string(front(_predecessors[v]))+","+std::to_string(v)+") and ("+std::to_string(u)+","+std::to_string(v)+") given)");
      } else return false;
      return true;
    }

    // the secure versions allow only adding edges uv for which u is in the tree, but v is not, or v is the root and u is not in the tree
    bool add_edge_secure(const Edge& uv) { return add_edge(uv.tail(), uv.get_adjacency()); }
    bool add_edge_secure(const Node u, const Adjacency& v)
    {
      auto emp_succ = append(_successors, u);
      if(emp_succ.second){
        // oh, u did not exist before, so we added a new root
        if((v == _root) || _predecessors.empty()){
          _root = u;
          append(*emp_succ.first, v);
          append(_predecessors, v, u);
          ++_size;
          return true;
        } else {
          if(_predecessors.contains(v))
            throw std::logic_error("cannot create reticulation ("+std::to_string(v)+") in a tree");
          else
            throw std::logic_error("cannot create isolated edge ("+std::to_string(u)+","+std::to_string(v)+") with add_edge_secure() - if you are adding a bunch of edges resulting in a valid tree, use add_edge() + set_root()");
        }
      } else {
        auto emp_pred = append(_predecessors, v);
        if(emp_pred.second){
          append(*emp_pred.first, u);
          append(*emp_succ.first, v);
          ++_size;
        } else throw std::logic_error("cannot create reticulation ("+std::to_string(v)+") in a tree");
      }
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

    //! initialization from edgelist
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    MutableTreeAdjacencyStorage(const GivenEdgeContainer& given_edges,
                                 NodeTranslation* old_to_new = nullptr,
                                 LeafContainer* leaves = nullptr):
      Parent()
    {
      for(const auto& uv: given_edges) add_edge(uv);
      compute_root();
      compute_translate_and_leaves(*this, old_to_new, leaves);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec>
    MutableTreeAdjacencyStorage(const non_consecutive_tag_t,
                                    const GivenEdgeContainer& given_edges,
                                    NodeTranslation* old_to_new = nullptr,
                                    LeafContainer* leaves = nullptr):
      MutableTreeAdjacencyStorage(given_edges, old_to_new, leaves)
    {}

  };


}

