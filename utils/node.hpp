
#pragma once

#include <vector>         // adjacency storage
#include <set>            // adjacency storage
#include <unordered_set>  // adjacency storage
#include <string>         // node labels

#include "types.hpp"
#include "edge_iter.hpp"

namespace PT{

  template<class EdgeData = void> class Adjacency;


  struct _ProtoNode {
#ifndef NDEBUG
    static size_t num_names;
    const size_t _name = num_names++;

    // for debugging purposes, we may want to change the name to something more readable, like a successive numbering
    size_t name() const { return _name; }
#else
    // by default, the name of a node is its address
    NodeDesc name() const { return (NodeDesc)this; }
#endif
  };

  // NOTE: we specifically refrain from polymorphic nodes (one node pointer that may point to a TreeNode or a NetworkNode) because
  //       the only gain would be to save one pointer on TreeNodes at the cost of a vtable for everyone, so not really worth it.
  //       Also, polymorphic access to the predecessors becomes a nightmare if its type is not known at compiletime
  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _EdgeData>
  class ProtoNode: public _ProtoNode {
  public:
    using Adjacency = PT::Adjacency<_EdgeData>;
    using EdgeData = _EdgeData;
    static constexpr StorageEnum SuccStorage = _SuccStorage;
    static constexpr StorageEnum PredStorage = _PredStorage;
    static constexpr bool _is_tree_node = (PredStorage == singleS);
    using SuccContainer = StorageType<SuccStorage, Adjacency>;
    using PredContainer = StorageType<PredStorage, Adjacency>;
    using LabelType = void;
  protected:
    SuccContainer _successors;
    PredContainer _predecessors;
 
  public:
    SuccContainer& successors() { return _successors; }
    const SuccContainer& successors() const { return _successors; }
    SuccContainer& children() { return _successors; }
    const SuccContainer& children() const { return _successors; }
    size_t out_degree() const { return successors().size(); }
    
    PredContainer& predecessors() { return _predecessors; }
    const PredContainer& predecessors() const { return _predecessors; }
    PredContainer& parents() { return _predecessors; }
    const PredContainer& parents() const { return _predecessors; }
    size_t in_degree() const { return predecessors().size(); }

    const ProtoNode& any_successor() const { return front(successors()); }
    const ProtoNode& any_child() const { return any_successor(); }
    template<class... Args>
    auto add_successor(Args&&... args) {
      std::cout << "adding new successor to " << successors() << "\n";
      return append(successors(), std::forward<Args>(args)...); 
    }
    template<class... Args>
    auto add_child(Args&&... args) { return add_successor(std::forward<Args>(args)...); }
    size_t remove_successor(const NodeDesc n) { return std::erase(successors(), n); }
    size_t remove_child(const NodeDesc n) { return remove_successor(n); }
    void remove_any_successor() { std::pop(successors()); }
    void remove_any_child() { remove_any_successor(); }

    const ProtoNode& any_predecessor() const { return std::front(predecessors()); }
    const ProtoNode& any_parent() const { return any_predecessor(); }
    template<class... Args>
    auto add_predecessor(Args&&... args) { return append(predecessors(), std::forward<Args>(args)...); }
    template<class... Args>
    auto add_parent(Args&&... args) { return add_predecessor(std::forward<Args>(args)...); }
    size_t remove_predecessor(const NodeDesc n) { return predecessors().erase(n); }
    size_t remove_parent(const NodeDesc n) { return remove_predecessor(n); }
    void remove_any_predecessor() { std::pop(predecessors()); }
    void remove_any_parent() { remove_any_predecessor(); }

    size_t degree() const { return in_degree() + out_degree(); }
    bool is_root() const { return predecessors().empty(); }
    bool is_tree_node() const { if constexpr(_is_tree_node) return true; else return in_degree() < 2; }
    bool is_leaf() const { return successors().empty(); }
    bool is_suppressible() const { return (in_degree() == 1) && (out_degree() == 1); }
    bool is_inner_node() const { return !successors().empty(); }
    bool is_isolated() const { return predecessors().empty() && successors().empty(); }

    template<AdjacencyType Adj>
    void replace_parent(const NodeDesc old_parent, Adj&& new_parent) {
      assert(test(predecessors(), old_parent));
      std::replace(predecessors(), old_parent, forward<Adj>(new_parent));
    }
    template<AdjacencyType Adj>
    void replace_child(const NodeDesc old_child, Adj&& new_child) {
      assert(test(successors(), old_child));
      std::replace(successors(), old_child, forward<Adj>(new_child));
    }

    // convenient way of applying some function to all nodes of a subtree
    void apply_to_subtree(auto&& function) const {
      function(*this);
      for(const NodeDesc v: successors())
        static_cast<const ProtoNode*>(v)->apply_to_subtree(std::as_const(function));
    }
    void apply_to_subtree(auto&& function) {
      function(*this);
      for(const NodeDesc v: successors())
        static_cast<ProtoNode*>(v)->apply_to_subtree(std::as_const(function));
    }

    size_t count_nodes_below() const {
      size_t result = 0;
      apply_to_subtree([&](const auto&){ ++result; });
      return result;
    }
    std::pair<size_t,size_t> count_nodes_and_edges_below() const {
      std::pair<size_t, size_t> result{0,0};
      if constexpr (is_tree_node) {
        result.first = count_nodes_below();
        result.second = result.first - 1;
      } else apply_to_subtree([&](const auto& v){ ++(result.first); result.second += v.out_degree(); });
      return result;
    }

    // ================ Edges =======================
    using OutEdgeContainer      = OutEdgeFactory<SuccContainer>;
    using ConstOutEdgeContainer = OutEdgeFactory<const SuccContainer>;
    using InEdgeContainer       = InEdgeFactory<PredContainer>;
    using ConstInEdgeContainer  = InEdgeFactory<const PredContainer>;

    OutEdgeContainer out_edges() { return {this, successors()}; }
    ConstOutEdgeContainer out_edges() const { return {this, successors()}; }
    InEdgeContainer in_edges() { return {this, predecessors()}; }
    ConstInEdgeContainer in_edges() const { return {this, predecessors()}; }
  };

  // A Node is a ProtoNode with possible NodeData
  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData>
  class _Node: public ProtoNode<_PredStorage, _SuccStorage, _EdgeData> {
    using Parent = ProtoNode<_PredStorage, _SuccStorage, _EdgeData>;
    _NodeData _data;
  public:
    using NodeData = _NodeData;
    static constexpr bool has_data = true;
    
    // initialize only the data, leaving parents and children empty
    template<class First, class... Args> requires (!std::is_same_v<std::remove_cvref_t<First>, _Node>)
    _Node(First&& first, Args&&... args): _data(std::forward<First>(first), std::forward<Args>(args)...) {
      std::cout << "created node named "<<this->name()<<" at "<<this <<"\n";
    }
    _Node() = default;

    NodeData& data() & { return _data; }
    NodeData&& data() && { return std::move(_data); }
    const NodeData& data() const & { return _data; }
  };

  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _EdgeData>
  class _Node<_PredStorage, _SuccStorage, void, _EdgeData>: public ProtoNode<_PredStorage, _SuccStorage, _EdgeData> {
    using Parent = ProtoNode<_PredStorage, _SuccStorage, _EdgeData>;
  public:
    using NodeData = void;
    static constexpr bool has_data = false;

    // default-initialization ignores all parameters
    template<class First, class... Args> requires (!std::is_same_v<std::remove_cvref_t<First>, _Node>)
    _Node(First&& first, Args&&... args) {}
    _Node() = default;
  };

  // a node may have a label (can be accessed via label())
  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData, class _LabelType = void>
  class Node: public _Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData> {
    using Parent = _Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData>;
    _LabelType _label;
  public:
    using Parent::Parent;
    using LabelType = _LabelType;
    static constexpr bool has_label = true;

    LabelType& label() { return _label; }
    const LabelType& label() const { return _label; }
  };

  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData>
  class Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData, void>: public _Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData> {
    using Parent = _Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData>;
  public:
    using Parent::Parent;
    using LabelType = void;
    static constexpr bool has_label = false;
  };


  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData, class _LabelType>
  using DefaultNode = Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType>;


  // create a new node and return its description
  template<NodeType _Node, class... Args>
  NodeDesc make_node(Args&&... args) { return new _Node(std::forward<Args>(args)...); }

  template<NodeType _Node>
  void delete_node(const NodeDesc u) { delete reinterpret_cast<_Node*>((uintptr_t)u); }

  // create a new node from the data of some other node
  template<NodeType _Node, StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData>
  NodeDesc make_node(const Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData>& other) {
    if constexpr (std::is_void_v<_NodeData> || std::is_void_v<typename _Node::NodeData>)
      return new _Node();
    else
      return new _Node(other.data());
  }

  template<NodeType _Node, StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData>
  NodeDesc make_node(Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData>&& other) {
    if constexpr (std::is_void_v<_NodeData> || std::is_void_v<typename _Node::NodeData>)
      return new _Node();
    else
      return new _Node(std::move(other.data()));
  }

  template<NodeType Node>
  Node& node_of(const NodeDesc x) { return *(reinterpret_cast<Node*>((uintptr_t)x)); }

  template<PhylogenyType Network>
  typename Network::Node& node_of(const NodeDesc x) { return node_of<typename Network::Node>(x); }

  template<NodeType _Node>
  struct NodeAccess {
    static constexpr auto PredStorage = _Node::PredStorage;
    static constexpr auto SuccStorage = _Node::SuccStorage;
    using SuccContainer = typename _Node::SuccContainer;
    using ChildContainer = SuccContainer;
    using PredContainer = typename _Node::PredContainer;
    using ParentContainer = PredContainer;
    using NodeData = typename _Node::NodeData;
    using EdgeData = typename _Node::EdgeData;
    using Node = _Node;
    using Adjacency = typename _Node::Adjacency;
    using Edge = PT::Edge<EdgeData>;
    using EdgeVec = std::vector<Edge>;
    using EdgeSet = std::unordered_set<Edge>;
    template<class T>
    using EdgeMap = std::unordered_map<Edge, T>;

    static constexpr Node& node_of(const NodeDesc& u) { return PT::node_of<Node>(u); }
    Node& operator[](const NodeDesc& u) const { return node_of(u); }
    
    static constexpr std::string& name(const NodeDesc& u) { return node_of(u).name(); }

    static constexpr SuccContainer& successors(const NodeDesc u) { return node_of(u).successors(); }
    static constexpr SuccContainer& children(const NodeDesc u) { return node_of(u).successors(); }

    static constexpr PredContainer& predecessors(const NodeDesc u) { return node_of(u).predecessors(); }
    static constexpr PredContainer& parents(const NodeDesc u) { return node_of(u).predecessors(); }

    static constexpr NodeDesc any_successor(const NodeDesc u) { return node_of(u).any_successor(); }
    static constexpr NodeDesc any_child(const NodeDesc u) { return node_of(u).any_successor(); }
    static constexpr NodeDesc child(const NodeDesc u) { return node_of(u).any_successor(); }

    template<class... Args>
    static constexpr auto add_successor(const NodeDesc u, Args&&... args) { return node_of(u).add_successor(std::forward<Args>(args)...); }
    template<class... Args>
    static constexpr auto add_child(const NodeDesc u, Args&&... args) { return node_of(u).add_successor(std::forward<Args>(args)...); }

    static constexpr size_t remove_successor(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_successor(n); }
    static constexpr size_t remove_child(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_successor(n); }

    static constexpr void remove_any_successor(const NodeDesc u) { return node_of(u).remove_any_successors(); }
    static constexpr void remove_any_child(const NodeDesc u) { return node_of(u).remove_any_successors(); }

    static constexpr NodeDesc any_predecessor(const NodeDesc u) { return node_of(u).any_predecessor(); }
    static constexpr NodeDesc any_parent(const NodeDesc u) { return node_of(u).any_predecessor(); }
    static constexpr NodeDesc parent(const NodeDesc u) { return node_of(u).any_predecessor(); }

    template<class... Args>
    static constexpr auto add_predecessor(const NodeDesc u, Args&&... args) { return node_of(u).add_predecessor(std::forward<Args>(args)...); }
    template<class... Args>
    static constexpr auto add_parent(const NodeDesc u, Args&&... args) { return node_of(u).add_predecessor(std::forward<Args>(args)...); }

    static constexpr size_t remove_predecessor(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_predecessor(n); }
    static constexpr size_t remove_parent(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_predecessor(n); }

    static constexpr void remove_any_predecessor(const NodeDesc u) { return node_of(u).remove_any_predecessor(); }
    static constexpr void remove_any_parent(const NodeDesc u) { return node_of(u).remove_any_predecessor(); }

    static constexpr size_t in_degree(const NodeDesc u) { return node_of(u).in_degree(); }
    static constexpr size_t out_degree(const NodeDesc u) { return node_of(u).out_degree(); }
    static constexpr size_t degree(const NodeDesc u) { return node_of(u).degree(); }
    static constexpr bool is_root(const NodeDesc u) { return node_of(u).is_root(); }
    static constexpr bool is_tree_node(const NodeDesc u) { return node_of(u).is_tree_node(); }
    static constexpr bool is_leaf(const NodeDesc u) { return node_of(u).is_leaf(); }
    static constexpr bool is_suppressible(const NodeDesc u) { return node_of(u).is_suppressible(); }
    static constexpr bool is_inner_node(const NodeDesc u) { return node_of(u).is_inner_node(); }
    static constexpr bool is_isolated(const NodeDesc u) { return node_of(u).is_isolated(); }

    template<AdjacencyType Adj>
    static constexpr void replace_parent(const NodeDesc u, const NodeDesc old_parent, Adj&& new_parent) { node_of(u).replace_parent(old_parent, std::forward<Adj>(new_parent)); }
    template<AdjacencyType Adj>
    static constexpr void replace_child(const NodeDesc u, const NodeDesc old_child, Adj&& new_child) { node_of(u).replace_child(old_child, std::forward<Adj>(new_child)); }

    // convenient way of applying some function to all nodes of a subtree
    template<class Func>
    static constexpr void apply_to_subtree(const NodeDesc u, Func&& function) { node_of(u).apply_to_subtree(std::forward<Func>(function)); }
    static constexpr size_t count_nodes_below(const NodeDesc u) { return node_of(u).count_nodes_below(); }

    // ================ Edges =======================
    using OutEdgeContainer = typename _Node::OutEdgeContainer;
    using ConstOutEdgeContainer = typename _Node::ConstOutEdgeContainer;
    using InEdgeContainer = typename _Node::InEdgeContainer;
    using ConstInEdgeContainer = typename _Node::ConstInEdgeContainer;

    static constexpr OutEdgeContainer out_edges(const NodeDesc u) { return node_of(u).out_edges(); }
    static constexpr InEdgeContainer in_edges(const NodeDesc u) { return node_of(u).in_edges(); }
  };

#ifndef NDEBUG
  inline size_t _ProtoNode::num_names = 0;
  std::ostream& operator<<(std::ostream& os, const NodeDesc& nd) { return os << ((_ProtoNode*)((uintptr_t)nd))->name(); }
#endif

}


