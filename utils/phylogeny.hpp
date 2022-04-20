
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
#include "extract_data.hpp"

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
    static constexpr bool has_unique_root = (RootStorage == singleS);
    using RootContainer = StorageClass<_RootStorage, NodeDesc>;
    using DefaultSeen = NodeSet;
  protected:
    RootContainer _roots;
		size_t _num_nodes = 0u;
		size_t _num_edges = 0u;

	public:
    static constexpr bool is_declared_tree = false;

    ProtoPhylogeny() = default;
    ProtoPhylogeny(const ProtoPhylogeny&) = delete;
    ProtoPhylogeny& operator=(const ProtoPhylogeny&) = delete;

    ProtoPhylogeny(ProtoPhylogeny&& other):
      _roots(other._roots), _num_nodes(other._num_nodes), _num_edges(other._num_edges)
    {
      other._roots.clear();
    }
    ProtoPhylogeny& operator=(ProtoPhylogeny&& other) {
      _roots = std::move(other._roots);
      _num_nodes = std::move(other._num_nodes);
      _num_edges = std::move(other._num_edges);
      other._roots.clear();
      return *this;
    }

		void count_node(const int nr = 1) { _num_nodes += nr; } 
		void count_edge(const int nr = 1) { _num_edges += nr; }

    bool is_forest() const { return _num_nodes == _num_edges + _roots.size(); }
		size_t num_nodes() const { return _num_nodes; }
		size_t num_edges() const { return _num_edges; }
    NodeDesc root() const { return front(_roots); }
    const RootContainer& roots() const { return _roots; }

    bool has_path(const NodeDesc x, NodeDesc y) const {
      std::unordered_set<NodeDesc> seen;
      std::vector<NodeDesc> todo{y};
      while(1) {
        do {
          if(LIKELY(!todo.empty())) {
            y = std::value_pop(todo);
          } else return false;
        } while(test(seen, y));
        if(LIKELY(y != x)) {
          append(todo, this->parents(y));
          append(seen, y);
        } else return true;
      }
      return false;
    }

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

    ProtoPhylogeny() = default;
    ProtoPhylogeny(const ProtoPhylogeny&) = delete;
    ProtoPhylogeny& operator=(const ProtoPhylogeny&) = delete;

    ProtoPhylogeny(ProtoPhylogeny&& other):
      _roots(other._roots), _num_nodes(other._num_nodes)
    {
      other._roots.clear();
    }
    ProtoPhylogeny& operator=(ProtoPhylogeny&& other) {
      _roots = std::move(other._roots);
      _num_nodes = std::move(other._num_nodes);
      other._roots.clear();
    }

		void count_node(const int nr = 1) { _num_nodes += nr; }
		static constexpr void count_edge(const int nr = 1) {}

    static constexpr bool is_forest() { return true; }
		size_t num_nodes() const { return _num_nodes; }
		size_t num_edges() const { return (_num_nodes == 0) ? 0 : _num_nodes - 1; }
    NodeDesc root() const { return front(_roots); }
    const RootContainer& roots() const { return _roots; }

    //! return whether there is a directed path from x to y in the tree
    bool has_path(const NodeDesc x, NodeDesc y) const {
      while(1){
        if(y == x) return true;
        const auto& p = this->parents(y);
        if(UNLIKELY(p.empty())) return false;
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
    using Parent::is_root;
    using Parent::is_reti;
    using Parent::is_leaf;
    using Parent::is_edge;
		using Parent::count_node;
		using Parent::count_edge;
    using Parent::has_edge_data;

    // ================ modification ======================
    // create a node in the void
    // NOTE: this is static and, as such, does not change the network,
    //       indeed this only creates a node structure in memory which can then be used with add_root() or add_child() or add_parent())
    template<class... Args> static constexpr NodeDesc create_node(Args&&... args) {
      return new Node(std::forward<Args>(args)...);
    }
    // in order to pass the Node's description to the node-data creator, we first reserve space for the node, then construct the Node in place (placement new)
    template<NodeFunctionType NodeDataExtract = DefaultExtractData<Ex_node_data, Phylogeny>>
    static constexpr NodeDesc create_node(NodeDataExtract&& node_data_extract) {
      if constexpr (!std::is_same_v<std::remove_cvref_t<NodeDataExtract>, DefaultExtractData<Ex_node_data, Phylogeny>>) {
        Node* space = reinterpret_cast<Node*>(operator new(sizeof(Node)));
        const NodeDesc result = space;
        new(space) Node(node_data_extract(result));
        return result;
      } else return create_node();
    }

#warning "TODO: clean up when the phylogeny is destroyed! In particular delete nodes!"
    void delete_node(const NodeDesc x) {
			count_node(-1);
      PT::delete_node<Node>(x);
    }

    // add an edge between two nodes of the tree/network
    // NOTE: the nodes are assumed to have been added by add_root(), add_child(), or add_parent() (of another node) before!
	  // NOTE: Edge data can be passed either in the Adjacency v or with additional args (the latter takes preference)
    template<AdjacencyType Adj, class... Args>
    std::pair<Adjacency, bool> add_edge(const NodeDesc u, Adj&& v, Args&&... args) {
      const auto [iter, success] = Parent::add_child(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      std::pair<Adjacency, bool> result = {NoNode, success};
      if(success) {
        const bool res = Parent::add_parent(v, u, *iter).second;
        assert(res && "u is a predecessor of v, but v is not a successor of u. This should never happen!");
				count_edge();
      }
      return result;
    }

    bool remove_edge(const NodeDesc u, const NodeDesc v) {
      const bool result = Parent::remove_edge(u,v);
      count_edge(result);
      return result;
    }

    // remove the given node u, as well as all ancestors who, after all removals, have no children left
    // if suppress_deg2 is true, then also suppress deg2-nodes encountered on the way
    template<bool suppress_deg2 = true, class... AdapterArgs>
    void remove_upwards(const NodeDesc v, AdapterArgs&&... args) {
      auto& v_node = node_of(v);
      const size_t v_indeg = v_node.in_degree();
      const size_t v_outdeg = v_node.out_degree();
      switch(v_outdeg) {
        case 0: {
          auto& v_parents = v_node.parents();
          while(!v_parents.empty()) {
            const NodeDesc u = front(v_parents);
            remove_edge(u,v);
            remove_upwards(u, args...);
          }
          return;
        }
        case 1: {
          if constexpr (suppress_deg2) {
            if(v_indeg == 1) {
              const NodeDesc v_parent = v_node.any_parent();
              contract_up(v, args...);
              remove_upwards(v_parent, args...);
            }
          }
          return;
        }
      }
    }


    // new nodes can only be added as 1. new roots, 2. children or 3. parents of existing nodes
    // NOTE: the edge data can either be passed using the Adjacency v or constructed from the parameter args
    template<AdjacencyType Adj, class... Args>
    std::pair<Adjacency, bool> add_child(const NodeDesc u, Adj&& v, Args&&... args) {
      assert(u != static_cast<NodeDesc>(v));
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
    bool mark_root(const NodeDesc u) {
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



    // transfer a child w of source to target
    // NOTE: this will not create loops
    // NOTE: no check is performed whether child is already a child of target (apart from duplication checks done by the SuccStorage)
    //       if the SuccStorage cannot exclude duplicates, then this can lead to double edges!
    //       if the SuccStorage does exclude duplicates, then the existing edge from target to w survives while the other is deleted
    // NOTE: the AdjAdapter can be used to merge the EdgeData passed with target and the source->w EdgeData into the target->... EdgeData
    //       for example like so: [](const auto& uv, auto& vw) { vw.data() += uv.data(); }
    //       By default the target->... EdgeData is ignored.
    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter>
    bool transfer_child(const NodeDesc source,
                        const Adj& target,
                        const std::iterator_of_t<typename Node::SuccContainer>& w_iter,
                        AdjAdapter&& adapt = AdjAdapter())
    {
      assert(w_iter != children(source).end());
      const NodeDesc w = *w_iter;
      assert(w != target);

      auto& w_parents = parents(w);
      children(source).erase(w_iter);

      const auto sw_iter = find(w_parents, source);
      assert(sw_iter != w_parents.end()); // since w_iter exists, sw_iter should exist!

      // if the PredStorage can be modified in place, we'll just change the node of the parent-adjacency of w to target
      if constexpr (is_inplace_modifyable<_PredStorage>) {
        adapt(target, *sw_iter);
        sw_iter->nd = target;
        const bool success = append(children(target), w, *sw_iter).second;
        // if we could not append w as child of target (that is, the edge target->w already existed) then the edge source->w should be deleted
        if(!success) w_parents.erase(sw_iter);
        return success;
      } else { // if the PredStorage cannot be modified in place, we'll have to remove the source_to_w adjacency, change it, and reinsert it
        // step 1: move EdgeData out of the parents-storage
        Adjacency source_to_w = std::value_pop(w_parents, sw_iter);
        // step 2: merge EdgeData with that passed with target
        adapt(target, source_to_w);
        // step 3: put back the Adjacency with changed node
        const auto [wpar_iter, success] = append(w_parents, static_cast<const NodeDesc>(target));

        if(success) {
          if constexpr (has_edge_data)
            wpar_iter->data_ptr = source_to_w.data_ptr;
          const bool target_success = append(children(target), w, *wpar_iter).second;
          assert(target_success);
        }
        return success;
      }
    }

    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void transfer_child(const NodeDesc source,
                        const Adj& target,
                        const NodeDesc w,
                        AdjAdapter&& adapt = AdjAdapter())
    {
      transfer_child(source, target, find(children(source), w), std::forward<AdjAdapter>(adapt));
    }

    // transfer all children of source_node to target, see comments of functions above
    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void transfer_children(const NodeDesc source, const Adj& target, AdjAdapter&& adapt = AdjAdapter()) {
      Node& source_node = node_of(source);
      auto& s_children = source_node.children();
#warning "TODO: improve repeated-erase performance if children are stored as vecS"
      Adjacency tmp(NoNode);
      while(!s_children.empty()) {
        auto w_iter = std::prev(std::end(s_children));
        if(*w_iter == target){
          if(w_iter != std::begin(s_children)) {
            tmp = std::move(*w_iter);
            s_children.erase(w_iter);
          } else break;
        } else transfer_child(source, target, w_iter, adapt);
      }
    }

    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void transfer_parent(const NodeDesc source,
                         const Adj& target,
                         const std::iterator_of_t<typename Node::PredContainer>& w_iter,
                         AdjAdapter&& adapt = AdjAdapter())
    {
      Node& source_node = node_of(source);
      
      assert(w_iter != source_node.parents().end());
      assert(*w_iter != target);

      const NodeDesc w = *w_iter;
      auto& w_children = children(w);
      // step 1: move EdgeData of w-->source out of the children-storage of w
      Adjacency ws = std::value_pop(w_children, source_node.get_desc());
      // step 2: merge EdgeData with that of uv
      adapt(target, ws);
      // step 3: put back the Adjacency with changed node
      const auto [wc_iter, success] = append(w_children, static_cast<NodeDesc>(target), std::move(ws));
      // step 4: add w as a new parent of the target
      if(success) append(parents(target), w, *wc_iter);
      // step 5: remove the old adjacency from s's parents
      source_node.parents().erase(w_iter);
    }
  
    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void transfer_parent(const NodeDesc source,
                         const Adj& target,
                         const NodeDesc w,
                         AdjAdapter&& adapt = AdjAdapter())
    {
      Node& source_node = node_of(source);
      auto w_iter = find(source_node.parents(), w);
      assert(w_iter != source_node.parents().end());
      transfer_parent(source, target, w_iter, std::forward<AdjAdapter>(adapt));
    }

    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void transfer_parents(const NodeDesc source, const Adj& target, AdjAdapter&& adapt = AdjAdapter()) {
      Node& source_node = node_of(source);
      auto& s_parents = source_node.parents();

      Adjacency tmp(NoNode);
      while(!s_parents.empty()) {
        auto w_iter = std::prev(std::end(s_parents));
        if(*w_iter == target) {
          // save the adjacency to target and go on
          if(w_iter != std::begin(s_parents)) {
            tmp = std::move(*w_iter);
            s_parents.erase(w_iter);
          } else break;
        } else transfer_parent(source, target, w_iter, adapt);
      }
      if(tmp != NoNode) append(s_parents, std::move(tmp));
    }

    // subdivide an edge uv with a new Adjacency w
    // NOTE: first, the edge wv gets its data from adapting the EdgeData passed with w and the EdgeData of u->v
    //        (if no adapter is passed, then wv gets the EdgeData of u->v)
    //       then, the edge uw gets its data from the Adjacency w
    template<AdjacencyType Adj, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void subdivide_edge(const NodeDesc u, const auto& v, Adj&& w, AdjAdapter&& adapt = AdjAdapter()) {
      assert(is_edge(u,v));
      assert(node_of(w).is_isolated());
      transfer_child(u, w, v, std::forward<AdjAdapter>(adapt));
      add_edge(u, std::forward<Adj>(w));
			count_node();
    }

    template<AdjacencyType Adj, EdgeType _Edge, AdjAdapterType<Adj, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
      requires std::is_same_v<std::remove_cvref_t<_Edge>, Edge>
    void subdivide_edge(_Edge&& uv, Adj&& w, AdjAdapter&& adapt = AdjAdapter()) {
      subdivide_edge(uv.tail(), std::forward<_Edge>(uv).head(), std::forward<Adj>(w), std::forward<AdjAdapter>(adapt));
    }


    // contract a node onto its unique parent
    template<AdjacencyType Adj, AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void contract_up(const NodeDesc v, Adj&& u_adj, AdjAdapter&& adapt = AdjAdapter()) {
      assert(in_degree(v) == 1);
      assert(u_adj == front(parents(v)));
      const NodeDesc u = u_adj;
      transfer_children(v, std::move(u_adj), std::forward<AdjAdapter>(adapt));
      // finally, remove the edge uv and free v's storage
      remove_edge(u, v);
      remove_node(v);
    }

    template<AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void contract_up(const NodeDesc v, AdjAdapter&& adapt = AdjAdapter()) {
      assert(in_degree(v) == 1);
      contract_up(v, front(parents(v)), std::forward<AdjAdapter>(adapt));
    }

    template<EdgeType _Edge, AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void contract_up(const _Edge& uv, AdjAdapter&& adapt = AdjAdapter()) {
      assert(in_degree(uv.head()) == 1);
      contract_up(uv.head(), uv.tail(), std::forward<AdjAdapter>(adapt));
    }

    // contract a node onto its unique child
    template<AdjacencyType Adj, AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void contract_down(const NodeDesc u, Adj&& v_adj, AdjAdapter&& adapt = AdjAdapter()) {
      assert(out_degree(u) == 1);
      assert(v_adj == front(children(u)));
      const NodeDesc v = v_adj;
      if(is_root(u)) {
        const auto iter = find(_roots, u);
        assert(iter != _roots.end());
        std::replace(_roots, iter, v);
      } else transfer_parents(u, std::move(v_adj), std::forward<AdjAdapter>(adapt));
      // finally, remove the edge uv and free u's storage
      remove_edge(u, v);
      remove_node(u);
    }

    template<AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void contract_down(const NodeDesc u, AdjAdapter&& adapt = AdjAdapter()) {
      assert(out_degree(u) == 1);
      contract_down(u, front(children(u)), std::forward<AdjAdapter>(adapt));
    }

    template<EdgeType _Edge, AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void contract_down(const _Edge& uv, AdjAdapter&& adapt = AdjAdapter()) {
      assert(out_degree(uv.tail()) == 1);
      contract_down(uv.tail(), uv.head(), std::forward<AdjAdapter>(adapt));
    }


    template<AdjAdapterType<Adjacency, Phylogeny> AdjAdapter = std::IgnoreFunction<void>>
    void suppress_node(const NodeDesc v, AdjAdapter&& adapt = AdjAdapter()) { contract_up(v, std::forward<AdjAdapter>(adapt)); }
    
    void remove_node(const NodeDesc v) {
      Node& v_node = node_of(v);
      // step 1: remove outgoing arcs of v
      const auto& v_children = v_node.children();
      while(!v_children.empty())
        remove_edge(v, front(v_children));
      // step 2: remove incoming arcs of v
      const auto& v_parents = v_node.parents();
      if(!v_parents.empty()){
        while(!v_parents.empty())
          remove_edge(front(v_parents), v);
      } else _roots.erase(v);
      // step 3: free storage
      delete_node(v);
    }

    // if we know that v is a leaf, we can remove v a bit faster
    void remove_leaf(const NodeDesc v) {
      Node& v_node = node_of(v);
      assert(v_node.is_leaf());
      // step 2: remove incoming arcs of v
      const auto& v_parents = v_node.parents();
      if(!v_parents.empty()){
        while(!v_parents.empty())
          remove_edge(front(v_parents), v);
      } else _roots.erase(v);
      // step 3: free storage
      delete_node(v);
    }  

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
    // NOTE: the empty network is considered a forest
    bool is_forest() const {
      if constexpr (!is_declared_tree)
        return empty() || (num_edges() + _roots.size() == _num_nodes);
      else return true;
    }
    // NOTE: the empty network is considered a tree
    bool is_tree() const {
      if constexpr (Parent::has_unique_root)
        return is_forest();
      else return (_roots.size() <= 1) && is_forest();
    }


    // =============== traversals ======================

    // list all nodes below u in order _o (default: postorder)
    template<TraversalType o = postorder>
    static auto nodes_below(const NodeDesc u, const order<o> _o = order<o>()) { return NodeTraversal<o, Phylogeny>(u); }
    // we can represent nodes that are forbidden to visit by passing either a container of forbidden nodes or a node-predicate
    template<class Forbidden, TraversalType o = postorder>
    static auto nodes_below(const NodeDesc u, Forbidden&& forbidden, const order<o> _o = order<o>()) {
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
    static auto edges_below(const NodeDesc u, const order<o> _o = order<o>()) {
      return AllEdgesTraversal<o, Phylogeny>(u);
    }
    template<class Forbidden, TraversalType o = postorder>
    static auto edges_below(const NodeDesc u, Forbidden&& forbidden, const order<o> _o = order<o>()) {
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
      return std::make_filtered_factory(nodes_below_preorder(std::forward<Args>(args)...).begin(), [](const NodeDesc n){return node_of(n).is_leaf();});
    }
    template<class... Args>
    auto leaves(Args&&... args) const { return leaves_below<const RootContainer&>(_roots, std::forward<Args>(args)...); }




    // ========================= LCA ===========================
    using TreeLCAOracle = NaiveTreeLCAOracle<Phylogeny>;
    using NetworkLCAOracle = NaiveNetworkLCAOracle<Phylogeny>;
    using LCAOracle = std::conditional_t<is_declared_tree, TreeLCAOracle, NetworkLCAOracle>;

    LCAOracle naiveLCA() const { return LCAOracle(*this); }
#warning "TODO: use more efficient LCA"
    LCAOracle LCA() const { return naiveLCA(); }

    // return the descendant among x and y unless they are incomparable; return NoNode in this case
    NodeDesc get_minimum(const NodeDesc x, const NodeDesc y) const {
      const NodeDesc lca = LCA()(x,y);
      if(lca == x) return y;
      if(lca == y) return x;
      return NoNode;
    }

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

    static bool are_siblings(const NodeDesc y, const NodeDesc z) {
      if(y != z) {
        const Node& y_node = node_of(y);
        const Node& z_node = node_of(z);
        if(!y_node.is_root() && !z_node.is_root()) {
          return std::are_disjoint(y_node.parents(), z_node.parents());
        } else return false;
       } else return true;
    }

    static NodeDesc common_parent(const NodeDesc y, const NodeDesc z) {
      const Node& y_node = node_of(y);
      if(y != z) {
        const Node& z_node = node_of(z);
      	if(!y_node.is_root() && !z_node.is_root()) {
          const auto iter = std::common_element(y_node.parents(), z_node.parents());
					return iter ? *iter : NoNode;
        } else return NoNode;
      } else return y_node.is_root() ? NoNode : y_node.any_parent();
    }

    static NodeVec common_parents(const NodeDesc y, const NodeDesc z) {
      NodeVec result;
      const Node& y_node = node_of(y);
      if(y != z) {
        const Node& z_node = node_of(z);
      	if(!y_node.is_root() && !z_node.is_root()) {
          if(y_node.in_degree() > z_node.in_degree()) {
            for(const auto& u: y_node.parents())
              if(test(z_node.parents(), u))
                result.push_back(u);
          } else {
            for(const auto& u: z_node.parents())
              if(test(y_node.parents(), u))
                result.push_back(u);
          }
        }
      } else {
        result.reserve(y_node.in_degree());
        for(const auto& u: y_node.parents()) result.push_back(u);
      }
      return result;
    }

  protected:
    // ================== construction helpers =====================
    // NOTE: these are powerful and can potentially leave your phylogeny in an inconsistent state
    //       (in particular if you use an EdgeEmplacer with 'track_roots = false' and forget to mark the roots afterwards)
    //       however, this power enables certain use cases where we want to "directly" access the edges of a network...
    //       just, promise to be careful with your EdgeEmplacers
    template<bool, StrictPhylogenyType, OptionalPhylogenyType, NodeTranslationType> friend struct ProtoEdgeEmplacementHelper;
    template<bool, StrictPhylogenyType, OptionalPhylogenyType, NodeTranslationType> friend struct EdgeEmplacementHelper;


    // emplace a set of new edges into *this
    // NOTE: to extract node/edge data, pass data-extraction functions when constructing the emplacer (see <edge_emplacement.hpp> and <extract_data.hpp>)
    // NOTE: node-/edge- data is passed into the extractor functions by move if the edge-container is not passed by const reference!
    // NOTE: the emplacer_args allow you to pass your own (partial) node translation old_to_new in order to pre-define node-correspondances (powerfull!)
    //       if you do this, make sure that all values in the translation are valid node descriptors (point to constructed nodes of the correct type)
    //       see <edge_emplacement.hpp> and <extract_data.hpp> for more info on passing node-translation functions
    template<std::IterableType Edges, EdgeEmplacerType Emplacer>
    void build_from_edges(Edges&& edges, Emplacer&& emplacer) {
      DEBUG3(std::cout << "init Network with edges "<<edges<<"\n");
      // step 1: copy other_x
      // step 2: copy everything below other_x
      for(auto&& e: std::forward<Edges>(edges))
        emplacer.emplace_edge(std::move(e));
    }

    // move the subtree below other_x into us by changing the parents of other_x to {x}
    // NOTE: the edge x --> other_x will be initialized using "args"
    // NOTE: _Phylo needs to have the same NodeType as we do!
    // NOTE: we are not removing other_x from ther other phylogeny; the caller should care for that!
    template<StrictPhylogenyType Phylo, class... Args> requires std::is_same_v<Node, typename Phylo::Node>
    void place_below_by_move(Phylo&& other, const NodeDesc other_x, const NodeDesc x, Args&&... args) {
      assert(other_x != NoNode);
      std::cout << "moving subtree below "<<other_x<<"...\n";
      // step 1: move other_x over to us
      auto& other_x_node = other.node_of(other_x);
      auto& other_x_parents = other_x_node.parents();
      const bool other_x_is_root = other_x_parents.empty();
      
      if(!other_x_parents.empty()){
        while(!other_x_parents.empty())
          other.remove_edge(front(other_x_parents), other_x_node);
      }

      if(x != NoNode) {
        const bool success = add_child(x, other_x, std::forward<Args>(args)...).second;
        assert(success);
      } else append(_roots, other_x);
      // step 2: update node and edge numbers
      std::pair<size_t,size_t> node_and_edge_count;

      // if other_x was the only root of other, then we'll use other's node- and edge- numbers, otherwise we'll have to count them :/
      if(other_x_is_root && other._roots.empty()) {
        node_and_edge_count = {other.num_nodes(), other.num_edges()};
      } else {
        node_and_edge_count = other.count_nodes_and_edges_below(other_x);
      }
      count_node(node_and_edge_count.first);
      count_edge(node_and_edge_count.second);
      other.count_node(-node_and_edge_count.first);
      other.count_edge(-node_and_edge_count.second);
    }

  public:
    // =============================== construction ======================================
    Phylogeny() = default;

    // initialize tree from any std::IterableType, for example, std::vector<PT::Edge<>>
    template<std::IterableType Edges, class... EmplacerArgs> requires (!PhylogenyType<Edges>)
    explicit Phylogeny(Edges&& edges, EmplacerArgs&&... args) {
      build_from_edges(std::forward<Edges>(edges), EdgeEmplacers<true>::make_emplacer(*this, std::forward<EmplacerArgs>(args)...));
      DEBUG2(tree_summary(std::cout));
    }

    // NOTE: construction from another (sub-)phylogeny largely depends on the data policy:
    // 1. "copy" (default)      - creates new nodes, copying/moving the data from the other nodes
    //                            (move data if the other phylogeny is presented as rvalue_reference)
    // 2. "move"                - simply moves the root from the given (sub-)phylogeny into *this as new root
    // NOTE: 2. requires that the given Phylogeny has the same NodeType as us (but may allow more/less roots)


    // "copy" construction with root(s)
    template<PhylogenyType Phylo, NodeIterableType RContainer, class... EmplacerArgs>
    Phylogeny(const policy_copy_t, Phylo&& N, const RContainer& in_roots, EmplacerArgs&&... args) {
      std::cout << "copy constructing phylogeny with "<<N.num_nodes()<< " nodes, "<<N.num_edges()<<" edges using roots "<<in_roots<<"\n";
      if(!in_roots.empty()) {
        auto emplacer = EdgeEmplacers<false, Phylo&&>::make_emplacer(*this, std::forward<EmplacerArgs>(args)...);
        if(in_roots.size() == 1) {
          build_from_edges(N.edges_below_preorder(front(in_roots)), emplacer);
        } else {
          build_from_edges(N.edges_below_preorder(in_roots), emplacer);
        }
        DEBUG4(std::cout << "marking roots: "<<in_roots<<"\n");
        // mark the roots
        for(const NodeDesc r: in_roots) append(_roots, emplacer[r]);
        DEBUG2(tree_summary(std::cout));
      }
    }
    
    // "copy" construction with single root
    template<PhylogenyType Phylo, class... EmplacerArgs>
    Phylogeny(const policy_copy_t, Phylo&& N, const NodeDesc in_root, EmplacerArgs&&... args) {
      std::cout << "copy constructing phylogeny with "<<N.num_nodes()<< " nodes, "<<N.num_edges()<<" edges using root "<<in_root<<"\n";
      auto emplacer = EdgeEmplacers<false, Phylo&&>::make_emplacer(*this, std::forward<EmplacerArgs>(args)...);
      build_from_edges(N.edges_below_preorder(in_root), emplacer);
      // mark the root
      DEBUG4(std::cout << "marking root: "<<emplacer[in_root]<<"\n");
      append(_roots, emplacer[in_root]);
      DEBUG2(tree_summary(std::cout));
    }

    // "copy" construction without root (using all roots of in_tree)
    template<PhylogenyType Phylo, class First, class... Args>
      requires (!NodeIterableType<First> && !std::remove_reference_t<Phylo>::has_unique_root)
    Phylogeny(const policy_copy_t, Phylo&& N, First&& first, Args&&... args):
      Phylogeny(policy_copy_t{}, std::forward<Phylo>(N), N.roots(), std::forward<First>(first), std::forward<Args>(args)...)
    {}
    template<PhylogenyType Phylo, class First, class... Args>
      requires (!NodeIterableType<First> && std::remove_reference_t<Phylo>::has_unique_root)
    Phylogeny(const policy_copy_t, Phylo&& N, First&& first, Args&&... args):
      Phylogeny(policy_copy_t{}, std::forward<Phylo>(N), front(N.roots()), std::forward<First>(first), std::forward<Args>(args)...)
    {}


    // "move" construction with root(s) by unlinking the in_roots from their in_tree and using them as our new _roots
    // NOTE: this requires the other phylogeny to have the same node type as we do; if that's not the case, copying cannot be avoided
    // NOTE: This will go horribly wrong if the sub-network below in_root has incoming arcs from outside the subnetwork!
    //       It is the user's responsibility to make sure this is not the case.
    template<StrictPhylogenyType Phylo, NodeIterableType RContainer> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_t, Phylo&& in_tree, const RContainer& in_roots) {
      for(const NodeDesc r: in_roots)
        place_below_by_move(std::move(in_tree), r, NoNode);
      // remember to remove the in_roots from the root storage of the in_tree
      if(&in_roots == &in_tree._roots)
        in_tree._roots.clear();
      else my_erase(in_tree._roots, in_roots);     
    }
    // "move" construction with single root
    template<StrictPhylogenyType Phylo, NodeIterableType RContainer> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_t, Phylo&& in_tree, const NodeDesc in_root) {
      place_below_by_move(std::move(in_tree), in_root, NoNode);
      // remember to manually remove the in_root from in_tree's root storage
      my_erase(in_tree._roots, in_root);
    }
    // "move" construction without root
    template<StrictPhylogenyType Phylo> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_t, Phylo&& in_tree):
      Phylogeny(policy_move_t{}, std::move(in_tree), in_tree.roots())
    {}


    // if we just want to copy a phylogeny, use the "copy" version (with or without node-translation)
    template<StrictPhylogenyType Phylo, NodeTranslationType OldToNew, class... Args>
    explicit Phylogeny(const Phylo& in_tree, OldToNew&& old_to_new, Args&&... args):
      Phylogeny(policy_copy_t(), in_tree, std::forward<OldToNew>(old_to_new), std::forward<Args>(args)...)
    {}
    template<StrictPhylogenyType Phylo, class First, class... Args> requires (!NodeTranslationType<First>)
    explicit Phylogeny(const Phylo& in_tree, First&& first, Args&&... args):
      Phylogeny(policy_copy_t(), in_tree, NodeTranslation(), std::forward<First>(first), std::forward<Args>(args)...)
    {}
    template<StrictPhylogenyType Phylo>
    explicit Phylogeny(const Phylo& in_tree):
      Phylogeny(policy_copy_t(), in_tree, NodeTranslation())
    {}
    // copy-constructing for the same type is not explicit
    Phylogeny(const Phylogeny& in_tree):
      Phylogeny(policy_copy_t(), in_tree, NodeTranslation())
    {}

    template<StrictPhylogenyType Phylo, class... Args>
      requires (!std::is_same_v<Phylo, Phylogeny> && std::is_same_v<Node, typename Phylo::Node>)
    explicit Phylogeny(Phylo&& in_tree, Args&&... args):
      Phylogeny(policy_move_t(), std::move(in_tree), in_tree.roots(), std::forward<Args>(args)...)
    {}
    // if we just want to move a phylogeny, we can delegate to the move-construction of the parent since we do not have members
    // NOTE: we have to make sure to remove the other phylogeny's roots, otherwise its destructor will destruct the roots as well
    Phylogeny(Phylogeny&& in_tree) = default;


    // swapping with an rvalue reference is just stealing their stuff - the ProtoPhylogeny knows how to do this
    void swap(Phylogeny&& other) {
      Parent::operator=(std::move(other));
    }
    // to swap with an lvalue reference, we let tmp steal our stuff, then steal other's stuff and finally let them steal tmp's stuff
    void swap(Phylogeny& other) {
      Phylogeny tmp(std::move(*this));
      Parent::operator=(std::move(other));
      other = std::move(tmp);
    }

    // assignment by copy-and-swap
    template<PhylogenyType Phylo>
    Phylogeny& operator=(Phylo&& other) {
      Phylogeny tmp(std::forward<Phylo>(other));
      swap(std::move(tmp));
      return *this;
    }

    // =================== i/o ======================

    std::ostream& tree_summary(std::ostream& os) const {
      DEBUG3(os << "network has "<< num_edges() <<" edges, "<< _num_nodes <<" nodes, "<<_roots.size()<<" roots\n");
      DEBUG3(os << "leaves: "<<leaves()<<"\n");
      DEBUG3(os << Parent::num_nodes() << " nodes: "<<nodes()<<'\n');
      DEBUG3(os << Parent::num_edges() << " edges: "<<edges()<<'\n');
      for(const NodeDesc u: nodes())
        os << u << ":" << "\tIN: "<< in_edges(u) << "\tOUT: "<< out_edges(u) << '\n';
      return os << "\n";
    }


    static void print_subtree(std::ostream& os, const NodeDesc u, std::string prefix, auto& seen) {
      const bool u_reti = is_reti(u);
      std::string u_name = std::to_string(name(u));
      if constexpr (Node::has_label){
        const std::string u_label = std::to_string(label(u));
        if(!u_label.empty())
          u_name += "[" + u_label + "]";
      }
      if(u_name == "") {
        if(u_reti)
          u_name = std::string("(" + std::to_string(u) + ")");
        else if(!is_leaf(u)) u_name = std::string("+");
      }
      os << '-' << u_name;
      if(u_reti) os << 'R';
      
      bool u_seen = true;
      if(!u_reti || !(u_seen = test(seen, u))) {
        const auto& u_childs = children(u);
        if(u_reti) append(seen, u);
        switch(u_childs.size()){
          case 0:
            os << std::endl;
            break;
          case 1:
            prefix += std::string(u_name.length() + 1, ' ');
            print_subtree(os, front(children(u)), prefix, seen);
            break;
          default:
            prefix += std::string(u_name.length(), ' ') + '|';

            uint32_t count = u_childs.size();
            for(const NodeDesc c: u_childs){
              print_subtree(os, c, prefix, seen);
              if(--count > 0) os << prefix;
              if(count == 1) prefix.back() = ' ';
            }
        }
      } else os << std::endl;
    }

    static void print_subtree(std::ostream& os, const NodeDesc u) {
      NodeSet tmp;
      print_subtree(os, u, "", tmp);
    }
    
    void print_subtree(std::ostream& os) const { print_subtree(os, front(_roots)); }


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
