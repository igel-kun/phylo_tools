
#pragma once

#include "predicates.hpp"
#include "filter.hpp"
#include "edge.hpp"
#include "edge_iter.hpp"
#include "storage.hpp"
#include "set_interface.hpp"
#include "singleton.hpp"

#warning TODO: implement move constructors for edge storages and adjacency storages!!!
#warning TODO: instead of having 2 containers (1 for succ, 1 for pred), store only 1 map (node to preds, succ). Then, create a 'DegreeIterator' and create a 'Reticulation'-predicate and then create a reticulations()-function
namespace PT{

  using consecutive_tag = std::true_type;
  using non_consecutive_tag = std::false_type;

  using mutable_tag = std::true_type;
  using immutable_tag = std::false_type;

  template<class EdgeStorage>
  static constexpr bool is_mutable = EdgeStorage::MutabilityTag::value;
  template<class EdgeStorage>
  static constexpr bool has_consecutive_nodes = !is_mutable<EdgeStorage>;


  template<class _EdgeData, class _SuccessorMap, class _PredecessorMap>
  class RootedAdjacencyStorage
  {
  public:
    using EdgeData = _EdgeData;
    using SuccessorMap = _SuccessorMap;
    using PredecessorMap = _PredecessorMap;
      
    //using NodeContainer = FirstFactory<SuccessorMap>;
    using ConstNodeContainer = FirstFactory<const SuccessorMap>;
    
    //using NodeIterator = std::iterator_of_t<NodeContainer>;
    using ConstNodeIterator = std::iterator_of_t<ConstNodeContainer>;

    using NodeData = void;
    static constexpr bool has_node_data = false;

    using SuccContainer         = typename SuccessorMap::mapped_type;
    using PredContainer         = typename PredecessorMap::mapped_type;
    // NOTE: okay, so C++ doesn't allow const containers of non-const objects, so if we want the user to be anble to change the edge-data of an adjacency,
    //       we'll have to return an IterFactory-wrapper around the internal container, thus keeping the latter hidden/unmodified
    //       This problem does not occur for constant successor containers, so we can just return a const reference to the internal storage
#warning TODO: implement continous storage succ-container as vectors instead of the self-built class; then SuccContainerRefs are actual refs to the succ containers
   
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
    using MapValueEmptyPredicate = std::MapValuePredicate<std::EmptySetPredicate>;
    using ConstLeafIterFactory  = std::FilteredIterFactory<const SuccessorMap, MapValueEmptyPredicate>;
    using ConstLeafContainer    = FirstFactory<const ConstLeafIterFactory>;

    struct RetiPredicate { static bool value(const typename PredecessorMap::value_type& p) { return p.second.size() >= 2; } };
    using ConstRetiIterFactory  = std::FilteredIterFactory<const PredecessorMap, RetiPredicate>;
    using ConstRetiContainer    = FirstFactory<const ConstRetiIterFactory>;

    using value_type      = Node;
    using reference       = Node;
    using const_iterator  = ConstNodeIterator;
    using iterator        = const_iterator;
  protected:
    SuccessorMap _successors;
    PredecessorMap _predecessors;
    
    Node _root = NoNode;
    size_t _size = 0;

    // compute the root from the _in_edges and _out_edges mappings
    void compute_root()
    {
      _root = NoNode;
      for(const auto& uV: _predecessors)
        if(uV.second.empty()) {
#ifdef NDEBUG
          _root = uV.first;
          return;
#else
          if(_root != NoNode) 
            throw std::logic_error("cannot create tree/network with multiple roots ("+std::to_string(_root)+" & "+std::to_string(uV.first)+")");
          _root = uV.first;
#endif
        }
      if((_root == NoNode) && !_predecessors.empty())
        throw std::logic_error("given edgelist is cyclic (has no root)");
    }

    virtual void erase_node_data(const Node) {}
    virtual ~RootedAdjacencyStorage() = default;
  public:
    // =============== iteration ================

    // iterator over nodes
    ConstNodeIterator begin() const { return _successors.begin(); }
    ConstNodeIterator end() const { return _successors.end(); }

    // =============== query ===================
    size_t num_nodes() const { return _successors.size(); }
    size_t num_edges() const { return size(); }
    Degree out_degree(const Node u) const { return successors(u).size(); }
    Degree in_degree(const Node u) const { return predecessors(u).size(); }
    InOutDegree in_out_degree(const Node u) const { return {predecessors(u).size(), successors(u).size()}; }
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }
    bool has_node(const Node u) const { return _successors.find(u) != _successors.end(); }
    Node root() const { return _root; }

    // set the root to a node that does not have predecessors (this might become necessary after adding edges willy nilly)
    void set_root(const Node u)
    {
      if(_predecessors.at(u).empty())
        _root = u;
      else throw std::logic_error("cannot set the root to "+std::to_string(u)+" as it has at least one predecesor "+std::to_string(front(_predecessors.at(u))));
    }

    // NOTE: this should go without saying, but: do not try to store away nodes() and access them after destroying the storage
    ConstNodeContainer nodes() const { return _successors; }
    //NodeContainerRef      nodes()       { return _successors; } 
    ConstLeafContainer leaves() const { return ConstLeafContainer(ConstLeafIterFactory(_successors, _successors.end())); }
    //LeafContainer      leaves()       { return LeafContainer(LeafIterFactory(_successors, _successors.end())); }
    ConstRetiContainer reticulations() const { return ConstRetiContainer(ConstRetiIterFactory(_predecessors, _predecessors.end())); }

    // iterate over adjacencies
    //NOTE: in order to allow the user to modify the edge-data associated with an adjacency, we give him/her a proxy container
    //      that only allows access to the items in the container, but not the container itself
    const SuccContainer& successors(const Node u) const { return _successors.at(u); }
    SuccContainer&       successors(const Node u) { return _successors.at(u); }

    // successors are also called "children"
    const SuccContainer& children(const Node u) const { return _successors.at(u); }
    SuccContainer&       children(const Node u) { return _successors.at(u); }

    const PredContainer& predecessors(const Node u) const { return _predecessors.at(u); }
    PredContainer&       predecessors(const Node u) { return _predecessors.at(u); }
    
    // predecessors are also called "parents"
    const PredContainer& parents(const Node u) const { return _predecessors.at(u); }
    PredContainer&       parents(const Node u) { return _predecessors.at(u); }

    // this returns the "first" parent of u; for trees, it must be the only parent of u (but networks might also be interested in "any" parent of u)
    //NOTE: this will segfault if called for the root
    const RevAdjacency& parent(const Node u) const { return std::front(parents(u)); }

    // this returns the root as parent for the root, instead of segfaulting / throwing out-of-range
    //NOTE: since this must work even if the tree has no edges, this cannot deal with edge-data, so everything is a node here
    Node parent_safe(const Node u) const { return (u == root()) ? u : parent(u); }
    
    Node any_child(const Node u) const { return std::front(children(u)); }

    const SuccessorMap&   successor_map() const { return _successors; }
    const PredecessorMap& predecessor_map() const { return _predecessors; }

    ConstOutEdgeContainer out_edges(const Node u) const { return ConstOutEdgeContainer(_successors.at(u), u); }
    OutEdgeContainer      out_edges(const Node u) { return OutEdgeContainer(_successors.at(u), u); }
    
    ConstInEdgeContainer  in_edges(const Node u) const { return ConstInEdgeContainer(_predecessors.at(u), u); }
    InEdgeContainer       in_edges(const Node u) { return InEdgeContainer(_predecessors.at(u), u); }
    
    ConstEdgeContainer    edges() const { return ConstEdgeContainer(_size, _successors); }
    EdgeContainer         edges() { return EdgeContainer(_size, _successors); }
  };

  template<class _Store>
  using SuccContainer_of = std::copy_cv_t<_Store, typename _Store::SuccContainer>&;
  template<class _Store>
  using PredContainer_of = std::copy_cv_t<_Store, typename _Store::PredContainer>&;
  template<class _Store>
  using LeafContainer_of = std::conditional_t<std::is_const_v<_Store>, typename _Store::ConstLeafContainerRef, typename _Store::LeafContainerRef>;
  template<class _Store>
  using OutEdgeContainer_of = std::conditional_t<std::is_const_v<_Store>, typename _Store::ConstOutEdgeContainerRef, typename _Store::OutEdgeContainerRef>;
  template<class _Store>
  using InEdgeContainer_of = std::conditional_t<std::is_const_v<_Store>, typename _Store::ConstInEdgeContainerRef, typename _Store::InEdgeContainerRef>;
  template<class _Store>
  using EdgeContainer_of = std::conditional_t<std::is_const_v<_Store>, typename _Store::ConstEdgeContainerRef, typename _Store::EdgeContainerRef>;
  // the following classes are for adding (or not) node-data onto any form of _EdgeStorage

#warning TODO: when constructing a tree from another tree with node-translation, the node-data also has to be translated!!!
  // NOTE: the weird std::conditional is needed because otherwise, the default template arg cannot be initialized if _NodeData == void
  template<class _NodeData,
           class _EdgeStorage,
           class _NodeDataMap = typename _EdgeStorage::template NodeMap<std::conditional_t<std::is_void_v<_NodeData>, int, _NodeData>>>
  class __AddNodeData: public _EdgeStorage
  {
    using Parent = _EdgeStorage;
    _NodeDataMap node_data;
  public:
    using NodeDataMap = _NodeDataMap;
    using NodeData = _NodeData;
    using Parent::Parent;
    static constexpr bool has_node_data = true;

  protected:
    template<class... Args>
    NodeData& emplace_node_data(const Node u, Args&&... args) { return node_data.try_emplace(u, std::forward<Args>(args)...).first->second; }

    void erase_node_data(const Node x) override { node_data.erase(x); }
    
  public:
    // on const EdgeStorages, use node_data.at(), which will throw an exception if u does not yet have node data
    const NodeData& get_node_data(const Node u) const { return node_data.at(u); }
    const NodeData& operator[](const Node u) const { return get_node_data(u); }
    
    // on non-const EdgeStorages, operator[] can emplace NodeData...
    template<class = std::enable_if_t<std::is_default_constructible_v<_NodeData>>, class... Args>
    NodeData& operator[](const Node u) { return node_data[u]; }
    // ... but get_node_data() will throw an out_of_range exception
    NodeData& get_node_data(const Node u) { return node_data.at(u); }
    const NodeDataMap& get_node_data() const { return node_data; }

    // modification
    template<class... Args>
    Node add_node(Args&& ...args)
    {
      const Node result = Parent::add_node();
      emplace_node_data(result, std::forward<Args>(args)...);
      return result;
    }

    // the user can suggest(!) a node index (if this suggestion is invalid (already exists), we'll ignore it)
    template<class... Args>
    Node add_node_idx(const Node index, Args&& ...args)
    {
      const Node result = Parent::add_node_idx(index);
      emplace_node_data(result, std::forward<Args>(args)...);
      return result;
    }

    // add a new child to u and return its index
    template<class... Args>
    Node add_child(const Node u, const Node index = NoNode, Args&& ...args)
    {
      const Node result = Parent::add_child(u, index);
      emplace_node_data(result, std::forward<Args>(args)...);
      return result;
    }
  };

  template<class _NodeData,
           class _EdgeStorage,
           class _NodeDataMap = typename _EdgeStorage::template NodeMap<std::conditional_t<std::is_void_v<_NodeData>, int, _NodeData>>>
  struct _AddNodeData { using type = __AddNodeData<_NodeData, _EdgeStorage, _NodeDataMap>; };
  template<class _EdgeStorage, class _NodeDataMap>
  struct _AddNodeData<void, _EdgeStorage, _NodeDataMap> { using type = _EdgeStorage; };

  template<class _NodeData,
           class _EdgeStorage,
           class _NodeDataMap = typename _EdgeStorage::template NodeMap<std::conditional_t<std::is_void_v<_NodeData>, int, _NodeData>>>
  using AddNodeData = typename _AddNodeData<_NodeData, _EdgeStorage, _NodeDataMap>::type;

}// namespace

