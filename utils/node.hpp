
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

  enum NodeTypeEnum { NODE_TYPE_LEAF, NODE_TYPE_INTERNAL_TREE, NODE_TYPE_INTERNAL_RETI};

#ifdef DEBUGNODES
  struct ProtoNode_ {
    static uintptr_t num_names;
    uintptr_t _name = num_names++;

    // for debugging purposes, we may want to change the name to something more readable, like a successive numbering
    std::string name() const { return std::to_string(_name); }
    NodeDesc get_desc() const noexcept { return this; }
  };
  inline uintptr_t ProtoNode_::num_names = 0;
  std::ostream& operator<<(std::ostream& os, const NodeDesc nd) {
    if(nd != NoNode) return os << (reinterpret_cast<ProtoNode_*>(static_cast<uintptr_t>(nd)))->name(); else return os << "NoNode";
  }
#else
  struct ProtoNode_ {
    static std::string name() { return ""; }
    NodeDesc get_desc() const noexcept { return NodeDesc{this}; }
  };
#endif
  // NOTE: we specifically refrain from polymorphic nodes (one node pointer that may point to a TreeNode or a NetworkNode) because
  //       the only gain would be to save one pointer on TreeNodes at the cost of a vtable for everyone, so not really worth it.
  //       Also, polymorphic access to the predecessors becomes a nightmare if its type is not known at compiletime
  template<StorageEnum PredStorage_, StorageEnum SuccStorage_, class EdgeData_>
  struct ProtoNode:
    public ProtoNode_
  {
    using Adjacency = PT::Adjacency<EdgeData_>;
    using EdgeData = EdgeData_;
    using Edge = PT::Edge<EdgeData>;
    
    static constexpr StorageEnum SuccStorage = SuccStorage_;
    static constexpr StorageEnum PredStorage = PredStorage_;
    static constexpr bool is_defined_tree_node = (PredStorage == singleS);
    static constexpr bool has_edge_data = has_data<Adjacency>;
    static constexpr bool unique_edges = unique_elements<PredStorage> && unique_elements<SuccStorage>;

    using SuccContainer = StorageClass<SuccStorage, Adjacency>;
    using PredContainer = StorageClass<PredStorage, Adjacency>;
    using ChildContainer = SuccContainer;
    using ParentContainer = PredContainer;
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

    const Adjacency& any_successor() const { return mstd::front(successors()); }
    const Adjacency& any_child() const { return any_successor(); }
    const Adjacency& child() const { return any_successor(); }
    Adjacency& any_successor() { return mstd::front(successors()); }
    Adjacency& any_child() { return any_successor(); }
    Adjacency& child() { return any_successor(); }

  protected:
    template<class... Args> auto add_successor(Args&&... args) { return mstd::append(successors(), std::forward<Args>(args)...); }
    template<class... Args> auto add_child(Args&&... args) { return add_successor(std::forward<Args>(args)...); }

    size_t remove_successor(const NodeDesc n) { return std::erase(successors(), n); }
    size_t remove_child(const NodeDesc n) { return remove_successor(n); }
    auto remove_any_successor() { return mstd::value_pop(successors()); }
    auto remove_any_child() { return remove_any_successor(); }

  public:
    const Adjacency& any_predecessor() const { return mstd::front(predecessors()); }
    const Adjacency& any_parent() const { return any_predecessor(); }
    const Adjacency& parent() const { return any_predecessor(); }
    Adjacency& any_predecessor() { return mstd::front(predecessors()); }
    Adjacency& any_parent() { return any_predecessor(); }
    Adjacency& parent() { return any_predecessor(); }

  protected:
    template<class... Args>
    auto add_predecessor(Args&&... args) { return mstd::append(predecessors(), std::forward<Args>(args)...); }
    template<class... Args>
    auto add_parent(Args&&... args) { return add_predecessor(std::forward<Args>(args)...); }
    size_t remove_predecessor(const NodeDesc n) { return predecessors().erase(n); }
    size_t remove_parent(const NodeDesc n) { return remove_predecessor(n); }
    void remove_any_predecessor() { mstd::pop_back(predecessors()); }
    void remove_any_parent() { remove_any_predecessor(); }

  public:
    size_t degree() const { return in_degree() + out_degree(); }
    std::pair<size_t,size_t> degrees() const { return {in_degree(), out_degree()}; }
    bool is_root() const { return predecessors().empty(); }
    bool is_tree_node() const { if constexpr(is_defined_tree_node) return true; else return in_degree() < 2; }
    bool is_reti() const { return not is_tree_node(); }
    bool is_leaf() const { return successors().empty(); }
    bool is_suppressible() const { return (in_degree() == 1) && (out_degree() == 1); }
    bool is_inner_node() const { return !successors().empty(); }
    bool is_isolated() const { return predecessors().empty() && successors().empty(); }
    NodeTypeEnum type_of() const {
      switch(in_degree()) {
        case 2: return NODE_TYPE_INTERNAL_RETI;
        default: return out_degree() == 0 ? NODE_TYPE_LEAF : NODE_TYPE_INTERNAL_TREE;
      }
    }

  protected:
    template<AdjacencyType Adj>
    void replace_parent(const NodeDesc old_parent, Adj&& new_parent) {
      assert(mstd::test(predecessors(), old_parent));
      std::replace(predecessors(), old_parent, forward<Adj>(new_parent));
    }
    template<AdjacencyType Adj>
    void replace_child(const NodeDesc old_child, Adj&& new_child) {
      assert(mstd::test(successors(), old_child));
      std::replace(successors(), old_child, forward<Adj>(new_child));
    }

    void apply_to_subtree(auto&& function, NodeSet& seen) const {
      function(*this);
      for(const NodeDesc v: successors())
        if(mstd::append(seen, v).second){
          node_of<const ProtoNode>(v).apply_to_subtree(function, seen);
        }
    }
    void apply_to_subtree(auto&& function, NodeSet& seen) {
      function(*this);
      for(const NodeDesc v: successors())
        if(mstd::append(seen, v).second)
          node_of<ProtoNode>(v).apply_to_subtree(function, seen);
    }

  public:
    // convenient way of applying some function to all nodes of a subtree
    template<class F>
    void apply_to_subtree(F&& function) const {
      if constexpr (is_defined_tree_node) {
        function(*this);
        for(const NodeDesc v: successors())
          node_of<const ProtoNode>(v).apply_to_subtree(function);
      } else {
        NodeSet seen;
        apply_to_subtree(function, seen);
      }
    }
    template<class F>
    void apply_to_subtree(F&& function) {
      if constexpr (is_defined_tree_node) {
        function(*this);
        for(const NodeDesc v: successors())
          node_of<const ProtoNode>(v).apply_to_subtree(function);
      } else {
        NodeSet seen;
        apply_to_subtree(function, seen);
      }
    }

    size_t count_nodes_below() const {
      size_t result = 0;
      apply_to_subtree([&](const auto&){ ++result; });
      return result;
    }
    auto count_nodes_and_edges_below() const {
      std::pair<size_t, size_t> result{0,0};
      if constexpr (is_defined_tree_node) {
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
    Edge any_outedge() const { assert(!_successors.empty()); return Edge{ get_desc(), any_successor() }; }

    InEdgeContainer in_edges() { return make_inedge_factory<PredContainer>(get_desc(), _predecessors); }
    ConstInEdgeContainer in_edges() const {return make_inedge_factory<const PredContainer>(get_desc(), _predecessors); }
    Edge any_inedge() const { assert(!_predecessors.empty()); return Edge{reverse_edge_tag(), get_desc(), any_predecessor() }; }
    
    // lookup a parent/child
    const Adjacency* find_predecessor(const NodeDesc v) const { return_pointer_lookup(_predecessors, v); }
    Adjacency* find_predecessor(const NodeDesc v) { return_pointer_lookup(_predecessors, v); }
    const Adjacency* find_parent(const NodeDesc v) const { return find_predecessor(v); }
    Adjacency* find_parent(const NodeDesc v) { return find_predecessor(v); }

    const Adjacency* find_successor(const NodeDesc v) const { return_pointer_lookup(_successors, v); }
    Adjacency* find_successor(const NodeDesc v) { return_pointer_lookup(_successors, v); }
    const Adjacency* find_child(const NodeDesc v) const { return find_successor(v); }
    Adjacency* find_child(const NodeDesc v) { return find_successor(v); }

    template<StrictNodeType Node_> friend struct NodeAccess;
  };

  // A Node is a ProtoNode with possible NodeData
  template<StorageEnum PredStorage_, StorageEnum SuccStorage_, class NodeData_, class EdgeData_>
  struct Node_:
    public ProtoNode<PredStorage_, SuccStorage_, EdgeData_>
  {
    using Parent = ProtoNode<PredStorage_, SuccStorage_, EdgeData_>;

  protected:
    NodeData_ _data;

  public:
    using NodeData = NodeData_;
    using Data = NodeData;
    static constexpr bool has_data = true;
    
    Node_() = default;

    // initialize only the data, leaving parents and children empty
    template<class First, class... Args> requires (not NodeType<First> and not mstd::is_any_of<First, Ex_node_data, Ex_node_label>)
    Node_(First&& first, Args&&... args):
      _data(std::forward<First>(first), std::forward<Args>(args)...) {}

    template<NodeFunctionType DataMaker> requires (not DataExtracterType<DataMaker>)
    Node_(DataMaker&& data_maker):
      _data(std::forward<DataMaker>(data_maker)(reinterpret_cast<uintptr_t>(this))) {}

    template<class... Args>
    Node_(const Ex_node_data, Args&&... args):
      Node_(std::forward<Args>(args)...) {}

    NodeData& data() & { return _data; }
    NodeData&& data() && { return std::move(_data); }
    const NodeData& data() const & { return _data; }
  };

  template<StorageEnum PredStorage_, StorageEnum SuccStorage_, class EdgeData_>
  struct Node_<PredStorage_, SuccStorage_, void, EdgeData_>:
    public ProtoNode<PredStorage_, SuccStorage_, EdgeData_>
  {
    using Parent = ProtoNode<PredStorage_, SuccStorage_, EdgeData_>;
    using NodeData = void;
    using Data = void;
    static constexpr bool has_data = false;

    Node_() = default;

    // default-initialization ignores all parameters
    template<class First, class... Args> requires (not NodeType<First>)
    Node_(First&& first, Args&&... args) {}
  };

  // a node may have a label (can be accessed via label())
  // NOTE: has_label only tells that a node MAY have a (possibly empty) label!
  template<StorageEnum PredStorage_, StorageEnum SuccStorage_, class NodeData_, class EdgeData_, class LabelType_ = void>
  struct Node:
    public Node_<PredStorage_, SuccStorage_, NodeData_, EdgeData_>
  {
    using Parent = Node_<PredStorage_, SuccStorage_, NodeData_, EdgeData_>;

  protected:
    LabelType_ _label;

  public:
    using Parent::Parent;
    using LabelType = LabelType_;
    static constexpr bool has_label = true;

    template<class LabelInit, class... Args>
    Node(const Ex_node_label, LabelInit&& label_init, Args&&... args):
      Parent(std::forward<Args>(args)...),
      _label(std::forward<LabelInit>(label_init))
    {}

    LabelType& label() & { return _label; }
    LabelType&& label() && { return _label; }
    const LabelType& label() const & { return _label; }
  };

  template<StorageEnum PredStorage_, StorageEnum SuccStorage_, class NodeData_, class EdgeData_>
  class Node<PredStorage_, SuccStorage_, NodeData_, EdgeData_, void>: public Node_<PredStorage_, SuccStorage_, NodeData_, EdgeData_> {
    using Parent = Node_<PredStorage_, SuccStorage_, NodeData_, EdgeData_>;
  public:
    using Parent::Parent;
    using LabelType = void;
    static constexpr bool has_label = false;
  };

  static_assert(StrictNodeType<Node<vecS, vecS, void, void, std::string>>);

  template<StorageEnum A, StorageEnum B, class C, class D, class E>
  std::ostream& operator<<(std::ostream& os, const Node<A,B,C,D,E>& n){
    os << "Pre: "<<n.parents()<<"\tSuc:"<<n.children();
    if constexpr (n.has_label) os << "\tlabel: "<<n.label();
    return os;
  }


  template<StorageEnum PredStorage_, StorageEnum SuccStorage_, class NodeData_, class EdgeData_, class LabelType_>
  using DefaultNode = Node<PredStorage_, SuccStorage_, NodeData_, EdgeData_, LabelType_>;

  template<NodeType Node>
  Node& node_of(const NodeDesc x) { return *(reinterpret_cast<std::remove_cvref_t<Node>*>(static_cast<uintptr_t>(x))); }

  // NOTE: whatever cvref qualifiers Network may have, they are also applied to the Node, prominently:
  // if Network is a const lvalue ref, then return a const lvalue ref to a Node
  // if Network is an rvalue ref, then return an rvalue ref to a Node
  // if Network is a simple type, then return a (non-const) lvalue reference to a Node
  template<PhylogenyType Network>
  using NodeOf = mstd::copy_cvref_t<Network, typename std::remove_cvref_t<Network>::Node>;
  template<PhylogenyType Network>
  NodeOf<Network>& node_of(const NodeDesc x) { return node_of<NodeOf<Network>>(x); }


  template<StrictPhylogenyType T>
  decltype(auto) children_of(const NodeDesc x) { return node_of<T>(x).children(); }
  template<StrictPhylogenyType T>
  decltype(auto) parents_of(const NodeDesc x) { return node_of<T>(x).parents(); }
  template<StrictPhylogenyType T>
  decltype(auto) any_parent_of(const NodeDesc x) { return node_of<T>(x).any_parent(); }

  template<class Network> struct functor_children_of { decltype(auto) operator()(const NodeDesc u) const { return Network::children(u); } };
  template<class Network> struct functor_parents_of { decltype(auto) operator()(const NodeDesc u) const { return Network::parents(u); } };
  template<class Network> struct functor_any_parent_of { decltype(auto) operator()(const NodeDesc u) const { return Network::any_parent(u); } };


  template<StrictNodeType Node_>
  struct NodeAccess
  {
    // ------- static stuff --------
    static constexpr auto PredStorage = Node_::PredStorage;
    static constexpr auto SuccStorage = Node_::SuccStorage;
    using SuccContainer = typename Node_::SuccContainer;
    using ChildContainer = SuccContainer;
    using PredContainer = typename Node_::PredContainer;
    using ParentContainer = PredContainer;
    using NodeData = typename Node_::NodeData;
    using EdgeData = typename Node_::EdgeData;
    using LabelType = typename Node_::LabelType;
    using Node = Node_;
    using Adjacency = typename Node_::Adjacency;
    using Edge = PT::Edge<EdgeData>;
    using EdgeVec = std::vector<Edge>;
    using EdgeSet = std::unordered_set<Edge>;
    template<class T>
    using EdgeMap = std::unordered_map<Edge, T>;
    
    static constexpr bool has_node_labels = Node::has_label;
    static constexpr bool has_node_data = Node::has_data;
    static constexpr bool has_edge_data = Adjacency::has_data;
    static constexpr bool unique_edges = Node::unique_edges;

    static constexpr Node& node_of(const NodeDesc u) { return PT::node_of<Node>(u); }
    static void delete_node(const NodeDesc u) { delete reinterpret_cast<Node*>(static_cast<uintptr_t>(u)); }

    // create a node in the void
    // NOTE: this only creates a node structure in memory which can then be used with add_root() or add_child() or add_parent() in the tree/network
    template<class... Args> requires ((sizeof...(Args) == 0) or (not DataExtracterType<mstd::FirstTypeOf<Args...>>))
    static constexpr NodeDesc create_node(Args&&... args) {
      DEBUG6(std::cout << "creating node of type "<<mstd::type_name<Node>() << " with " << sizeof...(Args) << " arguments\n");
      Node* result = new Node(std::forward<Args>(args)...);
      DEBUG6(std::cout << "created node at " << result << " (" << reinterpret_cast<uintptr_t>(result) << ")\n");
      return reinterpret_cast<uintptr_t>(result);
    }
    template<DataExtracterType DataMaker> requires (not NodeFunctionType<DataMaker>)
    static constexpr NodeDesc create_node(DataMaker&& data_maker) {
      using StrictDataMaker = std::remove_reference_t<DataMaker>;
      if constexpr (not StrictDataMaker::ignoring_node_data) {
        if constexpr (not StrictDataMaker::ignoring_node_labels) {
          return create_node(Ex_node_label{}, data_maker.get_node_label, data_maker.get_node_data);
        } else return create_node(data_maker.get_node_data);
      } else {
        if constexpr (not StrictDataMaker::ignoring_node_labels) {
          return create_node(Ex_node_label{}, data_maker.get_node_label);
        } else return create_node();
      }
    } 


    static constexpr auto  name(const NodeDesc u) { return node_of(u).name(); }
    static constexpr auto& label(const NodeDesc u) { return node_of(u).label(); }
    static constexpr auto& data(const NodeDesc u) requires (has_node_data) { return node_of(u).data(); }
    static constexpr auto& data(const Adjacency& uv) requires (has_edge_data) { return uv.data(); }
    static constexpr auto& data(const Edge& uv) requires (has_edge_data) { return uv.data(); }
    static constexpr auto& data(const NodeDesc u, Adjacency& v) requires (has_edge_data) { return v.data(); }
    static constexpr auto& data(const NodeDesc u, const Adjacency& v) requires (has_edge_data) { return v.data(); }

    static constexpr SuccContainer& successors(const NodeDesc u) { return node_of(u).successors(); }
    static constexpr SuccContainer& children(const NodeDesc u) { return node_of(u).successors(); }

    static constexpr PredContainer& predecessors(const NodeDesc u) { return node_of(u).predecessors(); }
    static constexpr PredContainer& parents(const NodeDesc u) { return node_of(u).predecessors(); }

    static constexpr Adjacency& any_successor(const NodeDesc u) { return node_of(u).any_successor(); }
    static constexpr Adjacency& any_child(const NodeDesc u) { return node_of(u).any_successor(); }
    static constexpr Adjacency& child(const NodeDesc u) { return node_of(u).any_successor(); }

    static constexpr Adjacency& any_predecessor(const NodeDesc u) { return node_of(u).any_predecessor(); }
    static constexpr Adjacency& any_parent(const NodeDesc u) { return node_of(u).any_predecessor(); }
    static constexpr Adjacency& parent(const NodeDesc u) { return node_of(u).any_predecessor(); }

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
    static constexpr NodeTypeEnum type_of(const NodeDesc u) { return node_of(u).type_of(); }
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
    using OutEdgeContainer = typename Node_::OutEdgeContainer;
    using ConstOutEdgeContainer = typename Node_::ConstOutEdgeContainer;
    using InEdgeContainer = typename Node_::InEdgeContainer;
    using ConstInEdgeContainer = typename Node_::ConstInEdgeContainer;

    static constexpr OutEdgeContainer out_edges(const NodeDesc u) { return node_of(u).out_edges(); }
    static constexpr InEdgeContainer in_edges(const NodeDesc u) { return node_of(u).in_edges(); }

    static constexpr Edge any_outedge(const NodeDesc u) { return node_of(u).any_outedge(); }
    static constexpr Edge any_inedge(const NodeDesc u)  { return node_of(u).any_inedge(); }


    // NOTE: we expect lookups in the parents set to be faster than in the children set
    static bool is_edge(const NodeDesc u, const NodeDesc v)  { return mstd::test(parents(v), u); }
    static bool adjacent(const NodeDesc u, const NodeDesc v) { return mstd::test(parents(u), v) || mstd::test(parents(v), u); }

    static Adjacency* find_predecessor(const NodeDesc u, const NodeDesc v) { return node_of(u).find_predecessor(v); }
    static Adjacency* find_parent(const NodeDesc u, const NodeDesc v) { node_of(u).find_predecessor(v); }

    static Adjacency* find_successor(const NodeDesc u, const NodeDesc v) { return node_of(u).find_successor(v); }
    static Adjacency* find_child(const NodeDesc u, const NodeDesc v) { return node_of(u).find_successor(v); }

    static Edge find_edge(const NodeDesc u, const NodeDesc v) {
      const auto& v_parents = parents(v);
      const auto uv_iter = mstd::find(v_parents, u);
      if(uv_iter != v_parents.end()){
        return Edge(reverse_edge_tag(), v, *uv_iter);
      } else return Edge(NoNode, Adjacency());
    }
    static Edge find_edge_fwd(const NodeDesc u, const NodeDesc v) {
      const auto& u_children = children(u);
      const auto uv_iter = mstd::find(u_children, v);
      if(uv_iter != u_children.end()){
        return Edge(u, *uv_iter);
      } else return Edge(NoNode, Adjacency());
    }

  protected:
    template<class... Args>
    static constexpr auto add_successor(const NodeDesc u, Args&&... args) { return node_of(u).add_successor(std::forward<Args>(args)...); }
    template<class... Args>
    static constexpr auto add_child(const NodeDesc u, Args&&... args) { return node_of(u).add_successor(std::forward<Args>(args)...); }

    static constexpr size_t remove_successor(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_successor(n); }
    static constexpr size_t remove_child(const NodeDesc u, const NodeDesc n) { return node_of(u).remove_successor(n); }

    static constexpr void remove_any_successor(const NodeDesc u) { return node_of(u).remove_any_successors(); }
    static constexpr void remove_any_child(const NodeDesc u) { return node_of(u).remove_any_successors(); }

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
      const auto uv_iter = mstd::find(u_children, v_node.get_desc());
      if(uv_iter != u_children.end()) {
        u_children.erase(uv_iter);

        // step 2: remove v-->u
        auto& v_parents = v_node.parents();
        const auto vu_iter = mstd::find(v_parents, u_node.get_desc());
        assert(vu_iter != v_parents.end());
        v_parents.erase(vu_iter);
        DEBUG5(std::cout << "NodeAccess: removed edge "<<u<<" -> "<<v<<"\n");
        return true;
      } else {
        DEBUG5(std::cout << "NodeAccess: DID NOT REMOVE EDGE "<<u<<" -> "<<v<<"\n");
        return false;
      }
    }



    // ------- members --------
    // ------- construction & desctruction ---------
  public:
    Node& operator[](const NodeDesc u) const & { return node_of(u); }
    Node&& operator[](const NodeDesc u) && { return node_of(u); }

    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: query --------
    // ------- methods: modification --------
  };


  template<PhylogenyType Net>
  struct InternalDataAccess {
    using Adjacency = typename Net::Adjacency;
    using Edge = typename Net::Edge;
    auto& operator()(const NodeDesc u) const requires Net::has_node_data { return node_of<Net>(u).data(); }
    auto& operator()(const NodeDesc u, const Adjacency& v) const requires Net::has_edge_data { return v.data(); }
    auto& operator()(const NodeDesc u, Adjacency& v) const requires Net::has_edge_data { return v.data(); }
    auto& operator()(const Edge& uv) const requires Net::has_edge_data { return uv.data(); }
  };

  template<class NodeData_ = void, class EdgeData_ = void>
  struct ExternalDataAccess {
    static constexpr bool has_node_data = not std::is_void_v<NodeData_>;
    static constexpr bool has_edge_data = not std::is_void_v<EdgeData_>;
    using NodeData = mstd::FirstNonVoid<NodeData_, mstd::monostate>;
    using EdgeData = mstd::FirstNonVoid<EdgeData_, mstd::monostate>;

    [[ no_unique_address ]] std::conditional_t<has_node_data, HashMap<NodeDesc, NodeData>, mstd::monostate> node_data;
    [[ no_unique_address ]] std::conditional_t<has_edge_data, HashMap<NodePair, EdgeData>, mstd::monostate> edge_data;

    auto& operator()(const NodeDesc u) const requires has_node_data { return node_data[u]; }
    auto& operator()(const NodeDesc u, const NodeDesc v) const requires has_edge_data { return edge_data[NodePair{u,v}]; }
    auto& operator()(const NodePair& uv) const requires has_edge_data { return edge_data(uv); }

    auto& operator()(const NodeDesc u) requires has_node_data { return node_data[u]; }
    auto& operator()(const NodeDesc u, const NodeDesc v) requires has_edge_data { return edge_data[NodePair{u,v}]; }
    auto& operator()(const NodePair& uv) requires has_edge_data { return edge_data(uv); }

  };

}



