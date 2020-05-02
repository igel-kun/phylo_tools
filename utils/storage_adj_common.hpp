
#pragma once

#include "predicates.hpp"
#include "filter.hpp"
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
      
    using NodeContainer = FirstFactory<SuccessorMap>;
    using NodeContainerRef = NodeContainer;
    using ConstNodeContainer = FirstFactory<const SuccessorMap>;
    using ConstNodeContainerRef = ConstNodeContainer;
    
    using NodeIterator = std::iterator_of_t<NodeContainer>;
    using ConstNodeIterator = std::iterator_of_t<ConstNodeContainer>;

  protected:
    using SuccContainer         = typename SuccessorMap::mapped_type;
    using PredContainer         = typename PredecessorMap::mapped_type;
  public:
    // NOTE: okay, so C++ doesn't allow const containers of non-const objects, so if we want the user to be anble to change the edge-data of an adjacency,
    //       we'll have to return an IterFactory-wrapper around the internal container, thus keeping the latter hidden/unmodified
    //       This problem does not occur for constant successor containers, so we can just return a const reference to the internal storage
    using SuccContainerRef      = std::IterFactory<SuccContainer>;
    using ConstSuccContainer    = const SuccContainer;
    using ConstSuccContainerRef = ConstSuccContainer&;
    using PredContainerRef      = std::IterFactory<PredContainer>;
    using ConstPredContainer    = const PredContainer;
    using ConstPredContainerRef = ConstPredContainer&;
   
    // Edges and RevEdges differ in that one of the two should contain a reference to the EdgeData instead of the data itself
    using Adjacency     = typename SuccContainer::value_type;
    using Edge          = EdgeFromAdjacency<Adjacency>;
    using EdgeRef       = Edge;
    using RevAdjacency  = typename PredContainer::value_type;
    using RevEdge       = EdgeFromAdjacency<Adjacency>;
    using RevEdgeRef    = RevEdge;

    using EdgeContainer         = OutEdgeMapIterFactory<SuccessorMap>;
    using EdgeContainerRef      = EdgeContainer;
    using ConstEdgeContainer    = OutEdgeMapIterFactory<const SuccessorMap>;
    using ConstEdgeContainerRef = ConstEdgeContainer;
    using OutEdgeContainer      = OutEdgeFactory<SuccContainer>;
    using OutEdgeContainerRef   = OutEdgeContainer;
    using ConstOutEdgeContainer = OutEdgeFactory<const SuccContainer>;
    using ConstOutEdgeContainerRef = ConstOutEdgeContainer;
    using InEdgeContainer       = InEdgeFactory<PredContainer>;
    using InEdgeContainerRef    = InEdgeContainer;
    using ConstInEdgeContainer  = InEdgeFactory<const PredContainer>;
    using ConstInEdgeContainerRef  = ConstInEdgeContainer;

    // to get the leaves, get all pairs (u,V) of the SuccessorMap, filter-out all pairs with non-empty V and return the first items of these pairs
    using MapValueNonEmptyPredicate = std::MapValuePredicate<const SuccessorMap, const std::NonEmptySetPredicate<const ConstSuccContainer>>;
    using EmptySuccIterFactory  = std::FilteredIterFactory<SuccessorMap, const MapValueNonEmptyPredicate>;
    using LeafContainer         = FirstFactory<EmptySuccIterFactory>;
    using LeafContainerRef      = LeafContainer;
    using ConstEmptySuccIterFactory  = std::FilteredIterFactory<const SuccessorMap, const MapValueNonEmptyPredicate>;
    using ConstLeafContainer    = FirstFactory<const ConstEmptySuccIterFactory>;
    using ConstLeafContainerRef = ConstLeafContainer;

    using value_type      = Node;
    using reference       = Node;
    using iterator        = NodeIterator;
    using const_iterator  = ConstNodeIterator;
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
      for(const auto& uV: _predecessors){
        std::cout << "predecessors of "<<uV.first<<" = "<<uV.second<<"\n";
        if(uV.second.empty()){
          if(_root == NoNode)
            _root = uV.first;
          else throw std::logic_error("cannot create tree/network with multiple roots ("+std::to_string(_root)+" & "+std::to_string(uV.first)+")");
        }
      }
#endif
      if((_root == NoNode) && !_predecessors.empty())
        throw std::logic_error("given edgelist is cyclic (has no root)");
    }

  public:

    // =============== iteration ================

    // iterator over nodes
    ConstNodeIterator begin() const { return _successors.begin(); }
    ConstNodeIterator end() const { return _successors.end(); }
    NodeIterator begin() { return _successors.begin(); }
    NodeIterator end() { return _successors.end(); }

    // =============== query ===================
    size_t num_nodes() const { return _successors.size(); }
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
      else throw std::logic_error("cannot set the root to "+std::to_string(u)+" as it has at least one predecesor "+std::to_string(front(_predecessors.at(u))));
    }

    // NOTE: this should go without saying, but: do not try to store away nodes() and access them after destroying the storage
    ConstNodeContainerRef nodes() const { return _successors; }
    NodeContainerRef      nodes()       { return _successors; } 
    ConstLeafContainerRef leaves() const { return ConstLeafContainerRef(ConstEmptySuccIterFactory(_successors, _successors.end(), _successors)); }
    LeafContainerRef      leaves()       { return LeafContainerRef(EmptySuccIterFactory(_successors, _successors.end(), _successors)); }

    // iterate over adjacencies
    //NOTE: in order to allow the user to modify the edge-data associated with an adjacency, we give him/her a proxy container
    //      that only allows access to the items in the container, but not the container itself
    ConstSuccContainerRef successors(const Node u) const { return _successors.at(u); }
    SuccContainerRef      successors(const Node u) { return _successors.at(u); }
    // successors are also called "children"
    ConstSuccContainerRef children(const Node u) const { return _successors.at(u); }
    SuccContainerRef      children(const Node u) { return _successors.at(u); }

    ConstPredContainerRef predecessors(const Node u) const { return _predecessors.at(u); }
    PredContainerRef      predecessors(const Node u) { return _predecessors.at(u); }
    // predecessors are also called "parents"
    ConstPredContainerRef parents(const Node u) const { return _predecessors.at(u); }
    PredContainerRef      parents(const Node u) { return _predecessors.at(u); }

    const SuccessorMap&   successor_map() const { return _successors; }
    const PredecessorMap& predecessor_map() const { return _predecessors; }

    ConstOutEdgeContainer out_edges(const Node u) const { return ConstOutEdgeContainer(_successors.at(u), u); }
    OutEdgeContainer      out_edges(const Node u) { return OutEdgeContainer(_successors.at(u), u); }
    
    ConstInEdgeContainer  in_edges(const Node u) const { return ConstInEdgeContainer(predecessors(u), u); }
    InEdgeContainer       in_edges(const Node u) { return InEdgeContainer(_predecessors.at(u), u); }
    
    ConstEdgeContainer    edges() const { return ConstEdgeContainer(_successors); }
    EdgeContainer         edges() { return EdgeContainer(_successors); }

  };


  // the following classes are for adding (or not) node-data onto any form of _EdgeStorage

  // NOTE: be advised not to interprete default-constructed NodeData as "has already been given data" when using consecutive storages,
  //       since ConsecutiveMap will default-construct all missing NodeData up to i when constructing NodeData for i (see raw_vector_map.hpp)
  // NOTE: the weird std::conditional is needed because otherwise, the default template arg cannot be initialized if _NodeData == void
  template<class _NodeData,
           class _EdgeStorage,
           class _NodeDataMap = typename _EdgeStorage::template NodeMap<std::conditional_t<std::is_void_v<_NodeData>, int, _NodeData>>>
  class AddNodeData: public _EdgeStorage
  {
    using Parent = _EdgeStorage;
    _NodeDataMap node_data;
  public:
    using NodeDataMap = _NodeDataMap;
    using NodeData = _NodeData;
    using Parent::Parent;
    static constexpr bool has_node_data = true;

    template<class... Args>
    NodeData& emplace_node_data(const Node u, Args&&... args) { return node_data.try_emplace(u, std::forward<Args>(args)...).first->second; }

    // on const EdgeStorages, use node_data.at(), which will throw an exception if u does not yet have node data
    const NodeData& get_node_data(const Node u) const { return node_data.at(u); }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    
    // on non-const EdgeStorages, operator[] can emplace NodeData...
    template<class = std::enable_if_t<std::is_default_constructible_v<_NodeData>>, class... Args>
    NodeData& operator[](const Node u) { return emplace_node_data(u); }
    // ... but get_node_data() will throw an out_of_range exception
    NodeData& get_node_data(const Node u) { return node_data.at(u); }

    const NodeDataMap& get_node_data() const { return node_data; }
  };

  template<class _EdgeStorage, class _NodeDataMap>
  class AddNodeData<void, _EdgeStorage, _NodeDataMap>: public _EdgeStorage
  {
    public:
      using _EdgeStorage::_EdgeStorage;
      using NodeData = void;
      static constexpr bool has_node_data = false;
  };



}// namespace

