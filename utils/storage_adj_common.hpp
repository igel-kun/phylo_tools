
#pragma once

#include "predicates.hpp"
#include "skipping_iter.hpp"
#include "edge.hpp"
#include "edge_iter.hpp"
#include "storage.hpp"
#include "storage_common.hpp"
#include "set_interface.hpp"
#include "singleton.hpp"

#warning TODO: implement move constructors for edge storages and adjacency storages!!!
namespace PT{


  template<class _EdgeData, class _SuccessorMap, class _PredecessorMap>
  class RootedAdjacencyStorage
  {
  public:
    using EdgeData = _EdgeData;
    using SuccessorMap = _SuccessorMap;
    using PredecessorMap = _PredecessorMap;
      
    using NodeContainer = FirstFactory<const SuccessorMap>;
    using NodeContainerRef = NodeContainer;
    using ConstNodeContainer = NodeContainer;
    using ConstNodeContainerRef = NodeContainerRef;
    using NodeIterator = typename NodeContainer::iterator;

    using SuccContainer         = typename SuccessorMap::mapped_type;
    using SuccContainerRef      = SuccContainer&;
    using ConstSuccContainer    = const SuccContainer;
    using ConstSuccContainerRef = const SuccContainer&;
    using PredContainer         = typename PredecessorMap::mapped_type;
    using PredContainerRef      = PredContainer&;
    using ConstPredContainer    = const PredContainer;
    using ConstPredContainerRef = const PredContainer&;
   
    // Edges and RevEdges differ in that one of the two should contain a reference to the EdgeData instead of the data itself
    using Adjacency = typename SuccContainer::value_type;
    using Edge = EdgeFromAdjacency<Adjacency>;
    using RevAdjacency = typename PredContainer::value_type;
    using RevEdge = EdgeFromAdjacency<Adjacency>;

    using ConstEdgeContainer    = OutEdgeMapIterFactory<const SuccessorMap>;
    using ConstEdgeContainerRef = ConstEdgeContainer;
    using ConstOutEdgeContainer = OutEdgeFactory<const ConstSuccContainer>;
    using ConstOutEdgeContainerRef = ConstOutEdgeContainer;
    using ConstInEdgeContainer  = InEdgeFactory<const ConstPredContainer>;
    using ConstInEdgeContainerRef = ConstInEdgeContainer;

    // to get the leaves, get all pairs (u,V) of the SuccessorMap, filter-out all pairs with non-empty V and return the first items of these pairs
    using MapValueNonEmptyPredicate = std::MapValuePredicate<const SuccessorMap, const std::NonEmptySetPredicate<const ConstSuccContainer>>;
    using EmptySuccIterFactory  = std::SkippingIterFactory<const SuccessorMap, const MapValueNonEmptyPredicate>;
    using ConstLeafContainer    = FirstFactory<const EmptySuccIterFactory>;
    using ConstLeafContainerRef = ConstLeafContainer;

    using value_type = Edge;
    using reference = Edge;
    using iterator        = typename ConstEdgeContainer::iterator;
    using const_iterator  = typename ConstEdgeContainer::const_iterator;
  protected:
    SuccessorMap _successors;
    PredecessorMap _predecessors;
    
    Node _root = NoNode;
    size_t _size = 0;

    // compute the root from the _in_edges and _out_edges mappings
    void compute_root()
    {
      _root = NoNode;
#ifdef NDEBUG
      for(const auto& uV: _predecessors)
        if(uV.second.empty()){ _root = uV.first; break; }
#else
      for(const auto& uV: _predecessors)
        if(uV.second.empty()){
          if(_root == NoNode)
            _root = uV.first;
          else throw std::logic_error("cannot create tree/network with multiple roots ("+std::string(_root)+" & "+std::string(uV.first)+")");
        }
#endif
      if((_root == NoNode) && !_predecessors.empty())
        throw std::logic_error("given edgelist is cyclic (has no root)");
    }

  public:

    // =============== iteration ================
    // iterate over edges
    const_iterator begin() const { return const_iterator(_successors); }
    const_iterator end() const { return const_iterator(_successors, _successors.end()); }
    // iterator over nodes
    NodeIterator nodes_begin() const { return _successors.begin(); }
    NodeIterator nodes_end() const { return _successors.end(); }

    // =============== query ===================
    size_t num_nodes() const { return _predecessors.size() + 1; }
    size_t num_edges() const { return size(); }
    Degree out_degree(const Node u) const { return successors(u).size(); }
    Degree in_degree(const Node u) const { return predecessors(u).size(); }
    InOutDegree in_out_degree(const Node u) const { return {predecessors(u).size(), successors(u).size()}; }
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }
    Node root() const { return _root; }

    // set the root to a node that does not have predecessors (this might become necessary after adding edges willy nilly)
    void set_root(const Node u)
    {
      if(_predecessors.at(u).empty())
        _root = u;
      else throw std::logic_error("cannot set the root to "+std::string(u)+" as it has at least one predecesor "+std::string(front(_predecessors.at(u))));
    }

    // NOTE: this should go without saying, but: do not try to store away nodes() and access them after destroying the storage
    ConstNodeContainerRef nodes() const { return FirstFactory<const SuccessorMap>(_successors); } //ConstNodeContainerRef(_successors); }
    ConstSuccContainerRef successors(const Node u) const { return _successors.at(u); }
    ConstPredContainerRef predecessors(const Node u) const { return _predecessors.at(u); }

    const SuccessorMap& successor_map() const { return _successors; }
    const PredecessorMap& predecessor_map() const { return _successors; }

    ConstOutEdgeContainer out_edges(const Node u) const { return {u, successors(u)}; }
    ConstInEdgeContainer  in_edges(const Node u) const { return {u, predecessors(u)}; }
    ConstEdgeContainer    edges() const { return _successors; }

    ConstLeafContainerRef leaves() const { return ConstLeafContainerRef(new EmptySuccIterFactory(_successors, _successors)); }
  };


}// namespace

