
#pragma once

#include <ranges>
#include "pair_iter.hpp"
#include "set_interface.hpp"

#include "utils.hpp"
#include "types.hpp"
#include "lca.hpp"
#include "dfs.hpp"
#include "except.hpp"
#include "node.hpp"
#include "edge.hpp"
#include "induced_tree.hpp"

namespace PT {


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
    using RootContainer = StorageType<_RootStorage, NodeDesc>;
  protected:
    RootContainer _roots;
		size_t _num_nodes = 0;
		size_t _num_edges = 0;

		void count_node(const int nr = 1) { _num_nodes += nr; std::cout << "counted "<<_num_nodes<<" nodes\n"; }
		void count_edge(const int nr = 1) { _num_edges += nr; std::cout << "counted "<<_num_edges<<" edges\n"; }
	public:
    static constexpr bool is_declared_tree = false;
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
    using RootContainer = StorageType<_RootStorage, NodeDesc>;
  protected:
    RootContainer _roots;
		size_t _num_nodes = 0;

		void count_node(const int nr = 1) { _num_nodes += nr; }
		void count_edge(const int nr = 1) const {}
	public:
    static constexpr bool is_declared_tree = true;
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
		using Parent::count_node;
		using Parent::count_edge;
  public:
    using typename Parent::Node;
    using LabelType = typename Node::LabelType;
    using typename Parent::Adjacency;
    using typename Parent::Edge;
    using typename Parent::NodeData;
    using typename Parent::EdgeData;
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
    using Parent::name;
    using Parent::label;
		using Parent::num_nodes;
		using Parent::num_edges;
    using Parent::root;
    using Parent::_roots;
    
    using IgnoreNodeDataFunc = std::IgnoreFunction<NodeDataOr<Phylogeny>>;
    using IgnoreEdgeDataFunc = std::IgnoreFunction<EdgeDataOr<Phylogeny>>;
  protected:
    size_t _num_nodes = 0u;

    void delete_node(const NodeDesc x) {
			count_node(-1);
      PT::delete_node<Node>(x);
    }

	  // NOTE: Edge data can be passed either in the Adjacency v or with additional args (the latter takes preference)
    template<AdjacencyType Adj, class... Args>
    std::pair<Adjacency*, bool> add_edge(const NodeDesc u, Adj&& v, Args&&... args) {
      const auto [iter, success] = node_of(u).add_child(std::forward<Adj>(v), std::forward<Args>(args)...);
      if(success) {
#ifndef NDEBUG
        const bool res =
#endif
        node_of(v).add_parent(u, *iter).second;
        assert(res);
				count_edge();
        std::cout << "node count: " << num_nodes() << " edge count: " << num_edges() << '\n';
        return {&(*iter), true};
      } else return {nullptr, false};
    }

    void delete_edge(Node& u_node, Node& v_node) {
      // step 1: remove u-->v & free the edge data
      auto& u_children = u_node.children();
      const auto uv_iter = u_children.find(&v_node);
      assert(uv_iter != u_children.end());
      uv_iter->free_edge_data();
      u_children.erase(uv_iter);

      // step 2: remove v-->u
      auto& v_parents = v_node.parents();
      const auto vu_iter = v_parents.find(&u_node);
      assert(vu_iter != v_parents.end());
      v_parents.erase(vu_iter);

      // step 3: count the missing edge
      count_edge(-1);
    }
    void delete_edge(Node& u_node, const NodeDesc v) { delete_edge(u_node, node_of(v)); }
    void delete_edge(const NodeDesc u, Node& v_node) { delete_edge(node_of(u), v_node); }
    void delete_edge(const NodeDesc u, const NodeDesc v) { delete_edge(node_of(u), node_of(v)); }

  public:

    // ================ modification ======================
    template<class... Args> static constexpr NodeDesc create_node(Args&&... args) {
      return reinterpret_cast<uintptr_t>(new Node(std::forward<Args>(args)...));
    }

    // new nodes can only be added as children or parents of existing nodes
    // NOTE: the edge data can either be passed using the Adjacency v or constructed from the parameter args
    // NOTE: if the tree is empty, it is acceptable to call add_parent(u, u); to emplace a root
    template<AdjacencyType Adj, class... Args>
    std::pair<Adjacency*, bool> add_child(const NodeDesc u, Adj&& v, Args&&... args) {
      assert(u != v);
      std::cout << "adding edge from " << u << " to " << v <<"\n";
      const auto result = add_edge(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      count_node(result.second);
      return result;
    }
    template<AdjacencyType Adj, class... Args>
    bool add_parent(Adj&& v, const NodeDesc u, Args&&... args) {
      const auto result = add_edge(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      if(result.second) {
				count_node();
        _roots.erase(v);
        append(_roots, u);
      }
      return result;
    }

    template<class... Args>
    NodeDesc add_root(Args&&... args) {
      std::cout << "creating root\n";
      const NodeDesc new_root = create_node(std::forward<Args>(args)...);
      std::cout << "adding "<<new_root<<" to roots\n";
      if(append(_roots, new_root).second) {
        count_node();
        std::cout << "mark1\n";
        return new_root;
      } else {
        delete_node(new_root);
        return NoNode;
      }
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
          const auto iter = _roots.find(u);
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
      delete_node(v_node);
    }  
    void remove_leaf(const NodeDesc v) { remove_leaf(node_of(v)); }

    // remove the subtree rooted at u
    // NOTE: LastRites can be used to say goodbye to your node(s) (remove it from other containers or whatever)
    template<class LastRites = std::IgnoreFunction<void>>
    void remove_subtree(const NodeDesc u, const LastRites goodbye = LastRites()){
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(back(C), goodbye);
      goodbye(u);
      remove_leaf(u);
    }
    
    template<class LastRites = std::IgnoreFunction<void>>
    void clear(const LastRites goodbye = LastRites()) {
      for(const NodeDesc r: _roots) remove_subtree(r, goodbye);
      _roots.clear();
    }

    template<class LastRites = std::IgnoreFunction<void>>
    void remove_subtree_except(const NodeDesc u, const NodeDesc except, const LastRites goodbye = LastRites()) {
      if(u != except) {
        const auto& C = children(u);
        auto C_iter = std::rbegin(C); // we will likely see vectors as child-containers and those are better treated from their back
        while(C_iter != std::rend(C)){
          const auto next_iter = std::next(C_iter);
          remove_subtree_except(*C_iter, except, goodbye);
          C_iter = std::move(next_iter);
        }
        if(C.empty()) {
          goodbye(u);
          remove_leaf(u);
        }
      } else remove_subtree_except_root(u, goodbye);
    }

    // remove the subtree rooted at u, but leave u be
    template<class LastRites = std::IgnoreFunction<void>>
    void remove_subtree_except_root(const NodeDesc u, const LastRites goodbye = LastRites()) {
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(back(C), goodbye);
    }
    
    // =============== variable query ======================
    constexpr bool is_tree() const { return true; }
    bool empty() const { return num_nodes() == 0; }
    bool edgeless() const { return num_edges() == 0; }
    size_t num_edges() const { return empty() ? 0 : _num_nodes - 1; }
    size_t num_nodes() const { return _num_nodes; }

    template<TraversalType o = postorder>
    auto nodes(const order<o> _o = order<o>()) const { return dfs().traversal(_o);  }
    auto nodes_preorder() const { return nodes(order<preorder>()); }
    auto nodes_inorder() const { return nodes(order<inorder>()); }
    auto nodes_postorder() const { return nodes(order<postorder>()); }
    
    template<TraversalType o = postorder>
    auto edges(const order<o> _o = order<o>()) const { return all_edges_dfs().traversal(_o);  }
    auto edges_preorder() const { return edges(order<preorder>()); }
    auto edges_inorder() const { return edges(order<inorder>()); }
    auto edges_postorder() const { return edges(order<postorder>()); }

    /* TODO: find out why this doesn't work!
    auto leaves() {
      auto X = nodes() | std::views::filter([](const auto& n){ return n.is_leaf();});
      static_assert(std::is_integral_v<decltype(X)>);
      return X;
    }
    auto leaves() const { return nodes() | std::views::filter([](const auto& n){ return n.is_leaf();});  }
    */
    template<TraversalType o = postorder>
    auto leaves(const order<o> _o = order<o>()) {
      auto nodes_sp = std::make_shared<decltype(nodes())>(nodes(_o));
      return std::FilteredIterFactory<decltype(nodes())>(nodes_sp, nodes_sp->end(), [](const NodeDesc& n){return node_of(n).is_leaf();});
    }
    auto leaves_preorder() { return leaves(order<preorder>()); }
    auto leaves_inorder() { return leaves(order<inorder>()); }
    auto leaves_postorder() { return leaves(order<postorder>()); }


    // =============== traversals ======================
    // the following functions return a "meta"-traversal object, which can be used as follows:
    // iterate over "dfs().preorder()" to get all nodes in preorder
    // iterate over "edge_dfs().postorder(u)" to get all edges below u in postorder
    // say "NodeVec(dfs_except(forbidden).inorder(u))" to get a vector of nodes below u but strictly above the nodes of "forbidden" in inorder
    //NOTE: refrain from taking "const auto x = dfs();" since const MetaTraversals are useless...
    //NOTE: if you say "const auto x = dfs().preorder();" you won't be able to track seen nodes (which might be OK for trees...)
  protected: // per default, disable tracking of seen nodes iff we're a tree
    using TreeDefaultSeen = void;
    using NetworkDefaultSeen = NodeSet;
  public:
    using DefaultSeen = std::conditional_t<is_declared_tree, TreeDefaultSeen, NetworkDefaultSeen>;

    MetaTraversal<      Phylogeny, DefaultSeen, NodeTraversal> dfs() { return *this; }
    MetaTraversal<const Phylogeny, DefaultSeen, NodeTraversal> dfs() const { return *this; }
    MetaTraversal<      Phylogeny, DefaultSeen, EdgeTraversal> edge_dfs() { return *this; }
    MetaTraversal<const Phylogeny, DefaultSeen, EdgeTraversal> edge_dfs() const { return *this; }
    MetaTraversal<      Phylogeny, DefaultSeen, AllEdgesTraversal> all_edges_dfs() { return *this; }
    MetaTraversal<const Phylogeny, DefaultSeen, AllEdgesTraversal> all_edges_dfs() const { return *this; }

    template<std::OptionalSetType ExceptSet, std::OptionalSetType SeenSet = DefaultSeen>
    MetaTraversal<      Phylogeny, SeenSet, NodeTraversal> dfs_except(ExceptSet&& except) { return {*this, std::forward<ExceptSet>(except)}; }
    template<std::OptionalSetType ExceptSet, std::OptionalSetType SeenSet = DefaultSeen>
    MetaTraversal<const Phylogeny, SeenSet, NodeTraversal> dfs_except(ExceptSet&& except) const { return {*this, std::forward<ExceptSet>(except)}; }
    template<std::OptionalSetType ExceptSet, std::OptionalSetType SeenSet = DefaultSeen>
    MetaTraversal<      Phylogeny, SeenSet, EdgeTraversal> edge_dfs_except(ExceptSet&& except) { return {*this, std::forward<ExceptSet>(except)}; }
    template<std::OptionalSetType ExceptSet, std::OptionalSetType SeenSet = DefaultSeen>
    MetaTraversal<const Phylogeny, SeenSet, EdgeTraversal> edge_dfs_except(ExceptSet&& except) const { return {*this, std::forward<ExceptSet>(except)}; }
    template<std::OptionalSetType ExceptSet, std::OptionalSetType SeenSet = DefaultSeen>
    MetaTraversal<      Phylogeny, SeenSet, AllEdgesTraversal> all_edges_dfs_except(ExceptSet&& except) { return {*this, std::forward<ExceptSet>(except)}; }
    template<std::OptionalSetType ExceptSet, std::OptionalSetType SeenSet = DefaultSeen>
    MetaTraversal<const Phylogeny, SeenSet, AllEdgesTraversal> all_edges_dfs_except(ExceptSet&& except) const { return {*this, std::forward<ExceptSet>(except)}; }

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

    bool is_edge(const NodeDesc u, const NodeDesc v) const  { return test(children(u), v); }
    bool adjacent(const NodeDesc u, const NodeDesc v) const { return is_edge(u,v) || is_edge(v,u); }

    //! for sanity checks: test if there is a directed cycle in the data structure (more useful for networks, but definable for trees too)
    bool has_cycle() const {
      if(!empty()) {
        // to detect cycles, we just run a non-tracking DFS and do our own tracking; as soon as we re-see a node, we know there is a cycle
        NodeSet seen;
        MetaTraversal<Phylogeny, void, NodeTraversal> no_tracking_dfs(*this);
        for(const NodeDesc x: no_tracking_dfs.preorder())
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

  
    // =================== rooted subtrees ====================

    // copy the subtree rooted at u into t
    template<PhylogenyType _Phylo = Phylogeny>
    _Phylo get_rooted_subtree(const NodeDesc u) const
    { return get_edges_below<typename _Phylo::EdgeVec>(u); }
    // copy the subtree rooted at u into t, but ignore subtrees rooted at nodes in 'except'
    template<PhylogenyType _Phylo = Phylogeny, NodeIterableType ExceptContainer = NodeSet>
    _Phylo get_rooted_subtree_except(const NodeDesc u, const ExceptContainer& except) const
    { return get_edges_below_except<typename _Phylo::EdgeVec>(u, except); }

    template<std::IterableType _Edgelist>
    _Edgelist& get_edges_below(const NodeDesc u, _Edgelist& el) const
    { return all_edges_dfs().postorder(u).append_to(el); }

    template<NodeIterableType ExceptContainer = NodeSet, std::IterableType _Edgelist = EdgeVec>
    _Edgelist& get_edges_below_except(const NodeDesc u, _Edgelist& el, const ExceptContainer& except) const
    { return all_edges_dfs_except(except).postorder(u).append_to(el); }

    // ================== construction =====================
  protected:
    static constexpr NodeDesc& lookup_or_create(NodeTranslation& translate, const NodeDesc x) {
      const auto [iter, success] = translate.emplace(x);
      NodeDesc& result = iter->second;
      if(success) result = new Node();
      return result;
    }
    // construct a rooted tree structure from the given edges and return the node description of the root as well as the number of nodes
    // NOTE: index_to_desc will contain a translation of old descriptions to new descriptions
    template<class Deleter = std::default_delete<NodeTranslation>, std::IterableType Edges>
    static constexpr std::pair<NodeDesc, size_t> _nodes_from_edgelist(Edges&& el, std::unique_ptr<NodeTranslation, Deleter> old_to_new, const NodeDesc old_root = NoNode) {
      for(auto&& uv: el) {
        // step 1: create the copy of u
        const NodeDesc new_u = lookup_or_create(*old_to_new, uv.tail());
        // step 2: create the copy of v
        const NodeDesc new_v = lookup_or_create(*old_to_new, uv.head());
        // step 3: create the edge uv with its EdgeData
        const auto adj_iter = append(children(new_u), new_v, uv.head()).first;
        append(parents(new_v), new_u, *adj_iter);
      }
      if(old_root == NoNode) {
        NodeDesc _root = NoNode;
        // finally, find the root
        for(const NodeDesc x: std::seconds(*old_to_new))
          if(in_degree(x) == 0) {
            _root = x;
            break;
          }
        assert(_root != NoNode);
        return {_root, old_to_new->size()};
      } else return {old_to_new->at(old_root), old_to_new->size()};
    }
    template<std::IterableType Edges>
    static constexpr std::pair<NodeDesc, size_t> nodes_from_edgelist(Edges&& el, NodeTranslation* old_to_new = nullptr, const NodeDesc old_root = NoNode) {
      return old_to_new ? 
          _nodes_from_edgelist<std::NoDeleter>(std::forward<Edges>(el), old_to_new, old_root) :
          _nodes_from_edgelist(std::forward<Edges>(el), std::make_unique<NodeTranslation>(), old_root);
    }

    // copy/move a node other_x and place it below x; if x == NoNode, the copy will be a new root
    // return the description of the copy of other_x
    template<NodeType OtherNode,
             class NodeDataTranslation = IgnoreNodeDataFunc,
             class... Args>
    NodeDesc place_node_below(OtherNode&& other_x,
                              const NodeDesc x,
                              NodeDataTranslation&& ndt = NodeDataTranslation(),
                              Args&&... args) {
      NodeDesc x_copy = NoNode;
      if(x == NoNode) {
        if constexpr (has_data<OtherNode>) {
          x_copy = add_root(ndt(std::forward<OtherNode>(other_x).data()));
        } else {
          x_copy = add_root();
        }
      } else {
        if constexpr (has_data<OtherNode>) {
          x_copy = create_node(ndt(std::forward<OtherNode>(other_x).data()));
        } else {
          x_copy = create_node();
        }
        const bool success = add_child(x, x_copy, std::forward<Args>(args)...).second;
        assert(success);
      }
      return x_copy;
    }

    // place a subtree (by copying/moving nodes and data) of another Phylogeny below node x or as a new root if x = NoNode
    //  the edge-data of the arc from x to the copy of other_x will be initialized with args
    template<PhylogenyType _Phylo,
             class NodeDataTranslation = IgnoreNodeDataFunc,
             class EdgeDataTranslation = IgnoreEdgeDataFunc,
             class... Args>
    NodeDesc place_below_by_copy(_Phylo&& other,
                                 const NodeDesc other_x,
                                 const NodeDesc x,
                                 NodeDataTranslation&& ndt = NodeDataTranslation(),
                                 EdgeDataTranslation&& edt = EdgeDataTranslation(),
                                 Args&&... args) {
      NodeTranslation old_to_new;
      // step 1: copy other_x
      const NodeDesc x_copy = place_node_below(other[other_x], x, ndt, std::forward<Args>(args)...);
      old_to_new.try_emplace(other_x, x_copy);
      // step 2: copy everything below other_x
      for(const NodeDesc other_u: other.dfs().preorder(other_x)) if(other_u != other_x) {
        for(const auto& other_p: other[other_u].parents()) {
          NodeDesc u_copy = NoNode;
          if constexpr (std::remove_reference_t<_Phylo>::has_edge_data) {
            u_copy = place_node_below(other[other_u], old_to_new[other_p], ndt, edt(other_p.data()));
          } else {
            u_copy = place_node_below(other[other_u], old_to_new[other_p], ndt);
          }
          const bool success = old_to_new.try_emplace(other_u, u_copy).second;
          assert(success);
        }
      }
      return x_copy;
    }

    // move the subtree below other_x into us by changing the parents of other_x to {x}
    // NOTE: the edge x --> other_x will be initialized using "args"
    // NOTE: _Phylo needs to have the same NodeType as we do!
    template<PhylogenyType _Phylo, class... Args> requires(std::is_same_v<Node, typename std::remove_reference_t<_Phylo>::Node>)
    NodeDesc place_below_by_move(std::make_rvalue_reference<_Phylo> other, const NodeDesc other_x, const NodeDesc x, Args&&... args) {
      assert(other_x != NoNode);
      // step 1: move other_x over to us
      auto& other_x_node = other[other_x];
      auto& other_x_parents = other_x_node.parents();
      
      if(!parents.empty()){
        while(!other_x_parents.empty())
          other.delete_edge(front(other_x_parents), other_x_node);
      } else erase(other.roots(), other_x); // if other_x has no parents, remove it from other.roots()

      if(x != NoNode) {
        const bool success = add_child(x, other_x, std::forward<Args>(args)...).second;
        assert(success);
      } else append(_roots, other_x);
      // step 2: update node and edge numbers
      size_t num_nodes = 0;
      size_t num_edges = 0;
      for(const NodeDesc u: dfs().postorder(other_x)) {
        num_nodes += 1;
        num_edges += out_degree(u);
      }
      count_node(num_nodes);
      count_edge(num_edges);
      other.count_node(-num_nodes);
      other.count_edge(-num_edges);
      return other_x;
    }

  public:
    // =============================== construction ======================================
    Phylogeny() = default;

    // initialize tree from any std::IterableType, for example, std::vector<PT::Edge<>>
    template<std::IterableType Edges>
    Phylogeny(Edges&& edges, NodeTranslation* old_to_new = nullptr) {
      const auto [new_root, nnodes] = nodes_from_edgelist(edges, old_to_new);
      append(_roots, new_root);
      _num_nodes = nnodes;
      DEBUG3(std::cout << "init Tree with edges "<<edges<<"\n");
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
             class NodeDataTranslation = IgnoreNodeDataFunc,
             class EdgeDataTranslation = IgnoreEdgeDataFunc>
    Phylogeny(_Phylo&& in_tree,
              const RContainer& in_roots,
              NodeDataTranslation&& ndt = NodeDataTranslation(),
              EdgeDataTranslation&& edt = EdgeDataTranslation(),
              const policy_copy_t = policy_copy_t()) {
      for(const NodeDesc r: in_roots)
        place_below_by_copy(std::forward<_Phylo>(in_tree), r, NoNode, std::forward<NodeDataTranslation>(ndt), std::forward<EdgeDataTranslation>(edt));
    }
    
    // "copy" construction with single root
    template<PhylogenyType _Phylo,
             class NodeDataTranslation = IgnoreNodeDataFunc,
             class EdgeDataTranslation = IgnoreEdgeDataFunc>
    Phylogeny(_Phylo&& in_tree,
              const NodeDesc& in_root,
              NodeDataTranslation&& ndt = NodeDataTranslation(),
              EdgeDataTranslation&& edt = EdgeDataTranslation(),
              const policy_copy_t = policy_copy_t()):
      Phylogeny(std::forward<_Phylo>(in_tree),
                std::array<NodeDesc,1>{in_root},
                std::forward<NodeDataTranslation>(ndt),
                std::forward<EdgeDataTranslation>(edt),
                policy_copy_t())
    {}
    // "copy" construction without root (using all roots of in_tree)
    template<PhylogenyType _Phylo,
             class NodeDataTranslation = IgnoreNodeDataFunc,
             class EdgeDataTranslation = IgnoreEdgeDataFunc>
    Phylogeny(_Phylo&& in_tree,
              NodeDataTranslation&& ndt = NodeDataTranslation(),
              EdgeDataTranslation&& edt = EdgeDataTranslation(),
              const policy_copy_t = policy_copy_t()):
      Phylogeny(std::forward<_Phylo>(in_tree),
                in_tree.roots(),
                std::forward<NodeDataTranslation>(ndt),
                std::forward<EdgeDataTranslation>(edt),
                policy_copy_t())
    {}


    // "move" construction with root(s) by unlinking the in_roots from their in_tree and using them as our new _roots
    // NOTE: This will go horribly wrong if the sub-network below in_root has incoming arcs from outside the subnetwork!
    //       It is the user's responsibility to make sure this is not the case.
    template<PhylogenyType _Phylo, NodeIterableType RContainer>
    Phylogeny(_Phylo&& in_tree, const RContainer& in_roots, const policy_move_t = policy_move_t()) {
      for(const NodeDesc r: in_roots)
        place_below_by_move(std::forward<_Phylo>(in_tree), r, NoNode);
    }
    // "move" construction with single root
    template<PhylogenyType _Phylo, NodeIterableType RContainer>
    Phylogeny(_Phylo&& in_tree, const NodeDesc in_root, const policy_move_t = policy_move_t()):
      Phylogeny(std::forward<_Phylo>(in_tree), std::array<NodeDesc,1>{in_root}, policy_move_t()) {}
    // "move" construction without root
    Phylogeny(Phylogeny&& in_tree, const policy_move_t):
      Phylogeny(std::move(in_tree), in_tree.roots(), policy_move_t()) {}


    // if we just want to copy a phylogeny, use the "copy" version
    Phylogeny(const Phylogeny& in_tree): Phylogeny(in_tree, in_tree.roots(), policy_copy_t()) {}
    // if we just want to move a phylogeny, use the "move" version
    Phylogeny(Phylogeny&& in_tree): Phylogeny(std::move(in_tree), in_tree.roots(), policy_move_t()) {}

#warning TODO: make sure get_induced_edges handles rvalue_references for supertrees
    // initialize a tree as the smallest subtree spanning a list L of nodes in a given supertree
    //NOTE: we'll need to know for each node u its distance to the root, and we'd also like L to be in some order (any of pre-/in-/post- will do)
    //      so if the caller doesn't provide them, we'll compute them from the given supertree
    //NOTE: if the infos are provided, then this runs in O(|L| * LCA-query in supertree), otherwise, an O(|supertree|) DFS is prepended
    //NOTE: when passed as a const container, the nodes are assumed to be in order; ATTENTION: this is not verified!
    //NOTE: we force the given _Phylogeny to be declared a tree (tree_tag)
    template<PhylogenyType _Phylo, NodeIterableType LeafList, NodeMapType NodeInfoMap = InducedSubtreeInfoMap>
    Phylogeny(_Phylo&& supertree, LeafList&& _leaves, std::shared_ptr<NodeInfoMap> _node_infos = std::make_shared<NodeInfoMap>()):
      Phylogeny(get_induced_edges(std::forward<_Phylo>(supertree), std::forward<LeafList>(_leaves), std::move(_node_infos)))
    {}
    // the caller can be explicit about the data transfer policy (policy_move, policy_copy, policy_inplace)
    template<PhylogenyType _Phylo, DataPolicyTag Tag, NodeIterableType LeafList, NodeMapType NodeInfoMap = InducedSubtreeInfoMap>
    Phylogeny(const Tag,
          _Phylo&& supertree,
          LeafList&& _leaves,
          std::shared_ptr<NodeInfoMap> _node_infos = std::make_shared<NodeInfoMap>()):
      Parent(get_induced_edges(Tag(), std::forward<_Phylo>(supertree), std::forward<LeafList>(_leaves), std::move(_node_infos)))
    {}

    // =================== i/o ======================

    std::ostream& tree_summary(std::ostream& os) const
    {
      DEBUG3(os << "tree has "<<num_edges()<<" edges and "<<num_nodes()<<" nodes, leaves: "<<leaves()<<"\n");
      DEBUG3(os << "nodes: "<<nodes()<<'\n');
      DEBUG3(os << "edges: "<<Parent::edges()<<'\n');
      
      for(const NodeDesc u: nodes()){
        os << u;
        os << ":" << "\tIN: "<< in_edges(u) << "\tOUT: "<< out_edges(u) << std::endl;
      }
      return os << "\n";
    }
 
    void print_subtree(std::ostream& os, std::string prefix = "") const { print_subtree(os, front(_roots), std::move(prefix)); }
    void print_subtree(std::ostream& os, const NodeDesc u, std::string prefix = "") const {
      std::string u_name = std::to_string(name(u));
      if constexpr (Node::has_label)
        u_name += "[" + std::to_string(label(u)) + "]";
      if(u_name == "") u_name = "+";
      os << '-' << u_name;

      const auto& u_childs = children(u);
      switch(u_childs.size()) {
        case 0:
          os << std::endl;
          break;
        case 1:
          print_subtree(os, std::front(u_childs), prefix + std::string(u_name.length() + 1u, ' '));
          break;
        default:
          prefix += std::string(u_name.length(), ' ') + '|';

          uint32_t count = u_childs.size();
          for(const auto c: u_childs){
            print_subtree(os, c, prefix);
            if(--count > 0) os << prefix;
            if(count == 1) prefix.back() = ' ';
          }
      }
    }    
  };

  template<PhylogenyType _Phylo>
  std::ostream& operator<<(std::ostream& os, const _Phylo& T) {
    if(!T.empty()) {
      T.print_subtree(os);
      return os;
    } else return os << "{}";
  }

}
