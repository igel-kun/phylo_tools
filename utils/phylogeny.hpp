
#pragma once

#include <ranges>
#include "tuple_iter.hpp"
#include "set_interface.hpp"

#include "utils.hpp"
#include "types.hpp"
#include "lca.hpp"
#include "dfs.hpp"
#include "except.hpp"
#include "node.hpp"
#include "edge.hpp"
#include "induced_tree.hpp"
#include "edge_emplacement.hpp"

namespace PT {
  template<StrictPhylogenyType Phylo, bool move_data = false>
  struct _CopyOrMoveDataFunc {
    using NodeData = NodeDataOr<Phylo>;
    using EdgeData = EdgeDataOr<Phylo>;
    using NodeDataReturn = std::conditional_t<move_data && std::remove_reference_t<Phylo>::has_node_data, NodeData&&, NodeData>;
    using EdgeDataReturn = std::conditional_t<move_data && std::remove_reference_t<Phylo>::has_edge_data, EdgeData&&, EdgeData>;
    using Node = typename std::remove_reference_t<Phylo>::Node;
    using Edge = typename std::remove_reference_t<Phylo>::Edge;

    NodeDataReturn operator()(const NodeDesc& u) const { if constexpr (Node::has_data) return node_of<Phylo>(u).data(); else return NodeData(); }
    EdgeDataReturn operator()(const Edge& uv) const { if constexpr (Edge::has_data) return uv.data(); else return EdgeData(); }
  };
  template<PhylogenyType Phylo>
  using CopyOrMoveDataFunc = _CopyOrMoveDataFunc<std::remove_reference_t<Phylo>, std::is_rvalue_reference_v<Phylo&&>>;


	// ================ ProtoPhylogeny ======================
	// in the proto phylogeny, differences between trees (singleS predecessor storage)
	// and networks manifest (for example, for networks, we have to count the number of edges)

	// general proto phylogeny implementing node and edge interaction
  template<StorageEnum _PredStorage,
           StorageEnum _SuccStorage,
           class _NodeData = void,
           class _EdgeData = void,
           class _LabelType = void,
           StorageEnum _RootStorage = singleS,
           template<StorageEnum, StorageEnum, class, class, class> class _Node = PT::DefaultNode>
  class ProtoPhylogeny: public NodeAccess<_Node<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType>> {
  public:
    static constexpr StorageEnum RootStorage = _RootStorage;
    static constexpr bool has_unique_root = (RootStorage == singleS);
    using RootContainer = StorageClass<_RootStorage, NodeDesc>;
    using DefaultSeen = NodeSet;
  protected:
    RootContainer _roots;
		size_t _num_nodes = 0u;
		size_t _num_edges = 0u;

	public:
    static constexpr bool is_declared_tree = false;

		void count_node(const int nr = 1) { _num_nodes += nr; } 
		void count_edge(const int nr = 1) { _num_edges += nr; }

    bool is_forest() const { return _num_nodes == _num_edges + _roots.size(); }
		size_t num_nodes() const { return _num_nodes; }
		size_t num_edges() const { return _num_edges; }
    NodeDesc root() const { return front(_roots); }
    const RootContainer& roots() const { return _roots; }
	};

	// proto phylogeny with maximum in-degree 1 (for use as trees/forests)
  template<StorageEnum _SuccStorage,
           class _NodeData,
           class _EdgeData,
           class _LabelType,
           StorageEnum _RootStorage,
           template<StorageEnum, StorageEnum, class, class, class> class _Node>
  class ProtoPhylogeny<singleS, _SuccStorage, _NodeData, _EdgeData, _LabelType, _RootStorage, _Node>:
    public NodeAccess<_Node<singleS, _SuccStorage, _NodeData, _EdgeData, _LabelType>> {
  public:
    static constexpr StorageEnum RootStorage = _RootStorage;
    static constexpr bool has_unique_root = (RootStorage == singleS);
    using RootContainer = StorageClass<_RootStorage, NodeDesc>;
    using DefaultSeen = void;
  protected:
    RootContainer _roots;
		size_t _num_nodes = 0;

  public:
    static constexpr bool is_declared_tree = true;

		void count_node(const int nr = 1) { _num_nodes += nr; }
		static constexpr void count_edge(const int nr = 1) {}

    static constexpr bool is_forest() { return true; }
		size_t num_nodes() const { return _num_nodes; }
		size_t num_edges() const { return (_num_nodes == 0) ? 0 : _num_nodes - 1; }
    NodeDesc root() const { return front(_roots); }
    const RootContainer& roots() const { return _roots; }

    //! return whether there is a directed path from x to y in the tree
    bool has_path(const NodeDesc x, const NodeDesc y) const {
      static_assert(is_declared_tree);
      while(1){
        if(y == x) return true;
        const auto& p = this->parents(y);
        if(p.empty()) return false;
        y = p.front();
      } 
    }
	};


  template<StorageEnum _PredStorage,
           StorageEnum _SuccStorage,
           class _NodeData = void,
           class _EdgeData = void,
           class _LabelType = void,
           StorageEnum _RootStorage = singleS,
           template<StorageEnum, StorageEnum, class, class, class> class _Node = PT::DefaultNode>
  class Phylogeny: public ProtoPhylogeny<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType, _RootStorage, _Node> {
    using Parent = ProtoPhylogeny<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType, _RootStorage, _Node>;
  public:
    using typename Parent::Node;
    using typename Parent::RootContainer;
    using LabelType = typename Node::LabelType;
    using typename Parent::Adjacency;
    using typename Parent::Edge;
    using typename Parent::NodeData;
    using typename Parent::EdgeData;
    using typename Parent::DefaultSeen;
    using Parent::is_declared_tree;
    using EdgeVec = PT::EdgeVec<EdgeData>;
    using EdgeSet = PT::EdgeSet<EdgeData>;
    using Parent::node_of;
    using Parent::parents;
    using Parent::parent;
    using Parent::children;
    using Parent::in_degree;
    using Parent::out_degree;
    using Parent::in_edges;
    using Parent::out_edges;
		using Parent::num_edges;
		using Parent::_num_nodes;
    using Parent::name;
    using Parent::label;
    using Parent::root;
    using Parent::_roots;
    using Parent::is_reti;
    using Parent::is_leaf;
		using Parent::count_node;
		using Parent::count_edge;
   
    using IgnoreNodeDataFunc = std::IgnoreFunction<NodeDataOr<Phylogeny>>;
    using IgnoreEdgeDataFunc = std::IgnoreFunction<EdgeDataOr<Phylogeny>>;

    template<PhylogenyType OtherPhylo>
    using CopyOrMoveNodeDataFunc = std::conditional_t<std::remove_reference_t<OtherPhylo>::has_node_data, CopyOrMoveDataFunc<OtherPhylo>, IgnoreNodeDataFunc>;
    template<PhylogenyType OtherPhylo>
    using CopyOrMoveEdgeDataFunc = std::conditional_t<std::remove_reference_t<OtherPhylo>::has_edge_data, CopyOrMoveDataFunc<OtherPhylo>, IgnoreNodeDataFunc>;

    // ================ modification ======================
    // create a node in the void
    // NOTE: this is static and, as such, does not change the network,
    //       indeed this only creates a node structure in memory which can then be used with add_root() or add_child() or add_parent())
    template<class... Args> static constexpr NodeDesc create_node(Args&&... args) {
      return (NodeDesc)(new Node(std::forward<Args>(args)...));
    }

#warning TODO: clean up when the phylogeny is destroyed! In particular delete nodes!
    void delete_node(const NodeDesc x) {
			count_node(-1);
      PT::delete_node<Node>(x);
    }

    // add an edge between two nodes of the tree/network
    // NOTE: the nodes are assumed to have been added by add_root(), add_child(), or add_parent() (of another node) before!
	  // NOTE: Edge data can be passed either in the Adjacency v or with additional args (the latter takes preference)
    template<AdjacencyType Adj, class... Args>
    std::pair<Adjacency*, bool> add_edge(const NodeDesc u, Adj&& v, Args&&... args) {
      const auto [iter, success] = node_of(u).add_child(std::forward<Adj>(v), std::forward<Args>(args)...);
      if(success) {
        const bool res = node_of(v).add_parent(u, *iter).second;
        assert(res);
				count_edge();
        return {&(*iter), true};
      } else return {nullptr, false};
    }

    void delete_edge(Node& u_node, Node& v_node) {
      // step 1: remove u-->v & free the edge data
      auto& u_children = u_node.children();
      const auto uv_iter = find(u_children, (NodeDesc)(&v_node));
      assert(uv_iter != u_children.end());
      uv_iter->free_edge_data();
      u_children.erase(uv_iter);

      // step 2: remove v-->u
      auto& v_parents = v_node.parents();
      const auto vu_iter = find(v_parents, (NodeDesc)(&u_node));
      assert(vu_iter != v_parents.end());
      v_parents.erase(vu_iter);

      // step 3: count the missing edge
      count_edge(-1);
    }
    void delete_edge(Node& u_node, const NodeDesc v) { delete_edge(u_node, node_of(v)); }
    void delete_edge(const NodeDesc u, Node& v_node) { delete_edge(node_of(u), v_node); }
    void delete_edge(const NodeDesc u, const NodeDesc v) { delete_edge(node_of(u), node_of(v)); }

    // new nodes can only be added as 1. new roots, 2. children or 3. parents of existing nodes
    // NOTE: the edge data can either be passed using the Adjacency v or constructed from the parameter args
    template<AdjacencyType Adj, class... Args>
    std::pair<Adjacency*, bool> add_child(const NodeDesc u, Adj&& v, Args&&... args) {
      assert(u != (NodeDesc)v);
      DEBUG5(std::cout << "adding edge from " << u << " to " << v <<"\n");
      const auto result = add_edge(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      count_node(result.second);
      return result;
    }

    template<AdjacencyType Adj, class... Args>
    bool add_parent(Adj&& v, const NodeDesc u, Args&&... args) {
      const auto result = add_edge(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      if(result.second) {
				count_node();
        erase(_roots, v);
        append(_roots, u);
      }
      return result;
    }

    // this marks a node as new root
    bool mark_root(const NodeDesc& u) {
      assert(in_degree(u) == 0);
      const bool result = append(_roots, u).second;
      return result;
    };

    template<class... Args>
    NodeDesc add_root(Args&&... args) {
      const NodeDesc new_root = create_node(std::forward<Args>(args)...);
      DEBUG5(std::cout << "adding "<<new_root<<" to roots\n");
      const bool result = mark_root(new_root);
      assert(result);
      count_node();
      return new_root;
    }



    // subdivide an edge uv with a new Adjacency (Node + EdgeData) w
    // NOTE: the edge uw gets its data from the Adjacency w and
    //       the edge wv gets its data from either the Adjacency v or the parameter args (the latter takes preference)
    template<AdjacencyType Adj, class... Args>
    void subdivide_edge(const NodeDesc u, Adj&& v, Adj&& w, Args&&... args) {
      assert(is_edge(u,v));
      assert(node_of(v).in_degree() == 1);
      assert(node_of(w).is_isolated());
      delete_edge(u,v);
      add_edge(u, std::forward<Adj>(w));
      add_edge(w, std::forward<Adj>(v), std::forward<Args>(args)...);
			count_node();
    }

    // transfer all children of source_node to target
    // NOTE: this will not create loops
    // NOTE: no check is performed whether u already has some of them as children - if the child storage cannot exclude duplicates, then this can lead to double edges!
    // NOTE: the AdjAdaptor can be used to merge the uv EdgeData and the vw EdgeData into the uw EdgeData (by default the uv EdgeData is ignored)
    template<class AdjAdaptor>
    void transfer_children(Node& source_node, const Adjacency& target, AdjAdaptor&& adapt = [](const Adjacency& uv, Adjacency& vw){}) {
      auto& t_children = children(target);
      auto& s_children = source_node.children();
      
      while(!s_children.empty()) {
        auto sc_iter = std::rbegin(s_children);
        if(*sc_iter == target)
          if(++sc_iter == std::rend(s_children)) break;
        const NodeDesc w = *sc_iter;
        auto& w_parents = parents(w);
        // step 1: move EdgeData out of the parents-storage
        Adjacency vw = std::value_pop(w_parents, &source_node);
        // step 2: merge EdgeData with that of uv
        adapt(target, vw);
        // step 3: put back the Adjacency with changed node
        const auto [wpar_iter, success] = append(w_parents, static_cast<NodeDesc>(target), std::move(vw));
        // step 4: add w as a new child of the target
        if(success) append(t_children, w, *wpar_iter);
        erase(s_children, sc_iter);
      }
    }
    template<class AdjAdaptor>
    void transfer_children(const NodeDesc source, const Adjacency& target, AdjAdaptor&& adapt = [](const Adjacency& wv, Adjacency& uv){}) {
      transfer_children(node_of(source), target, std::forward<AdjAdaptor>(adapt));
    }
    template<class AdjAdaptor>
    void transfer_parents(Node& source_node, const Adjacency& target, AdjAdaptor&& adapt = [](const Adjacency& wv, Adjacency& uv){}) {
      auto& t_parents = parents(target);
      auto& s_parents = source_node.parents();
      
      while(!s_parents.empty()) {
        auto sp_iter = std::rbegin(s_parents);
        if(*sp_iter == target)
          if(++sp_iter == std::rend(s_parents)) break;
        const NodeDesc w = *sp_iter;
        auto& w_children = children(w);
        // step 1: move EdgeData out of the parents-storage
        Adjacency wv = std::value_pop(w_children, &source_node);
        // step 2: merge EdgeData with that of uv
        adapt(wv, target);
        // step 3: put back the Adjacency with changed node
        const auto [wc_iter, success] = append(w_children, static_cast<NodeDesc>(target), std::move(wv));
        // step 4: add w as a new parent of the target
        if(success) append(t_parents, w, *wc_iter);
        erase(s_parents, sp_iter);
      }
    }
    template<class AdjAdaptor>
    void transfer_parents(const NodeDesc source, const Adjacency& target, AdjAdaptor&& adapt = [](const Adjacency& wv, Adjacency& uv){}) {
      transfer_parents(node_of(source), target, std::forward<AdjAdaptor>(adapt));
    }

    // try to contract a node onto its parent; return false if it has not exactly one parent
    template<class AdjAdaptor>
    bool contract_up(const NodeDesc v, AdjAdaptor&& adapt = [](const Adjacency& uv, Adjacency& vw){} ) {
      Node& v_node = node_of(v);
      auto& v_parents = v_node.parents();
      if(v_parents.size() == 1) {
        auto& u_adj = front(v_parents);
        const NodeDesc u = u_adj;
        transfer_children(v_node, std::move(u_adj), std::forward<AdjAdaptor>(adapt));
        // finally, remove the edge uv and free v's storage
        delete_edge(u, v);
        delete_node(v);
        return true;
      } else return false;
    }
    // try to contract a node onto its child; return false if it has not exactly one child
    template<class AdjAdaptor>
    bool contract_down(const Adjacency& u, AdjAdaptor&& adapt = [](const Adjacency& uv, Adjacency& vw){} ) {
      Node& u_node = node_of(u);
      auto& u_children = u_node.children();
      if(u_children.size() == 1) {
        auto& v_adj = front(u_children);
        const NodeDesc v = v_adj;
        if(u_node.is_root()) {
          const auto iter = find(_roots, u);
          assert(iter != _roots.end());
          std::replace(_roots, iter, v);
        } else transfer_parents(u_node, std::move(v_adj), std::forward<AdjAdaptor>(adapt));
        // finally, remove the edge uv and free u's storage
        delete_edge(u, v);
        delete_node(u);
        return true;
      } else return false;
    }
    template<class AdjAdaptor>
    void suppress_node(const NodeDesc v, AdjAdaptor&& adapt = [](const Adjacency& uv, Adjacency& vw){} ) {
      contract_up(v, std::forward<AdjAdaptor>(adapt));
    }
    void remove_node(Node& v_node) {
      // step 1: remove outgoing arcs of v
      const auto& v_children = v_node.children();
      while(!v_children.empty())
        delete_edge(v_node, front(v_children));
      // step 2: remove incoming arcs of v
      const auto& v_parents = v_node.parents();
      if(!v_parents.empty()){
        while(!v_parents.empty())
          delete_edge(front(v_parents), v_node);
      } else _roots.erase(&v_node);
      // step 3: free storage
      delete_node(v_node);
    }
    void remove_node(const NodeDesc v) { remove_node(node_of(v)); }
    // if we know that v is a leaf, we can remove v a bit faster
    void remove_leaf(Node& v_node) {
      assert(v_node.is_leaf());
      // step 2: remove incoming arcs of v
      const auto& v_parents = v_node.parents();
      if(!v_parents.empty()){
        while(!v_parents.empty())
          delete_edge(front(v_parents), v_node);
      } else _roots.erase(&v_node);
      // step 3: free storage
      delete_node(&v_node);
    }  
    void remove_leaf(const NodeDesc& v) { remove_leaf(node_of(v)); }

    template<class LastRites = std::IgnoreFunction<void>>
    void remove_leaf(const auto& v, const LastRites& goodbye = LastRites()) {
      goodbye(v);
      remove_leaf(v);
    }

    // remove the subtree rooted at u
    // NOTE: LastRites can be used to say goodbye to your node(s) (remove it from other containers or whatever)
    template<class... Args>
    void remove_subtree(const NodeDesc u, Args&&... args) {
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(back(C), std::forward<Args>(args)...);
      remove_leaf(u, std::forward<Args>(args)...);
    }
    
    template<class... Args>
    void clear(Args&&... args) {
      for(const NodeDesc r: _roots) remove_subtree(r, std::forward<Args>(args)...);
      _roots.clear();
    }

    template<class... Args>
    void remove_subtree_except(const NodeDesc u, const NodeDesc except, Args&&... args) {
      if(u != except) {
        const auto& C = children(u);
        auto C_iter = std::rbegin(C); // we will likely see vectors as child-containers and those are better treated from their back
        while(C_iter != std::rend(C)){
          const auto next_iter = std::next(C_iter);
          remove_subtree_except(*C_iter, except, std::forward<Args>(args)...);
          C_iter = std::move(next_iter);
        }
        if(C.empty())
          remove_leaf(u, std::forward<Args>(args)...);
      } else remove_subtree_except_root(u, std::forward<Args>(args)...);
    }

    // remove the subtree rooted at u, but leave u be
    template<class... Args>
    void remove_subtree_except_root(const NodeDesc u, Args&&... args) {
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(back(C), std::forward<Args>(args)...);
    }
    
    // =============== variable query ======================
    bool empty() const { return _num_nodes == 0; }
    bool edgeless() const { return num_edges() == 0; }


    // =============== traversals ======================

    // list all nodes below u in order _o (default: postorder)
    template<TraversalType o = postorder>
    static auto nodes_below(const NodeDesc& u, const order<o> _o = order<o>()) { return NodeTraversal<o, Phylogeny>(u); }
    // we can represent nodes that are forbidden to visit by passing either a container of forbidden nodes or a node-predicate
    template<class Forbidden, TraversalType o = postorder>
    static auto nodes_below(const NodeDesc& u, Forbidden&& forbidden, const order<o> _o = order<o>()) {
      return NodeTraversal<o, Phylogeny, void, DefaultSeen, Forbidden>(u, std::forward<Forbidden>(forbidden));
    }
    // we allow passing an iteratable of nodes to traverse from
    template<NodeIterableType Roots, TraversalType o = postorder>
    static auto nodes_below(Roots&& R, const order<o> _o = order<o>()) {
      if constexpr (std::SingletonSetType<Roots>) {
        return nodes_below(front(R), _o); 
      } else {
        return NodeTraversal<o, Phylogeny, Roots>(std::forward<Roots>(R));
      }
    }
    template<NodeIterableType Roots, NodeFunctionType ForbiddenPred, TraversalType o = postorder>
    static auto nodes_below(Roots&& R, ForbiddenPred&& forbidden, const order<o> _o = order<o>()) {
      if constexpr (std::SingletonSetType<Roots>) {
        return nodes_below(front(R), std::forward<ForbiddenPred>(forbidden), _o); 
      } else {
        return NodeTraversal<o, Phylogeny, Roots, DefaultSeen, ForbiddenPred>(std::forward<Roots>(R), std::forward<ForbiddenPred>(forbidden));
      }
    }

    template<class... Args> static auto nodes_below_preorder(Args&&... args)  { return nodes_below(std::forward<Args>(args)..., pre_order); }
    template<class... Args> static auto nodes_below_inorder(Args&&... args)   { return nodes_below(std::forward<Args>(args)..., in_order); }
    template<class... Args> static auto nodes_below_postorder(Args&&... args) { return nodes_below(std::forward<Args>(args)..., post_order); }

    template<TraversalType o = postorder, class... Args>
    auto nodes(Args&&... args) const { return nodes_below<const RootContainer&, o>(_roots, std::forward<Args>(args)...); }
    template<class... Args> auto nodes_preorder(Args&&... args) const  { return nodes<preorder>(std::forward<Args>(args)...); }
    template<class... Args> auto nodes_inorder(Args&&... args) const   { return nodes<inorder>(std::forward<Args>(args)...); }
    template<class... Args> auto nodes_postorder(Args&&... args) const { return nodes<postorder>(std::forward<Args>(args)...); }



    template<TraversalType o = postorder>
    static auto edges_below(const NodeDesc& u, const order<o> _o = order<o>()) {
      return AllEdgesTraversal<o, Phylogeny>(u);
    }
    template<class Forbidden, TraversalType o = postorder>
    static auto edges_below(const NodeDesc& u, Forbidden&& forbidden, const order<o> _o = order<o>()) {
      return AllEdgesTraversal<o, Phylogeny, void, DefaultSeen, Forbidden>(u, std::forward<Forbidden>(forbidden));
    }
    template<NodeIterableType Roots, TraversalType o = postorder>
    static auto edges_below(Roots&& R, const order<o> _o = order<o>()) {
      if constexpr (std::SingletonSetType<Roots>) {
        return edges_below(front(R), _o); 
      } else {
        return AllEdgesTraversal<o, Phylogeny, Roots>(std::forward<Roots>(R));
      }
    }
    template<NodeIterableType Roots, NodeFunctionType ForbiddenPred, TraversalType o = postorder>
    static auto edges_below(Roots&& R, ForbiddenPred&& forbidden, const order<o> _o = order<o>()) {
      if constexpr (std::SingletonSetType<Roots>) {
        return edges_below(front(R), std::forward<ForbiddenPred>(forbidden), _o); 
      } else {
        return AllEdgesTraversal<o, Phylogeny, Roots, DefaultSeen, ForbiddenPred>(std::forward<Roots>(R), std::forward<ForbiddenPred>(forbidden));
      }
    }

    template<class... Args> static auto edges_below_preorder(Args&&... args)  { return edges_below(std::forward<Args>(args)..., pre_order); }
    template<class... Args> static auto edges_below_inorder(Args&&... args)   { return edges_below(std::forward<Args>(args)..., in_order); }
    template<class... Args> static auto edges_below_postorder(Args&&... args) { return edges_below(std::forward<Args>(args)..., post_order); }

    template<TraversalType o = postorder, class... Args>
    auto edges(Args&&... args) const { return edges_below<const RootContainer&, o>(_roots, std::forward<Args>(args)...); }
    template<class... Args> auto edges_preorder(Args&&... args) const  { return edges<preorder>(std::forward<Args>(args)...); }
    template<class... Args> auto edges_inorder(Args&&... args) const   { return edges<inorder>(std::forward<Args>(args)...); }
    template<class... Args> auto edges_postorder(Args&&... args) const { return edges<postorder>(std::forward<Args>(args)...); }
    template<class... Args> auto edges_tail_postorder(Args&&... args) const { return edges<tail_postorder>(std::forward<Args>(args)...); }


    template<class... Args>
    static auto leaves_below(Args&&... args) {
      return std::make_filtered_factory(nodes_below_preorder(std::forward<Args>(args)...).begin(), [](const NodeDesc& n){return node_of(n).is_leaf();});
    }
    template<class... Args>
    auto leaves(Args&&... args) const { return leaves_below<const RootContainer&>(_roots, std::forward<Args>(args)...); }




    // ========================= LCA ===========================
    NaiveLCAOracle<Phylogeny> naiveLCA() const { return *this; }
#warning TODO: use more efficient LCA
    NaiveLCAOracle<Phylogeny> LCA() const { return naiveLCA(); }

    // return the descendant among x and y unless they are incomparable; return NoNode in this case
    NodeDesc get_minimum(const NodeDesc x, const NodeDesc y) const {
      const NodeDesc lca = LCA()(x,y);
      if(lca == x) return y;
      if(lca == y) return x;
      return NoNode;
    }

    static bool is_edge(const NodeDesc u, const NodeDesc v)  { return test(children(u), v); }
    static bool adjacent(const NodeDesc u, const NodeDesc v) { return is_edge(u,v) || is_edge(v,u); }

    //! for sanity checks: test if there is a directed cycle in the data structure (more useful for networks, but definable for trees too)
    bool has_cycle() const {
      if(!empty()) {
        // to detect cycles, we just run a non-tracking DFS and do our own tracking; as soon as we re-see a node, we know there is a cycle
        NodeSet seen;
        NodeTraversal<preorder, Phylogeny, void, void> no_tracking_dfs(_roots);
        for(const NodeDesc x: no_tracking_dfs)
          if(!append(seen, x).second)
            return true;
        return false;
      } else return false;
    }

    bool are_siblings(const Node& y_node, const Node& z_node) const {
      if(&y_node != &z_node) {
        if(!y_node.is_root() && !z_node.is_root()) {
          return std::are_disjoint(y_node.parents(), z_node.parents());
        } else return false;
       } else return true;
    }
    bool are_siblings(const Node& y_node, const NodeDesc z) const { return are_siblings(y_node, node_of(z)); }
    bool are_siblings(const NodeDesc y, const Node& z_node) const { return are_siblings(node_of(y), z_node); }
    bool are_siblings(const NodeDesc y, const NodeDesc z) const { return are_siblings(node_of(y), node_of(z)); }

    NodeDesc common_parent(const Node& y_node, const Node& z_node) const {
      if(&y_node != &z_node) {
      	if(!y_node.is_root() && !z_node.is_root()) {
          const auto iter = std::common_element(y_node.parents(), z_node.parents());
					return iter ? *iter : NoNode;
        } else NoNode;
      } else return y_node.any_parent();
    }
    NodeDesc common_parent(const Node& y_node, const NodeDesc z) const { return common_parent(y_node, node_of(z)); }
    NodeDesc common_parent(const NodeDesc y, const Node& z_node) const { return common_parent(node_of(y), z_node); }
    NodeDesc common_parent(const NodeDesc y, const NodeDesc z) const { return common_parent(node_of(y), node_of(z)); }

    // ================== construction helpers =====================
    // NOTE: these are powerful and can potentially leave your phylogeny in an inconsistent state
    //       (in particular if you use an EdgeEmplacer with 'track_roots = false' and forget to mark the roots afterwards)
    //       however, this power enables certain use cases where we want to "directly" access the edges of a network...
    //       just, promise to be careful with your EdgeEmplacers
  public:
    template<bool, PhylogenyType> friend struct EdgeEmplacementHelper;
    template<bool, PhylogenyType, NodeTranslationType, class> friend struct EdgeEmplacer;

    // emplace a set of new edges into *this
    // NOTE: to extract node/edge data, use the data-extraction functions - they will receive a reference to a node/edge of the source phylogeny
    // NOTE: node-/edge- data is passed into the extractor functions by move if the edge-container is passed by rvalue_reference; otherwise by const ref
    // NOTE: you can pass your own (partial) node translation old_to_new in order to pre-define node-correspondances (this is quite powerfull!)
    //       if you do this, make sure that all values in the translation are valid node descriptors (point to constructed nodes of the correct type)
    // NOTE: if you build_from_edges<false> you HAVE TO REMEMBER to update the roots! This will only be done automatically for build_from_edges<true>
    template<bool track_roots,
             std::IterableType Edges,
             NodeTranslationType OldToNewTranslation = NodeTranslation,
             NodeFunctionType NodeDataExtract = IgnoreNodeDataFunc, // takes a Node (possible rvalue-ref) and returns (steals) its data
             class EdgeDataExtract = IgnoreEdgeDataFunc> // takes an Edge (possible rvalue-ref) and returns (steals) its data
    void build_from_edges(Edges&& edges,
                         OldToNewTranslation&& old_to_new = OldToNewTranslation(),
                         NodeDataExtract&& nde = NodeDataExtract(),
                         EdgeDataExtract&& ede = EdgeDataExtract())
    {
      using MyEdge = std::conditional_t<std::is_rvalue_reference_v<Edges&&>, Edge&&, const Edge&>;
      auto emp = EdgeEmplacers<track_roots>::make_emplacer(*this, old_to_new, nde);
      // step 1: copy other_x
      // step 2: copy everything below other_x
      if constexpr (track_roots) {
        NodeSet root_candidates;
        for(auto other_uv: std::forward<Edges>(edges))
          emp.emplace_edge(other_uv.as_pair(), root_candidates, ede(static_cast<MyEdge>(other_uv)));
        emp.commit_roots();
      } else
        for(auto other_uv: std::forward<Edges>(edges))
          emp.emplace_edge(other_uv.as_pair(), ede(static_cast<MyEdge>(other_uv)));
    }

    // move the subtree below other_x into us by changing the parents of other_x to {x}
    // NOTE: the edge x --> other_x will be initialized using "args"
    // NOTE: _Phylo needs to have the same NodeType as we do!
    template<PhylogenyType _Phylo, class... Args>
        requires(std::is_same_v<Node, typename std::remove_reference_t<_Phylo>::Node> && std::is_rvalue_reference_v<_Phylo&&>)
    NodeDesc place_below_by_move(_Phylo&& other, const NodeDesc other_x, const NodeDesc x, Args&&... args) {
      assert(other_x != NoNode);
      std::cout << "moving subtree below "<<other_x<<"...\n";
      // step 1: move other_x over to us
      auto& other_x_node = other[other_x];
      auto& other_x_parents = other_x_node.parents();
      
      if(!other_x_parents.empty()){
        while(!other_x_parents.empty())
          other.delete_edge(front(other_x_parents), other_x_node);
      } else erase(other._roots, other_x); // if other_x has no parents, remove it from other.roots()

      if(x != NoNode) {
        const bool success = add_child(x, other_x, std::forward<Args>(args)...).second;
        assert(success);
      } else append(_roots, other_x);
      // step 2: update node and edge numbers
      size_t node_count = 0;
      size_t edge_count = 0;
      for(const NodeDesc u: nodes_below_postorder(other_x)) {
        node_count++;
        edge_count += out_degree(u);
      }
      count_node(node_count);
      count_edge(edge_count);
      other.count_node(-node_count);
      other.count_edge(-edge_count);
      return other_x;
    }

    // =============================== construction ======================================
    Phylogeny() = default;

    // initialize tree from any std::IterableType, for example, std::vector<PT::Edge<>>
    template<std::IterableType Edges,
             NodeTranslationType OldToNewTranslation = NodeTranslation,
             NodeFunctionType NodeDataExtract = IgnoreNodeDataFunc, // takes a Node (possible rvalue-ref) and returns (steals) its data
             class EdgeDataExtract = IgnoreEdgeDataFunc> // takes an Edge (possible rvalue-ref) and returns (steals) its data
    Phylogeny(Edges&& edges,
             NodeDataExtract&& nde = NodeDataExtract(),
             EdgeDataExtract&& ede = EdgeDataExtract(),
             OldToNewTranslation&& old_to_new = OldToNewTranslation())
    {
      DEBUG3(std::cout << "init Tree with edges "<<edges<<"\n");
      build_from_edges<true>(std::forward<Edges>(edges), old_to_new,
                             std::forward<NodeDataExtract>(nde),
                             std::forward<EdgeDataExtract>(ede));
      DEBUG2(tree_summary(std::cout));
    }

    // NOTE: construction from another (sub-)phylogeny largely depends on the data policy:
    // 1. "copy" (default)      - creates new nodes, copying/moving the data from the other nodes
    //                            (move data if the other phylogeny is presented as rvalue_reference)
    // 2. "move"                - simply moves the root from the given (sub-)phylogeny into *this as new root
    // NOTE: 2. requires that the given Phylogeny has the same type as us


    // "copy" construction with root(s)
    template<PhylogenyType _Phylo,
             NodeIterableType RContainer,
             NodeTranslationType OldToNewTranslation = NodeTranslation,
             NodeFunctionType NodeDataExtract = CopyOrMoveNodeDataFunc<_Phylo>,
             class EdgeDataExtract = CopyOrMoveEdgeDataFunc<_Phylo>>
    Phylogeny(_Phylo&& N,
              const RContainer& in_roots,
              NodeDataExtract&& nde = NodeDataExtract(),
              EdgeDataExtract&& ede = EdgeDataExtract(),
              OldToNewTranslation&& old_to_new = OldToNewTranslation(),
              const policy_copy_t = policy_copy_t())
    {
      if(!in_roots.empty()) {
        if(in_roots.size() == 1) {
          build_from_edges<false>(N.edges_below_preorder(front(in_roots)), old_to_new,
                                  std::forward<NodeDataExtract>(nde),
                                  std::forward<EdgeDataExtract>(ede));
        } else {
          build_from_edges<false>(N.edges_below_preorder(in_roots), old_to_new,
                                  std::forward<NodeDataExtract>(nde),
                                  std::forward<EdgeDataExtract>(ede));
        }
        // mark the roots
        for(const NodeDesc& r: in_roots) append(_roots, old_to_new.at(r));
      }
    }
    
    // "copy" construction with single root
    template<PhylogenyType _Phylo,
             NodeTranslationType OldToNewTranslation = NodeTranslation,
             NodeFunctionType NodeDataExtract = CopyOrMoveNodeDataFunc<_Phylo>,
             class EdgeDataExtract = CopyOrMoveEdgeDataFunc<_Phylo>>
    Phylogeny(_Phylo&& N,
              const NodeDesc& in_root,
              NodeDataExtract&& nde = NodeDataExtract(),
              EdgeDataExtract&& ede = EdgeDataExtract(),
              OldToNewTranslation&& old_to_new = OldToNewTranslation(),
              const policy_copy_t = policy_copy_t())
    {
      build_from_edges<false>(N.edges_below_preorder(in_root), old_to_new,
            std::forward<NodeDataExtract>(nde),
            std::forward<EdgeDataExtract>(ede));
      // mark the root
      append(_roots, old_to_new.at(in_root));
    }

    // "copy" construction without root (using all roots of in_tree)
    template<PhylogenyType _Phylo,
             NodeTranslationType OldToNewTranslation = NodeTranslation,
             NodeFunctionType NodeDataExtract = CopyOrMoveNodeDataFunc<_Phylo>,
             class EdgeDataExtract = CopyOrMoveEdgeDataFunc<_Phylo>>
    Phylogeny(_Phylo&& N,
              NodeDataExtract&& nde = NodeDataExtract(),
              EdgeDataExtract&& ede = EdgeDataExtract(),
              OldToNewTranslation&& old_to_new = OldToNewTranslation(),
              const policy_copy_t = policy_copy_t()):
      Phylogeny(std::forward<_Phylo>(N),
                N.roots(),
                std::forward<NodeDataExtract>(nde),
                std::forward<EdgeDataExtract>(ede),
                old_to_new,
                policy_copy_t())
    {}


    // "move" construction with root(s) by unlinking the in_roots from their in_tree and using them as our new _roots
    // NOTE: This will go horribly wrong if the sub-network below in_root has incoming arcs from outside the subnetwork!
    //       It is the user's responsibility to make sure this is not the case.
#warning TODO: check if this is still correct
    template<StrictPhylogenyType _Phylo, NodeIterableType RContainer>
    Phylogeny(_Phylo&& in_tree, const RContainer& in_roots, const policy_move_t = policy_move_t()) {
      for(const NodeDesc& r: in_roots)
        place_below_by_move(std::move(in_tree), r, NoNode);
    }
    // "move" construction with single root
    template<StrictPhylogenyType _Phylo, NodeIterableType RContainer>
    Phylogeny(_Phylo&& in_tree, const NodeDesc in_root, const policy_move_t = policy_move_t()):
      Phylogeny(std::move(in_tree), std::array<NodeDesc,1>{in_root}, policy_move_t()) {}
    // "move" construction without root
    Phylogeny(Phylogeny&& in_tree, const policy_move_t):
      Phylogeny(std::move(in_tree), in_tree.roots(), policy_move_t()) {}


    // if we just want to copy a phylogeny, use the "copy" version
    Phylogeny(const Phylogeny& in_tree):
      Phylogeny(in_tree, in_tree.roots())
    {}
    // if we just want to move a phylogeny, we can delegate to the move-construction of the parent since we do not have members
    Phylogeny(Phylogeny&& in_tree): Parent(static_cast<Parent&&>(in_tree)) {}


    Phylogeny& operator=(const Phylogeny& other) = default;
    Phylogeny& operator=(Phylogeny&& other) = default;

    // =================== i/o ======================

    std::ostream& tree_summary(std::ostream& os) const
    {
      DEBUG3(os << "tree has "<< num_edges() <<" edges and "<< _num_nodes <<" nodes, leaves: "<<leaves()<<"\n");
      DEBUG3(os << "nodes: "<<nodes()<<'\n');
      DEBUG3(os << "edges: "<<edges()<<'\n');
      for(const NodeDesc& u: nodes())
        os << u << ":" << "\tIN: "<< in_edges(u) << "\tOUT: "<< out_edges(u) << '\n';
      return os << "\n";
    }


    void print_subtree(std::ostream& os, const NodeDesc& u, std::string prefix, std::unordered_bitset& seen) const {
      const bool u_reti = is_reti(u);
      std::string u_name = std::to_string(name(u));
      if constexpr (Node::has_label){
        const std::string u_label = std::to_string(label(u));
        if(!u_label.empty())
          u_name += "[" + u_label + "]";
      }
      if(u_name == "") {
        if(u_reti)
          u_name = std::string("(" + std::to_string(u) + ")R");
        else if(!is_leaf(u)) u_name = std::string("+");
      }
      os << '-' << u_name;
      if(u_reti) os << 'R';
      
      bool u_seen = true;
      if(!u_reti || !(u_seen = test(seen, name(u)))) {
        const auto& u_childs = children(u);
        if(u_reti) append(seen, name(u));
        switch(u_childs.size()){
          case 0:
            os << std::endl;
            break;
          case 1:
            prefix += std::string(u_name.length() + 1, ' ');
            print_subtree(os, std::front(children(u)), prefix, seen);
            break;
          default:
            prefix += std::string(u_name.length(), ' ') + '|';

            uint32_t count = u_childs.size();
            for(const auto c: u_childs){
              print_subtree(os, c, prefix, seen);
              if(--count > 0) os << prefix;
              if(count == 1) prefix.back() = ' ';
            }
        }
      } else os << std::endl;
    }

    void print_subtree(std::ostream& os) const { print_subtree(os, front(_roots)); }
    void print_subtree(std::ostream& os, const NodeDesc& u) const {
      std::unordered_bitset seen(_num_nodes);
      print_subtree(os, u, "", seen);
    }


    // other phylogenies are our friends
    template<StorageEnum,
             StorageEnum,
             class,
             class,
             class,
             StorageEnum,
             template<StorageEnum, StorageEnum, class, class, class> class>
    friend class Phylogeny;

  };

  template<PhylogenyType _Phylo>
  std::ostream& operator<<(std::ostream& os, const _Phylo& T) {
    if(!T.empty()) {
      T.print_subtree(os);
      return os;
    } else return os << "{}";
  }

}
