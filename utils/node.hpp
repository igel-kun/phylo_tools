
#pragma once

#include <vector>         // adjacency storage
#include <set>            // adjacency storage
#include <unordered_set>  // adjacency storage
#include <string>         // node labels

#include "types.hpp"
#include "edge.hpp"
#include "edge_iter.hpp"
#include "adjacency.hpp"

namespace PT{

#ifndef NDEBUG
  struct _ProtoNode {
    static uintptr_t num_names;
    const uintptr_t _name = num_names++;

    // for debugging purposes, we may want to change the name to something more readable, like a successive numbering
    std::string name() const { return std::to_string(_name); }
    NodeDesc get_desc() const { return this; }
  };
  inline uintptr_t _ProtoNode::num_names = 0;
  std::ostream& operator<<(std::ostream& os, const NodeDesc nd) {
    if(nd != NoNode) return os << (reinterpret_cast<_ProtoNode*>(static_cast<uintptr_t>(nd)))->name(); else return os << "NoNode";
  }
#else
  struct _ProtoNode {
    static std::string name() { return ""; }
    NodeDesc get_desc() const { return NodeDesc(this); }
  };
#endif
  // NOTE: we specifically refrain from polymorphic nodes (one node pointer that may point to a TreeNode or a NetworkNode) because
  //       the only gain would be to save one pointer on TreeNodes at the cost of a vtable for everyone, so not really worth it.
  //       Also, polymorphic access to the predecessors becomes a nightmare if its type is not known at compiletime
  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _EdgeData>
  class ProtoNode: public _ProtoNode {
  public:
    using Adjacency = PT::Adjacency<_EdgeData>;
    using EdgeData = _EdgeData;
    using Edge = PT::Edge<EdgeData>;
    static constexpr StorageEnum SuccStorage = _SuccStorage;
    static constexpr StorageEnum PredStorage = _PredStorage;
    static constexpr bool _is_tree_node = (PredStorage == singleS);
    static constexpr bool has_edge_data = has_data<Adjacency>;
    using SuccContainer = StorageClass<SuccStorage, Adjacency>;
    using PredContainer = StorageClass<PredStorage, Adjacency>;
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

    const Adjacency& any_successor() const { return front(successors()); }
    const Adjacency& any_child() const { return any_successor(); }
    const Adjacency& child() const { return any_successor(); }
    Adjacency& any_successor() { return front(successors()); }
    Adjacency& any_child() { return any_successor(); }
    Adjacency& child() { return any_successor(); }

  protected:
    template<class... Args>
    auto add_successor(Args&&... args) { return append(successors(), std::forward<Args>(args)...); }
    template<class... Args>
    auto add_child(Args&&... args) { return add_successor(std::forward<Args>(args)...); }
    size_t remove_successor(const NodeDesc n) { return std::erase(successors(), n); }
    size_t remove_child(const NodeDesc n) { return remove_successor(n); }
    auto remove_any_successor() { return std::value_pop(successors()); }
    auto remove_any_child() { return remove_any_successor(); }

  public:
    const Adjacency& any_predecessor() const { return std::front(predecessors()); }
    const Adjacency& any_parent() const { return any_predecessor(); }
    const Adjacency& parent() const { return any_predecessor(); }
    Adjacency& any_predecessor() { return std::front(predecessors()); }
    Adjacency& any_parent() { return any_predecessor(); }
    Adjacency& parent() { return any_predecessor(); }

  protected:
    template<class... Args>
    auto add_predecessor(Args&&... args) { return append(predecessors(), std::forward<Args>(args)...); }
    template<class... Args>
    auto add_parent(Args&&... args) { return add_predecessor(std::forward<Args>(args)...); }
    size_t remove_predecessor(const NodeDesc n) { return predecessors().erase(n); }
    size_t remove_parent(const NodeDesc n) { return remove_predecessor(n); }
    void remove_any_predecessor() { std::pop(predecessors()); }
    void remove_any_parent() { remove_any_predecessor(); }

  public:
    size_t degree() const { return in_degree() + out_degree(); }
    std::pair<size_t,size_t> degrees() const { return {in_degree(), out_degree()}; }
    bool is_root() const { return predecessors().empty(); }
    bool is_tree_node() const { if constexpr(_is_tree_node) return true; else return in_degree() < 2; }
    bool is_reti() const { return !is_tree_node(); }
    bool is_leaf() const { return successors().empty(); }
    bool is_suppressible() const { return (in_degree() == 1) && (out_degree() == 1); }
    bool is_inner_node() const { return !successors().empty(); }
    bool is_isolated() const { return predecessors().empty() && successors().empty(); }

  protected:
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

  public:
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

    OutEdgeContainer out_edges() { return make_outedge_factory<SuccContainer>(get_desc(), _successors); }
    ConstOutEdgeContainer out_edges() const { return make_outedge_factory<const SuccContainer>(get_desc(), _successors); }
    Edge any_out_edge() const { assert(!_successors.empty()); return Edge{ get_desc(), front(_successors) }; }
    InEdgeContainer in_edges() { return make_inedge_factory<PredContainer>(get_desc(), _predecessors); }
    ConstInEdgeContainer in_edges() const { return make_inedge_factory<const PredContainer>(get_desc(), _predecessors); }
    Edge any_in_edge() const { assert(!_predecessors.empty()); return Edge{reverse_edge_t(), get_desc(), front(_predecessors) }; }

    // lookup a parent/child
    const Adjacency* find_predecessor(const NodeDesc v) const { return_pointer_lookup(_predecessors, v); }
    Adjacency* find_predecessor(const NodeDesc v) { return_pointer_lookup(_predecessors, v); }
    const Adjacency* find_parent(const NodeDesc v) const { return find_predecessor(v); }
    Adjacency* find_parent(const NodeDesc v) { return find_predecessor(v); }

    const Adjacency* find_successor(const NodeDesc v) const { return_pointer_lookup(_successors, v); }
    Adjacency* find_successor(const NodeDesc v) { return_pointer_lookup(_successors, v); }
    const Adjacency* find_child(const NodeDesc v) const { return find_successor(v); }
    Adjacency* find_child(const NodeDesc v) { return find_successor(v); }

    template<StrictNodeType _Node> friend struct NodeAccess;
  };

  // A Node is a ProtoNode with possible NodeData
  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData>
  class _Node: public ProtoNode<_PredStorage, _SuccStorage, _EdgeData> {
    using Parent = ProtoNode<_PredStorage, _SuccStorage, _EdgeData>;
    _NodeData _data;
  public:
    using NodeData = _NodeData;
    using Data = NodeData;
    static constexpr bool has_data = true;
    
    // initialize only the data, leaving parents and children empty
    template<class First, class... Args> requires (!NodeType<First>)
    _Node(First&& first, Args&&... args): _data(std::forward<First>(first), std::forward<Args>(args)...) {}
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
    using Data = void;
    static constexpr bool has_data = false;

    // default-initialization ignores all parameters
    template<class First, class... Args> requires (!NodeType<First>)
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

    template<class LabelInit, class... Args>
    Node(const std::piecewise_construct_t, LabelInit&& label_init, Args&&... args):
      Parent(std::forward<Args>(args)...),
      _label(label_init)
    {}

    LabelType& label() & { return _label; }
    LabelType&& label() && { return _label; }
    const LabelType& label() const & { return _label; }
  };

  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData>
  class Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData, void>: public _Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData> {
    using Parent = _Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData>;
  public:
    using Parent::Parent;
    using LabelType = void;
    static constexpr bool has_label = false;
  };
  template<StorageEnum A, StorageEnum B, class C, class D, class E>
  std::ostream& operator<<(std::ostream& os, const Node<A,B,C,D,E>& n){
    os << "Pre: "<<n.parents()<<"\tSuc:"<<n.children();
    if constexpr (n.has_label) os << "\tlabel: "<<n.label();
    return os;
  }


  template<StorageEnum _PredStorage, StorageEnum _SuccStorage, class _NodeData, class _EdgeData, class _LabelType>
  using DefaultNode = Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType>;

  template<StrictNodeType Node>
  Node& node_of(const NodeDesc x) { return *(reinterpret_cast<Node*>(static_cast<uintptr_t>(x))); }

  // NOTE: whatever cvref qualifiers Network may have, they are also applied to the Node, prominently:
  // if Network is a const lvalue ref, then return a const lvalue ref to a Node
  // if Network is an rvalue ref, then return an rvalue ref to a Node
  // if Network is a simple type, then return a (non-const) lvalue reference to a Node
  template<PhylogenyType Network>
  std::copy_cvref_t<Network, typename Network::Node>& node_of(const NodeDesc x) {
    return node_of<typename Network::Node>(x);
  }


  template<StrictPhylogenyType T>
  auto& children_of(const NodeDesc x) { return node_of<T>(x).children(); }
  template<StrictPhylogenyType T>
  auto& parents_of(const NodeDesc x) { return node_of<T>(x).parents(); }
  template<StrictPhylogenyType T>
  auto& any_parent_of(const NodeDesc x) { return node_of<T>(x).any_parent(); }



  template<StrictNodeType _Node>
  struct NodeAccess {
    static constexpr auto PredStorage = _Node::PredStorage;
    static constexpr auto SuccStorage = _Node::SuccStorage;
    using SuccContainer = typename _Node::SuccContainer;
    using ChildContainer = SuccContainer;
    using PredContainer = typename _Node::PredContainer;
    using ParentContainer = PredContainer;
    using NodeData = typename _Node::NodeData;
    using EdgeData = typename _Node::EdgeData;
    using LabelType = typename _Node::LabelType;
    using Node = _Node;
    using Adjacency = typename _Node::Adjacency;
    using Edge = PT::Edge<EdgeData>;
    using EdgeVec = std::vector<Edge>;
    using EdgeSet = std::unordered_set<Edge>;
    template<class T>
    using EdgeMap = std::unordered_map<Edge, T>;
    
    static constexpr bool has_node_labels = Node::has_label;
    static constexpr bool has_node_data = Node::has_data;
    static constexpr bool has_edge_data = Adjacency::has_data;

    static constexpr Node& node_of(const NodeDesc u) { return PT::node_of<Node>(u); }

    void delete_node(const NodeDesc u) { delete reinterpret_cast<Node*>(static_cast<uintptr_t>(u)); }

    Node& operator[](const NodeDesc u) const & { return node_of(u); }
    Node&& operator[](const NodeDesc u) && { return node_of(u); }
    
    static constexpr auto  name(const NodeDesc u) { return node_of(u).name(); }
    static constexpr auto& label(const NodeDesc u) { return node_of(u).label(); }

    static constexpr SuccContainer& successors(const NodeDesc u) { return node_of(u).successors(); }
    static constexpr SuccContainer& children(const NodeDesc u) { return node_of(u).successors(); }

    static constexpr PredContainer& predecessors(const NodeDesc u) { return node_of(u).predecessors(); }
    static constexpr PredContainer& parents(const NodeDesc u) { return node_of(u).predecessors(); }

    static constexpr Adjacency& any_successor(const NodeDesc u) { return node_of(u).any_successor(); }
    static constexpr Adjacency& any_child(const NodeDesc u) { return node_of(u).any_successor(); }
    static constexpr Adjacency& child(const NodeDesc u) { return node_of(u).any_successor(); }

  protected:
    template<class... Args>
    static constexpr auto add_successor(const NodeDesc u, Args&&... args) { return node_of(u).add_successor(std::forward<Args>(args)...); }
    template<class... Args>
    static constexpr auto add_child(const NodeDesc u, Args&&... args) { return node_of(u).add_successor(std::forward<Args>(args)...); }

    static constexpr size_t remove_successor(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_successor(n); }
    static constexpr size_t remove_child(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_successor(n); }

    static constexpr void remove_any_successor(const NodeDesc u) { return node_of(u).remove_any_successors(); }
    static constexpr void remove_any_child(const NodeDesc u) { return node_of(u).remove_any_successors(); }

  public:
    static constexpr Adjacency& any_predecessor(const NodeDesc u) { return node_of(u).any_predecessor(); }
    static constexpr Adjacency& any_parent(const NodeDesc u) { return node_of(u).any_predecessor(); }
    static constexpr Adjacency& parent(const NodeDesc u) { return node_of(u).any_predecessor(); }

  protected:
    template<class... Args>
    static constexpr auto add_predecessor(const NodeDesc u, Args&&... args) { return node_of(u).add_predecessor(std::forward<Args>(args)...); }
    template<class... Args>
    static constexpr auto add_parent(const NodeDesc u, Args&&... args) { return node_of(u).add_predecessor(std::forward<Args>(args)...); }

    static constexpr size_t remove_predecessor(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_predecessor(n); }
    static constexpr size_t remove_parent(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_predecessor(n); }

    static constexpr void remove_any_predecessor(const NodeDesc u) { return node_of(u).remove_any_predecessor(); }
    static constexpr void remove_any_parent(const NodeDesc u) { return node_of(u).remove_any_predecessor(); }

    static constexpr bool remove_edge(const NodeDesc u, const NodeDesc v) {
      Node& u_node = node_of(u);
      Node& v_node = node_of(v);
      // step 1: remove u-->v & free the edge data
      auto& u_children = u_node.children();
      const auto uv_iter = find(u_children, v_node.get_desc());
      if(uv_iter != u_children.end()) {
        u_children.erase(uv_iter);

        // step 2: remove v-->u
        auto& v_parents = v_node.parents();
        const auto vu_iter = find(v_parents, u_node.get_desc());
        assert(vu_iter != v_parents.end());
        v_parents.erase(vu_iter);
        DEBUG3(std::cout << "NodeAccess: removed edge "<<u<<" -> "<<v<<"\n");
        return true;
      } else {
        DEBUG3(std::cout << "NodeAccess: DID NOT REMOVE EDGE "<<u<<" -> "<<v<<"\n");
        return false;
      }
    }


  public:
    static constexpr size_t in_degree(const NodeDesc u) { return node_of(u).in_degree(); }
    static constexpr size_t out_degree(const NodeDesc u) { return node_of(u).out_degree(); }
    static constexpr size_t degree(const NodeDesc u) { return node_of(u).degree(); }
    static constexpr std::pair<size_t,size_t> degrees(const NodeDesc u) { return node_of(u).degrees(); }
    static constexpr bool is_root(const NodeDesc u) { return node_of(u).is_root(); }
    static constexpr bool is_tree_node(const NodeDesc u) { return node_of(u).is_tree_node(); }
    static constexpr bool is_reti(const NodeDesc u) { return node_of(u).is_reti(); }
    static constexpr bool is_leaf(const NodeDesc u) { return node_of(u).is_leaf(); }
    static constexpr bool is_suppressible(const NodeDesc u) { return node_of(u).is_suppressible(); }
    static constexpr bool is_inner_node(const NodeDesc u) { return node_of(u).is_inner_node(); }
    static constexpr bool is_isolated(const NodeDesc u) { return node_of(u).is_isolated(); }
/*
    template<AdjacencyType Adj>
    static constexpr void replace_parent(const NodeDesc u, const NodeDesc old_parent, Adj&& new_parent)
    { node_of(u).replace_parent(old_parent, std::forward<Adj>(new_parent)); }
    template<AdjacencyType Adj>
    static constexpr void replace_child(const NodeDesc u, const NodeDesc old_child, Adj&& new_child)
    { node_of(u).replace_child(old_child, std::forward<Adj>(new_child)); }
*/
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
    static constexpr Edge any_out_edge(const NodeDesc u) { return node_of(u).any_out_edge(); }
    static constexpr InEdgeContainer in_edges(const NodeDesc u) { return node_of(u).in_edges(); }
    static constexpr Edge any_in_edge(const NodeDesc u) { return node_of(u).any_in_edge(); }

    // NOTE: we expect lookups in the parents set to be faster than in the children set
    static bool is_edge(const NodeDesc u, const NodeDesc v)  { return test(parents(v), u); }
    static bool adjacent(const NodeDesc u, const NodeDesc v) { return test(parents(u), v) || test(parents(v), u); }

    static Adjacency* find_predecessor(const NodeDesc u, const NodeDesc v) { return node_of(u).find_predecessor(v); }
    static Adjacency* find_parent(const NodeDesc u, const NodeDesc v) { node_of(u).find_predecessor(v); }

    static Adjacency* find_successor(const NodeDesc u, const NodeDesc v) { return node_of(u).find_successor(v); }
    static Adjacency* find_child(const NodeDesc u, const NodeDesc v) { return node_of(u).find_successor(v); }
  };

}



