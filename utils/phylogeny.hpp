
#pragma once

#include <iostream>
#include "set_interface.hpp"

#include "utils.hpp"
#include "config.hpp"
#include "types.hpp"
#include "tags.hpp"
#include "lca.hpp"
#include "except.hpp"
#include "node.hpp"
#include "edge.hpp"
#include "induced_tree.hpp"
#include "edge_emplacement.hpp"
#include "extract_data.hpp"

#ifdef DFSCORO
#include "dfs_coro.hpp"
#else
#include "dfs.hpp"
#endif


namespace PT {

	// ================ ProtoPhylogeny ======================
	// in the proto phylogeny, differences between trees (singleS predecessor storage)
	// and networks manifest (for example, for networks, we have to count the number of edges)

  // ------- ProtoPhylogeny: main class ---------
  template<StorageEnum PredStorage_,
           StorageEnum SuccStorage_,
           class NodeData_ = void,
           class EdgeData_ = void,
           class LabelType_ = void,
           StorageEnum RootStorage_ = singleS,
           template<StorageEnum, StorageEnum, class, class, class> class Node_ = PT::DefaultNode>
  struct ProtoPhylogeny:
    public NodeAccess<Node_<PredStorage_, SuccStorage_, NodeData_, EdgeData_, LabelType_>> 
  {
    // ------- static stuff --------
    static constexpr StorageEnum RootStorage = RootStorage_;
    static constexpr bool has_unique_root = (RootStorage == singleS);
    static constexpr bool is_declared_tree = false;
    
    using RootContainer = StorageClass<RootStorage_, NodeDesc>;
    using DefaultSeen = NodeSet;

    // ------- members --------
  protected:
#warning "TODO: make counting nodes and edges optional by template!"
    RootContainer _roots;
		size_t _num_nodes = 0u;
		size_t _num_edges = 0u;

    // ------- construction & desctruction ---------
	public:
    ProtoPhylogeny() = default;
    ProtoPhylogeny(const ProtoPhylogeny&) = delete;
    ProtoPhylogeny& operator=(const ProtoPhylogeny&) = delete;

    ProtoPhylogeny(ProtoPhylogeny&& other) noexcept:
      _roots(std::move(other._roots)),
      _num_nodes(other._num_nodes),
      _num_edges(other._num_edges)
    {
      other._roots.clear();
    }

    // ------- operators --------    
    auto& operator=(ProtoPhylogeny&& other) noexcept {
      _roots = std::move(other._roots);
      _num_nodes = other._num_nodes;
      _num_edges = other._num_edges;
      other._roots.clear();
      return *this;
    }

    // ------- methods: initialization --------
    // ------- methods: query --------
  public:
    size_t num_nodes() const { return _num_nodes; }
		size_t num_edges() const { return _num_edges; }
		size_t num_roots() const { return _roots.size(); }
    bool is_forest() const { return _num_nodes == _num_edges + num_roots(); }
    bool is_tree() const { return is_forest() and (num_roots() <= 1); }
    NodeDesc root() const { return mstd::front(_roots); }
    const RootContainer& roots() const & { return _roots; }
    RootContainer&& roots() && { return std::move(_roots); }
   
    // ------- methods: modification --------
  protected:
    void count_node(const int nr = 1) { _num_nodes += nr; } 
		void count_edge(const int nr = 1) { _num_edges += nr; }

  public:
    void clear() {
      _num_nodes = 0;
      _num_edges = 0;
      _roots.clear();
    }
	};

	// proto phylogeny with maximum in-degree 1 (for use as trees/forests)
  template<StorageEnum SuccStorage_,
           class NodeData_,
           class EdgeData_,
           class LabelType_,
           StorageEnum RootStorage_,
           template<StorageEnum, StorageEnum, class, class, class> class Node_>
  struct ProtoPhylogeny<singleS, SuccStorage_, NodeData_, EdgeData_, LabelType_, RootStorage_, Node_>:
    public NodeAccess<Node_<singleS, SuccStorage_, NodeData_, EdgeData_, LabelType_>>
  {
    // ------- static stuff --------
    static constexpr StorageEnum RootStorage = RootStorage_;
    static constexpr bool has_unique_root = (RootStorage == singleS);
    static constexpr bool is_declared_tree = true;
    
    using RootContainer = StorageClass<RootStorage_, NodeDesc>;
    using DefaultSeen = void;

		static constexpr void count_edge(const int nr = 1) {}
    static constexpr bool is_forest() { return true; }
    
    // ------- members --------
  protected:
    RootContainer _roots;
		size_t _num_nodes = 0;

    // ------- construction & desctruction ---------
  public:
    ProtoPhylogeny() = default;
    ProtoPhylogeny(const ProtoPhylogeny&) = delete;
    ProtoPhylogeny& operator=(const ProtoPhylogeny&) = delete;

    ProtoPhylogeny(ProtoPhylogeny&& other) noexcept:
      _roots(std::move(other._roots)),
      _num_nodes(other._num_nodes)
    {
      other._roots.clear();
    }

    // ------- operators --------
    auto& operator=(ProtoPhylogeny&& other) noexcept {
      _roots = std::move(other._roots);
      _num_nodes = other._num_nodes;
      other._roots.clear();
      return *this;
    }

    // ------- methods: initialization --------
    // ------- methods: query --------
    bool is_tree() const { return num_roots() <= 1; }
		size_t num_nodes() const { return _num_nodes; }
		size_t num_roots() const { return _roots.size(); }
		size_t num_edges() const { return (_num_nodes == 0) ? 0 : _num_nodes - num_roots(); }
    NodeDesc root() const { return mstd::front(_roots); }
    const RootContainer& roots() const & { return _roots; }
    RootContainer&& roots() && { return std::move(_roots); }

    // ------- methods: modification --------

  protected:
		void count_node(const int nr = 1) { _num_nodes += nr; }

    void clear() {
      _num_nodes = 0;
      _roots.clear();
    }
	};



  // ========== Phylogeny ==========
  // This is the main network/tree class. It's basically just a NodeAccess
  // with node/edge counting and some added functionality
 
  // ------- Phylogeny: helpers --------- 
  // for phylogeniess that do not support multiple edges:
  // when inserting an edge that is already in the phylogeny, we can either ignore the incident, abort the insertion or count the # of such events
  enum class UniquenessBy { ignore, abort, count };
  
  // ------- Phylogeny: main class ---------
  template<StorageEnum PredStorage_,
           StorageEnum SuccStorage_,
           class NodeData_ = void,
           class EdgeData_ = void,
           class LabelType_ = void,
           StorageEnum RootStorage_ = singleS,
           template<StorageEnum, StorageEnum, class, class, class> class Node_ = PT::DefaultNode>
  struct Phylogeny:
    public ProtoPhylogeny<PredStorage_, SuccStorage_, NodeData_, EdgeData_, LabelType_, RootStorage_, Node_>
  {
    // ------- static stuff --------
    using Parent = ProtoPhylogeny<PredStorage_, SuccStorage_, NodeData_, EdgeData_, LabelType_, RootStorage_, Node_>;
 
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
    using SuccIterator = mstd::iterator_of_t<typename Node::SuccContainer>;
    using PredIterator = mstd::iterator_of_t<typename Node::PredContainer>;
    using Parent::node_of;
    using Parent::parents;
    using Parent::parent;
    using Parent::children;
    using Parent::child;
    using Parent::in_degree;
    using Parent::out_degree;
    using Parent::degree;
    using Parent::in_edges;
    using Parent::out_edges;
		using Parent::num_edges;
    using Parent::num_roots;
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
    using Parent::create_node;

    // ------- members --------
    // ------- construction & desctruction ---------
    // ------- operators --------
    // ------- methods: initialization --------
    // ------- methods: query --------
    // ------- methods: modification --------

  protected:
    void delete_node(const NodeDesc x) {
			count_node(-1);
      Parent::delete_node(x);
    }
  public:

    // add an edge between two nodes of the tree/network
    // NOTE: the nodes are assumed to have been added by add_root(), add_child(), or add_parent() (of another node) before!
	  // NOTE: Edge data can be passed either in the Adjacency v or with additional args (the latter takes preference)
    // NOTE: don't add edges incoming to any root! Use add_parent or transfer_above_root for this
    template<AdjacencyType Adj, class... Args>
    auto add_edge(const NodeDesc u, Adj&& v, Args&&... args) {
      assert(!test(_roots, v));
      const auto result = Parent::add_child(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      if(result.second) {
        const bool res = Parent::add_parent(v, u, *(result.first)).second;
        assert(res and "u is a predecessor of v, but v is not a successor of u. This should never happen!");
				count_edge();
      }
      return result;
    }

    /*
    template<AdjacencyType Adj, DataExtracterType DataMaker>
    std::pair<Adjacency, bool> add_edge(const NodeDesc u, Adj&& v, DataMaker&& data_maker) {
      using StrictDataMaker = std::remove_reference_t<DataMaker>;
      if constexpr (!StrictDataMaker::ignoring_edge_data)
        return add_edge(u, std::forward<Adj>(v), data_maker.get_edge_data(u, v));
      else return add_edge(u, std::forward<Adj>(v));
    }
    */

    // remove an edge, updating edge numbers but not roots
    bool remove_edge_no_cleanup(const NodeDesc u, const NodeDesc v) {
      const int result = Parent::remove_edge(u,v);
      count_edge(-result);
      return result;
    }
    bool remove_edge_no_cleanup(const Edge& uv) { return remove_edge_no_cleanup(uv.tail(), uv.head()); }
    // remove an edge incoming to a leaf-node, along with said leaf-node
    bool remove_edge_and_child(const NodeDesc u, const NodeDesc v) {
      const bool result = remove_edge_no_cleanup(u, v);
      if(result) {
        assert(in_degree(v) == 0);
        assert(out_degree(v) == 0);
        delete_node(v);
        return true;
      } else return false;
    }
    // remove an edge outgoing from a node u, along with u itself
    // NOTE:  this does not maintain the root-set, you'll have to do that by hand!
    bool remove_edge_and_parent(const NodeDesc u, const NodeDesc v) {
      const bool result = remove_edge_no_cleanup(u, v);
      if(result) {
        assert(in_degree(u) == 0);
        assert(out_degree(u) == 0);
        delete_node(u);
        return true;
      } else return false;
    }


    bool remove_edge(const NodeDesc u, const NodeDesc v) {
      const bool result = remove_edge_no_cleanup(u, v);
      if(in_degree(v) == 0) mstd::append(_roots, v);
      return result;
    }

    // remove the given node u, as well as all ancestors who, after all removals, have no children left
    // if suppress_deg2 is true, then also suppress deg2-nodes encountered on the way
    template<bool suppress_deg2 = true>
    void remove_upwards(const NodeDesc v) {
      auto& v_node = node_of(v);
      const size_t v_indeg = v_node.in_degree();
      const size_t v_outdeg = v_node.out_degree();
      switch(v_outdeg) {
        case 0: {
          auto& v_parents = v_node.parents();
          while(!v_parents.empty()) {
            const NodeDesc u = mstd::front(v_parents);
            remove_edge_no_cleanup(u,v);
            remove_upwards<suppress_deg2>(u);
          }
          delete_node(v);
          return;
        }
        case 1: {
          if constexpr (suppress_deg2) {
            if(v_indeg == 1) {
              const NodeDesc u = v_node.any_parent();
              contract_up(v);
              remove_upwards<suppress_deg2>(u);
            }
          }
          return;
        }
      }
    }
    void remove_upwards_no_suppression(const NodeDesc v) { remove_upwards<false>(v); }


    // new nodes can only be added as 1. new roots, 2. children or 3. parents of existing nodes
    // NOTE: the edge data can either be passed using the Adjacency v or constructed from the parameter args
    template<AdjacencyType Adj, class... Args>
    auto add_child(const NodeDesc u, Adj&& v, Args&&... args) {
      assert(u != static_cast<NodeDesc>(v));
      DEBUG5(std::cout << "adding edge from " << u << " to " << v <<"\n");
      const auto result = add_edge(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      count_node(result.second);
      return result;
    }

    template<AdjacencyType Adj, class... Args>
    auto add_parent(Adj&& v, const NodeDesc u, Args&&... args) {
      assert(degree(u) == 0);
      const auto result = add_edge(u, std::forward<Adj>(v), std::forward<Args>(args)...);
      if(result.second) {
				count_node();
        mstd::erase(_roots, v);
        mstd::append(_roots, u);
      }
      return result;
    }

    // this marks a node as new root
    bool mark_root(const NodeDesc u) {
      if(in_degree(u) > 0) throw std::logic_error("trying to mark a node with in-degree as root");
      return mstd::append(_roots, u).second;
    };
    // this marks nodes as new roots
    template<NodeIterableType Roots>
    void mark_roots(const Roots& roots) {
      for(const NodeDesc r: roots)
        mark_root(r);
    };

    NodeDesc add_root(const NodeDesc new_root) {
      assert(degree(new_root) == 0);
      DEBUG5(std::cout << "adding "<<new_root<<" to roots\n");
      const bool result = mark_root(new_root);
      assert(result);
      count_node();
      return new_root;
    }
    template<class First, class... Args> requires (!std::is_same_v<std::remove_cvref_t<First>, NodeDesc>)
    NodeDesc add_root(First&& first, Args&&... args) {
      return add_root(create_node(std::forward<First>(first), std::forward<Args>(args)...));
    }
    NodeDesc add_root() {
      return add_root(create_node());
    }


    // split a node off the network and install it as new root above r
    // NOTE: all edges incoming to x are going to be deleted
    // NOTE: all edges outgoing from x are kept
    template<class... Args>
    void transfer_above_root(const NodeDesc x, const NodeDesc r, Args&&... args) {
      // step 1: remove all incoming edges of x
      auto& x_parents = parents(x);
      while(!x_parents.empty())
        remove_edge_no_cleanup(mstd::front(x_parents), x);

      const auto [iter, success] = Parent::add_child(x, r, std::forward<Args>(args)...);
      assert(success);
      const bool res = Parent::add_parent(r, x, *iter).second;
      assert(res and "u is a predecessor of v, but v is not a successor of u. This should never happen!");
      count_edge();
      mstd::erase(_roots, r);
      mstd::append(_roots, x);
    }
    void transfer_above_root(const NodeDesc x) { transfer_above_root(x, root()); }

    // transfer a child w of source to target, return whether the child was transfered successfully
    // NOTE: this will not create loops
    // NOTE: if w is already a child of target, the behavior of the function is governed by the "uniqueness" variable:
    //       if uniqueness == ignore, then no precaution is taken
    //          ATTENTION: if the SuccStorage cannot exclude duplicates, then this can lead to double edges!
    //       if uniqueness == abort, then the function will return false without further modifying the phylogeny
    //       if uniqueness == count, then the existing edge from target to w survives while the other is deleted; also, false is returned
    // NOTE: use the DataMaker to modify the EdgeData of the newly formed edge
    //       for example like so: [](const auto& uv, auto& vw) { vw.data() += uv.data(); }
    //       By default the target->... EdgeData is ignored.
    // NOTE: theoretically, it is possible that the target node does not belong to the same phylogeny as the source node,
    //       but this is highly discouraged (unless node/edge count is disabled) because the node and edge counts will be wrong
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    bool transfer_child(const SuccIterator& w_iter,
                        const NodeDesc source,
                        const Adj& target,                        
                        DataMaker&& make_data = DataMaker())
    {
      assert(w_iter != children(source).end());
      const NodeDesc w = *w_iter;
      assert(w != target);
      DEBUG5(std::cout << "transferring child "<< w << " from "<<source<<" to "<<target<<"\n");

      auto& w_parents = parents(w);
      const auto sw_iter = mstd::find(w_parents, source);
      assert(sw_iter != w_parents.end()); // since w_iter exists, sw_iter should exist!
      
      if constexpr (uniqueness != UniquenessBy::ignore) {
        if(mstd::test(w_parents, target)) {
          if constexpr (uniqueness == UniquenessBy::count) {
            DEBUG5(std::cout << "detected double-edge - counting\n");
            children(source).erase(w_iter);
            w_parents.erase(sw_iter);
            count_edge(-1);
          } else {DEBUG5(std::cout << "detected double-edge - aborting\n");}
          return false;
        }
      }
      children(source).erase(w_iter);
      // if the PredStorage can be modified in place, we'll just change the node of the parent-adjacency of w to target
      if constexpr (is_inplace_modifyable<PredStorage_>) {
        // set the target in-place
        sw_iter->nd = target;
        // insert a new adjacency into target's children
        const bool success = mstd::append(children(target), w, mstd::apply_or_pass2(make_data, target, *sw_iter)).second;
        // if we could not append w as child of target (that is, the edge target->w already existed) then the edge source->w should be deleted
        if(!success){
          assert(mstd::test(children(target), *sw_iter)); 
          w_parents.erase(sw_iter);
          count_edge(-1);
          return false;
        } else return true;
      } else { // if the PredStorage cannot be modified in place, we'll have to remove the source_to_w adjacency, change it, and reinsert it
        // step 1: move EdgeData out of the parents-storage
        // step 2: merge EdgeData with that passed with target
        Adjacency source_to_w = apply_or_pass2(make_data, target, mstd::value_pop(w_parents, sw_iter));
        // step 3: put back the Adjacency with changed node
        const auto [wpar_iter, success] = mstd::append(w_parents, static_cast<const NodeDesc>(target), std::move(source_to_w));

        if(success) {
          const bool target_success = mstd::append(children(target), w, *wpar_iter).second;
          assert(target_success);
          return true;
        } else {
          count_edge(-1);
          return false;
        }
      }
    }
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    bool transfer_child(const NodeDesc w,
                        const NodeDesc source,
                        const Adj& target,
                        DataMaker&& make_data = DataMaker())
    { return transfer_child<uniqueness>(mstd::find_reverse(children(source), w), source, target, make_data); }

    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
      requires (!AdjacencyType<DataMaker>)
    bool transfer_child(const NodeDesc w, const Adj& target, DataMaker&& make_data = DataMaker()) {
      assert(in_degree(w) == 1);
      return transfer_child<uniqueness>(w, parent(w), target, make_data);
    }
    
    template<class... Args>
    bool transfer_child_abort(Args&&... args) { return transfer_child<UniquenessBy::abort>(std::forward<Args>(args)...); }

    template<class... Args>
    bool transfer_child_count(Args&&... args) { return transfer_child<UniquenessBy::count>(std::forward<Args>(args)...); }

    // transfer all children of source_node to target, see comments of functions above
    // NOTE: return the number of children of v that were already children of target
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    size_t transfer_children(const NodeDesc source, const Adj& target, DataMaker&& make_data = DataMaker()) {
      size_t result = 0;
      auto& s_children = children(source);
      typename Parent::SuccContainer tmp;
#warning "TODO: improve repeated-erase performance if children are stored as vecS"
#warning "TODO: in particular, remove children from the BACK of the container EVERYWHERE!"
#warning "TODO: implement uniqueness by abort"
      while(!s_children.empty()) {
        auto w_iter = std::prev(std::end(s_children));
        if(*w_iter == target) {
          append(tmp, std::move(*w_iter));
          s_children.erase(w_iter);
        } else {
          const bool success = transfer_child<uniqueness>(w_iter, source, target, make_data);
          DEBUG5(std::cout << "done transferring child from "<<source<<" to "<<target<<" - success: "<<success<<"\n");
          if constexpr (uniqueness == UniquenessBy::abort) {
            if(!success){
              append(tmp, std::move(*w_iter));
              s_children.erase(w_iter);
            }
          }
          result += !success;
        }
      }
      assert(s_children.empty());
      
      // restore target (and double-edge-preventions) in the children of the source
      if(!tmp.empty()) s_children = std::move(tmp);
      return result;
    }
    template<class... Args>
    size_t transfer_children_abort(Args&&... args) { return transfer_children<UniquenessBy::abort>(std::forward<Args>(args)...); }
    template<class... Args>
    size_t transfer_children_count(Args&&... args) { return transfer_children<UniquenessBy::count>(std::forward<Args>(args)...); }

  protected:
    // transfer the parent *w_iter of the given source to the given target
    // NOTE: w_iter must be an interator into parents(w)
    // NOTE: return false if we failed to inert *w_iter into the parents of the target (if *w_iter was already a parent of target before)
    // NOTE: for uniqueness handing, see transfer_child(..)
    // NOTE: while source WILL become a root by this operation, it is NOT registered in _roots! --> do this by hand if required!
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    bool transfer_parent(const PredIterator& w_iter,
                         const NodeDesc source,
                         const Adj& target,                         
                         DataMaker&& make_data = DataMaker())
    {
#warning "TODO: use inplace_modifyable to improve performance"
      auto& s_parents = parents(source);
      assert(w_iter != s_parents.end());
      assert(*w_iter != target);

      const NodeDesc w = *w_iter;
      s_parents.erase(w_iter);
      
      auto& w_children = children(w);

      auto& t_parents = parents(target);
      if constexpr (uniqueness != UniquenessBy::ignore) {
        if(mstd::test(t_parents, w)) {
          if constexpr (uniqueness == UniquenessBy::count) {
            mstd::erase(w_children, source);
            count_edge(-1);
          }
          return false;
        }
      }
      // step 1: move Adjacency of w-->source out of the children-storage of w
      Adjacency ws = mstd::apply_or_pass2(make_data, target, mstd::value_pop(w_children, source));
      // step 3: put back the Adjacency with changed node
      const auto [wc_iter, success] = mstd::append(w_children, static_cast<NodeDesc>(target), std::move(ws));
      // step 4: add w as a new parent of the target
      if(success) {
        const bool t_success = mstd::append(t_parents, w, *wc_iter).second;
        assert(t_success);
        return true;
      } else {
        count_edge(-1);
        return false;
      }
    }
  
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    bool transfer_parent(const NodeDesc w,
                         const NodeDesc source,
                         const Adj& target,                         
                         DataMaker&& make_data = DataMaker())
    {
      Node& source_node = node_of(source);
      auto w_iter = mstd::find(source_node.parents(), w);
      assert(w_iter != source_node.parents().end());
      return transfer_parent<uniqueness>(w_iter, source, target, make_data);
    }
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
      requires (!AdjacencyType<DataMaker>)
    bool transfer_parent(const NodeDesc w, const Adj& target, DataMaker&& make_data = DataMaker()) {
      assert(out_degree(w) == 1);
      const NodeDesc source = child(w);
      return transfer_parent<uniqueness>(mstd::find(parents(source), w), source, target, make_data);
    }

    template<class... Args>
    bool transfer_parent_abort(Args&&... args) { return transfer_parent<UniquenessBy::abort>(std::forward<Args>(args)...); }

    template<class... Args>
    bool transfer_parent_count(Args&&... args) { return transfer_parent<UniquenessBy::count>(std::forward<Args>(args)...); }


    // transfer the parents of the given source to the given parent
    // NOTE: return the number of parents of source that were already parents of target
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    size_t transfer_parents(const NodeDesc source, const Adj& target, DataMaker&& make_data = DataMaker()) {
      size_t result = 0;
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
        } else result += !transfer_parent<uniqueness>(w_iter, source, target, make_data);
      }
      if(tmp != NoNode) mstd::append(s_parents, std::move(tmp));
      return result;
    }
    template<class... Args>
    size_t transfer_parents_abort(Args&&... args) { return transfer_parents<UniquenessBy::abort>(std::forward<Args>(args)...); }

    template<class... Args>
    size_t transfer_parents_count(Args&&... args) { return transfer_parents<UniquenessBy::count>(std::forward<Args>(args)...); }


  public:

    // re-hang a node v below a node target
    // NOTE: this removes all incoming edges of u except target->u
    template<AdjacencyType Adj, class... Args>
    void replace_parents(Adj v, const NodeDesc target, Args&&... args) {
      assert(v != NoNode);
      auto& v_parents = parents(v);
      // step 1: remove v from all children-sets of its parents
      // NOTE: if target is already a parent of v, then v will not be removed from the child-set of target
      auto iter = v_parents.end();
      for(auto i = v_parents.begin(); i != v_parents.end(); ++i) {
        if(*i != target) {
          mstd::erase(children(*i), v);
        } else iter = i;
      }
      // step 2: clear v_parents and count edges
      count_edge(-v_parents.size());
      if(iter != v_parents.end()) {
        clear_except(v_parents, iter);
        count_edge();
      } else {
        v_parents.clear();
        add_child(target, std::move(v), std::forward<Args>(args)...); 
      }
    }


    // subdivide an edge uv with a new Adjacency w
    // NOTE: first, the edge wv gets its data from adapting the EdgeData passed with w and the EdgeData of u->v
    //        (if no adapter is passed, then wv gets the EdgeData of u->v)
    //       then, the edge uw gets its data from the Adjacency w
    template<AdjacencyType Adj, class DataMaker = bool>
    void subdivide_edge(const NodeDesc u, const auto& v, Adj&& w, DataMaker&& make_data = DataMaker()) {
      assert(is_edge(u,v));
      assert(node_of(w).is_isolated());
      // step 1: transfer v from u to w
      transfer_child(v, u, w, make_data);
      // step 2: add edge u->w
      if constexpr (DataExtracterType<DataMaker>)
        add_edge(u, std::forward<Adj>(w), make_data);
      else add_edge(u, std::forward<Adj>(w));
			count_node();
    }

    template<AdjacencyType Adj, EdgeType Edge_, class DataMaker = bool>
    void subdivide_edge(Edge_&& uv, Adj&& w, DataMaker&& make_data = DataMaker()) {
      subdivide_edge(uv.tail(), std::forward<Edge_>(uv).head(), std::forward<Adj>(w), make_data);
    }

    // subdivide an edge, creating a new node
    // NOTE: if a NodeFunctionType or a DataExtracterType is passed, then we try to initialize the node data with it
    //       if DataMaker is invocable with 2 Adjacencies, then we use it to set the edge data
    template<EdgeType Edge_, class DataMaker = bool> requires (!AdjacencyType<DataMaker>)
    void subdivide_edge(Edge_&& uv, DataMaker&& data_maker = DataMaker()) {
      if constexpr (DataExtracterType<DataMaker> || NodeFunctionType<DataMaker>)
        subdivide_edge(std::forward<Edge_>(uv), create_node(data_maker), data_maker);
      else
        subdivide_edge(std::forward<Edge_>(uv), create_node(), data_maker);
    }


    // contract a node v onto its unique parent u
    // NOTE: return the number of children of v that were already children of the parent of v
    // NOTE: v will be deleted!
    // NOTE: set uniqueness to something other than UniquenessBy::ignore in order to prevent double-edges
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class DataMaker = bool>
    size_t contract_up(const NodeDesc v, const Adj& u_adj, DataMaker&& make_data = DataMaker()) {
      assert(in_degree(v) == 1);
      assert(u_adj == mstd::front(parents(v)));
      const NodeDesc u = u_adj;
      const size_t result = transfer_children<uniqueness>(v, u_adj, make_data);
      // finally, remove the edge uv and free v's storage
      remove_edge_and_child(u, v);
      return result;
    }

    template<UniquenessBy uniqueness = UniquenessBy::abort, class DataMaker = bool> requires (!AdjacencyType<DataMaker>)
    size_t contract_up(const NodeDesc v, DataMaker&& make_data = DataMaker()) {
      assert(in_degree(v) == 1);
      return contract_up<uniqueness>(v, mstd::front(parents(v)), make_data);
    }

    template<UniquenessBy uniqueness = UniquenessBy::abort, EdgeType Edge_, class DataMaker = bool> requires (!AdjacencyType<DataMaker>)
    size_t contract_up(const Edge_& uv, DataMaker&& make_data = DataMaker()) {
      assert(in_degree(uv.head()) == 1);
      return contract_up<uniqueness>(uv.head(), uv.tail(), make_data);
    }
    template<class... Args>
    size_t contract_up_abort(Args&&... args) { return contract_up<UniquenessBy::abort>(std::forward<Args>(args)...); }

    template<class... Args>
    size_t contract_up_count(Args&&... args) { return contract_up<UniquenessBy::count>(std::forward<Args>(args)...); }
    
    template<class... Args>
    size_t contract_up_ignore(Args&&... args) { return contract_up<UniquenessBy::ignore>(std::forward<Args>(args)...); }


    // contract a node u onto its unique child v
    // NOTE: return the number of parents of u that were already parents of v
    // NOTE: u will be deleted!
    template<UniquenessBy uniqueness = UniquenessBy::abort, AdjacencyType Adj, class... Args>
    size_t contract_down(const NodeDesc u, const Adj& v_adj, Args&&... args) {
      assert(out_degree(u) == 1);
      assert(v_adj == mstd::front(children(u)));
      const NodeDesc v = v_adj;
      std::cout << "contracting "<<u<<" onto "<<v<<"\n";
      size_t result = 0;
      if(is_root(u)) {
        const auto iter = mstd::find(_roots, u);
        assert(iter != _roots.end());
        if(in_degree(v) == 1) {
          mstd::replace(_roots, iter, v);
        } else mstd::erase(_roots, iter);
        assert(!_roots.empty());
      } else result = transfer_parents<uniqueness>(u, v_adj, std::forward<Args>(args)...);
      // finally, remove the edge uv and free u's storage
      remove_edge_and_parent(u, v);
      return result;
    }

    template<UniquenessBy uniqueness = UniquenessBy::abort, class DataMaker = bool> requires (!AdjacencyType<DataMaker>)
    size_t contract_down(const NodeDesc u, DataMaker&& make_data = DataMaker()) {
      assert(out_degree(u) == 1);
      return contract_down<uniqueness>(u, mstd::front(children(u)), make_data);
    }

    template<UniquenessBy uniqueness = UniquenessBy::abort, EdgeType Edge_, class DataMaker = bool> requires (!AdjacencyType<DataMaker>)
    size_t contract_down(const Edge_& uv, DataMaker&& make_data = DataMaker()) {
      assert(out_degree(uv.tail()) == 1);
      return contract_down<uniqueness>(uv.tail(), uv.head(), make_data);
    }
    template<class... Args>
    size_t contract_down_abort(Args&&... args) { return contract_down<UniquenessBy::abort>(std::forward<Args>(args)...); }

    template<class... Args>
    size_t contract_down_count(Args&&... args) { return contract_down<UniquenessBy::count>(std::forward<Args>(args)...); }

    template<class... Args>
    size_t contract_down_ignore(Args&&... args) { return contract_down<UniquenessBy::ignore>(std::forward<Args>(args)...); }


    // if v's in-degree is 1, then contract-up v, otherwise, contract-down v
    // NOTE: v will be deleted!
    template<UniquenessBy uniqueness = UniquenessBy::abort>
    void suppress_node(const NodeDesc v) {
      if(in_degree(v) == 1) {
        contract_up<uniqueness>(v);
      } else {
        contract_down<uniqueness>(v);
      }
    }
    template<class... Args>
    void suppress_node_abort(Args&&... args) { suppress_node<UniquenessBy::abort>(std::forward<Args>(args)...); }

    template<class... Args>
    void suppress_node_count(Args&&... args) { suppress_node<UniquenessBy::count>(std::forward<Args>(args)...); }



    void remove_node(const NodeDesc v) {
      Node& v_node = node_of(v);
      // step 1: remove outgoing arcs of v
      const auto& v_children = v_node.children();
      while(!v_children.empty())
        remove_edge_no_cleanup(v, mstd::front(v_children));
      // step 2: remove incoming arcs of v
      const auto& v_parents = v_node.parents();
      if(!v_parents.empty()){
        while(!v_parents.empty())
          remove_edge_no_cleanup(mstd::front(v_parents), v);
      } else _roots.erase(v);
      // step 3: free storage
      delete_node(v);
    }

    void remove_node(const auto& v, auto&& goodbye) {
      goodbye(v);
      remove_node(v);
    }

    // remove the subtree rooted at u
    // NOTE: LastRites can be used to say goodbye to your node(s) (remove it from other containers or whatever)
    template<class LastRites = mstd::IgnoreFunction<>>
    void remove_subtree(const NodeDesc u, LastRites&& goodbye = LastRites()) {
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(mstd::back(C), goodbye);
      remove_node(u, goodbye);
    }

    template<class LastRites = mstd::IgnoreFunction<>>
    void clear(LastRites&& goodbye = LastRites()) {
#ifdef NDEBUG
      for(const NodeDesc v: nodes_postorder()) {
        goodbye(v);
        Parent::delete_node(v);
      }
#else
      // NOTE: the debug messages in the DFS code cause memory access on freed nodes, so we copy the nodes into a vector first
      const NodeVec all_nodes = nodes_preorder().to_container();
      for(const NodeDesc v: all_nodes) {
        goodbye(v);
        Parent::delete_node(v);
      }
#endif
      Parent::clear();
      assert(edgeless());
      assert(empty());
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
          remove_node(u, std::forward<Args>(args)...);
      } else remove_subtree_except_root(u, std::forward<Args>(args)...);
    }

    // remove the subtree rooted at u, but leave u be
    template<class... Args>
    void remove_subtree_except_root(const NodeDesc u, Args&&... args) {
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(back(C), std::forward<Args>(args)...);
    }

    // re-root the phylogeny at a given node r
    // if requested, suppress deg-2 nodes on the root-r-path (including the root itself)
    // NOTE: the _no_cleanup-suffix indicates that the 'roots' set is not updated, if you want that updated, call the 'reroot' function
    // NOTE: this is only supported if we have a unique root and the path from the root to r does not contain reticulations
    //       (in this case, we will just reverse this path)
    void reroot_no_cleanup(const NodeDesc r, const bool suppress_deg2 = false) {
      if(Parent::num_roots() == 1){
        NodeDesc x = r;
        while(true) {
          const auto& x_pars = parents(x);
          if(x_pars.size() == 1) {
            const auto& x_adj = x_pars.front();
            const NodeDesc tmp = x_adj;
            reverse_edge(Edge(reverse_edge_tag(), x, x_adj));
            x = tmp;
          } else break;
        }
        auto& rt = front(_roots);
        if(x == rt) {
          rt = r;
        } else throw mstd::Unimplemented("rerooting at a descendant of a reticulation");
#warning "TODO: implement rooting strategy"
      // TODO: idea: 
      // binary strategy: "reversal network"
      //  1. reversal nerwork Nr = reverse of any root-r-path p
      //  2. for each reticulation x on p in N, reverse any path from x to any node on Nr
      //  --> if we reverse Nr in N, then we have a valid network if N was binary
      // multifurcating strategy:
      //  1. use the binary strategy
      //  2. for each node with indeg > 1 AND outdeg > 1, "pull apart" the node (turn the node into 2 nodes, one with all in-edges, and one with all out-edges)
      // TODO: add a callback that ist called for all nodes with indeg > 1 and outdeg > 1 to resolve them (or exit throwing exceptions etc)
      } else throw mstd::Unimplemented("rerooting of multi-root phylogenies");
    }


    // =============== variable query ======================
    bool empty() const { return _num_nodes == 0; }
    bool edgeless() const { return num_edges() == 0; }
    size_t num_leaves() const { return leaves().to_container().size(); } // NOTE: this is slow since we have to crawl the phylogeny

    // =============== traversals ======================
    // NOTE: when calling nodes(NodeContainer X), there is an ambiguity:
    // is X supposed to be a set of roots above/below the traversal goes or a set of forbidden nodes?
    // We will interpret as follows:
    // nodes(direction_tag, R) --> R = roots
    // nodes(X) --> X = forbidden (if you want to pass a root-set to nodes(), then give it a direction_tag as well!)
    // nodes_above(R) --> R = roots
    // nodes_below(R) --> R = roots
    //

    // --------------- relative node traversals (below) ------------------
    // list all nodes below u in order _o (default: postorder)
    template<TraversalType o = postorder, NodeOrIterableType Roots, class Forbidden> 
    static auto nodes_below(Roots&& R, Forbidden&& forbidden) {
      using RootSet = std::conditional_t<AdjacencyType<Roots>, NodeDesc, std::remove_cvref_t<Roots>>;
      if constexpr (std::is_void_v<DefaultSeen>)
        return NodeTraversal<o, Phylogeny, RootSet, Forbidden>(std::forward<Roots>(R), std::forward<Forbidden>(forbidden));
      else return NodeTraversal<o, Phylogeny, RootSet, Forbidden>(std::forward<Roots>(R), std::forward<Forbidden>(forbidden));
    }
    template<TraversalType o = postorder, NodeOrIterableType Roots> 
    static auto nodes_below(Roots&& R) {
      using RootSet = std::conditional_t<AdjacencyType<Roots>, NodeDesc, std::remove_cvref_t<Roots>>;
      if constexpr (std::is_void_v<DefaultSeen>)
        return NodeTraversal<o, Phylogeny, RootSet>(std::forward<Roots>(R));
      else return NodeTraversal<o, Phylogeny, RootSet>(std::forward<Roots>(R));
    }

    template<class... Args> static auto nodes_below_preorder(Args&&... args)  { return nodes_below<preorder>(std::forward<Args>(args)...); }
//    template<class... Args> static auto nodes_below_inorder(Args&&... args)   { return nodes_below<inorder>(std::forward<Args>(args)...); }
    template<class... Args> static auto nodes_below_postorder(Args&&... args) { return nodes_below<postorder>(std::forward<Args>(args)...); }

    // --------------- relative reverse node traversals (above) ------------------
    template<TraversalType o = preorder, class... Args>
    static auto nodes_above(Args&&... args) { return nodes_below<TraversalType(o | reverse_traversal)>(std::forward<Args>(args)...); }

    template<class... Args> static auto nodes_above_preorder(Args&&... args)  { return nodes_above<preorder>(std::forward<Args>(args)...); }
//    template<class... Args> static auto nodes_above_inorder(Args&&... args)   { return nodes_above<inorder>(std::forward<Args>(args)...); }
    template<class... Args> static auto nodes_above_postorder(Args&&... args) { return nodes_above<postorder>(std::forward<Args>(args)...); }

    // --------------- absolute node traversals (below (default) or above roots) ------------------
    template<TraversalType o = postorder, DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto nodes(const DIR_tag dt, Roots&& rt, Args&&...args) {
      if constexpr (mstd::is_convertible_v<DIR_tag, below_tag>)
        return nodes_below<o>(std::forward<Roots>(rt), std::forward<Args>(args)...);
      else return nodes_above<o>(std::forward<Roots>(rt), std::forward<Args>(args)...);
    }
    template<TraversalType o = postorder> auto nodes() const { return nodes<o>(below_tag{}, _roots); }

    // --------------- node traversals (with pred) ------------------
    // NOTE: this cannot be static since we may need to grab the _roots of the current network
    template<TraversalType o = preorder, NodePredicateType Predicate>
    auto nodes_with(Predicate&& pred) { return mstd::make_filtered_factory(nodes<o>().begin(), std::forward<Predicate>(pred)); }

    template<TraversalType o = preorder, NodePredicateType Predicate, class First, class... Args> requires (not DirectionTag<First>)
    auto nodes_with(Predicate&& pred, First&& first, Args&&... args) const {
      return mstd::make_filtered_factory(nodes<o>(std::forward<First>(first), std::forward<Args>(args)...).begin(), std::forward<Predicate>(pred));
    }
    // NOTE: however, if we are given a root-set, then we can be static
    template<TraversalType o = preorder, NodePredicateType Predicate, DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto nodes_with(Predicate&& pred, const DIR_tag dt, Roots&& rt, Args&&... args) {
      return mstd::make_filtered_factory(
          nodes<o>(dt, std::forward<Roots>(rt), std::forward<Args>(args)...).begin(), std::forward<Predicate>(pred));
    }

    template<TraversalType o = preorder, NodePredicateType Predicate, NodeOrIterableType Roots, class... Args>
    static auto nodes_with_below(Predicate&& pred, Roots&& rt, Args&&... args) {
      return nodes_with<o>(std::forward<Predicate>(pred), below_tag{}, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }

    template<TraversalType o = preorder, NodePredicateType Predicate, NodeOrIterableType Roots, class... Args>
    static auto nodes_with_above(Predicate&& pred, Roots&& rt, Args&&... args) {
      return nodes_with<o>(std::forward<Predicate>(pred), above_tag{}, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }

    // -------------- preorder -----------------
    auto nodes_preorder() const  { return nodes_below<preorder>(_roots); }
    
    template<class First, class... Args> requires (not DirectionTag<First>)
    auto nodes_preorder(First&& first, Args&&... args) const  {
      return nodes_below<preorder>(_roots, std::forward<First>(first), std::forward<Args>(args)...);
    }
    
    template<DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto nodes_preorder(const DIR_tag dt, Roots&& rt, Args&&... args) {
      return nodes<preorder>(dt, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }

    /*
    // -------------- inorder -----------------
    auto nodes_inorder() const  { return nodes_below<inorder>(_roots); }

    template<class First, class... Args> requires (not DirectionTag<First>)
    auto nodes_inorder(First&& first, Args&&... args) const {
      return nodes_below<inorder>(_roots, std::forward<First>(first), std::forward<Args>(args)...);
    }

    template<DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto nodes_inorder(const DIR_tag dt, Roots&& rt, Args&&... args) {
      return nodes<inorder>(dt, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }
    */

    // -------------- postorder -----------------
    auto nodes_postorder() const  { return nodes_below<postorder>(_roots); }

    template<class First, class... Args> requires (not DirectionTag<First>)
    auto nodes_postorder(First&& first, Args&&... args) const {
      return nodes_below<postorder>(_roots, std::forward<First>(first), std::forward<Args>(args)...);
    }
    
    template<DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto nodes_postorder(const DIR_tag dt, Roots&& rt, Args&&... args) {
      return nodes<postorder>(dt, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }

    // -------------- leaves -----------------
    template<class... Args>
    static auto leaves_below(Args&&... args) { return nodes_with_below(is_leaf, std::forward<Args>(args)...); }
    // there are no "leaves_above" ^^

    auto leaves() const  { return leaves_below(_roots); }

    template<class First, class... Args> requires (not DirectionTag<First>)
    auto leaves(First&& first, Args&&... args) const  { return leaves_below(_roots, std::forward<First>(first), std::forward<Args>(args)...); }
    
    template<DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto leaves(const DIR_tag dt, Roots&& rt, Args&&... args) {
      return nodes_with<postorder>(is_leaf, dt, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }


    // -------------- reticulations -----------------
    template<TraversalType o = preorder, class... Args>
    static auto retis_below(Args&&... args) { return nodes_with_below<o>(is_reti, std::forward<Args>(args)...); }

    template<TraversalType o = preorder, class... Args>
    static auto retis_above(Args&&... args) { return nodes_with_above<o>(is_reti, std::forward<Args>(args)...); }

    template<TraversalType o = preorder> auto retis() const  { return retis_below<o>(_roots); }

    template<TraversalType o = preorder, class First, class... Args> requires (not DirectionTag<First>)
    auto retis(First&& first, Args&&... args) const  { return retis_below<o>(_roots, std::forward<First>(first), std::forward<Args>(args)...); }

    template<TraversalType o = preorder, DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto retis(const DIR_tag dt, Roots&& rt, Args&&... args) {
      return nodes_with<o>(is_reti, dt, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }


    // --------------- relative edge traversals (below) ------------------
    template<TraversalType o = postorder, NodeOrIterableType Roots, class Forbidden>
      requires (std::is_void_v<Forbidden> or std::movable<Forbidden>)
    static auto edges_below(Roots&& R, Forbidden&& forbidden) {
      using RootSet = std::conditional_t<AdjacencyType<Roots>, NodeDesc, std::remove_cvref_t<Roots>>;
      if constexpr (std::is_void_v<DefaultSeen>)
        return AllEdgesTraversal<o, Phylogeny, RootSet, Forbidden>(std::forward<Roots>(R), std::forward<Forbidden>(forbidden));
      else return AllEdgesTraversal<o, Phylogeny, RootSet, Forbidden>(std::forward<Roots>(R), std::forward<Forbidden>(forbidden));
    }
    template<TraversalType o = postorder, NodeOrIterableType Roots>
    static auto edges_below(Roots&& R) {
      using RootSet = std::conditional_t<AdjacencyType<Roots>, NodeDesc, std::remove_cvref_t<Roots>>;
      if constexpr (std::is_void_v<DefaultSeen>)
        return AllEdgesTraversal<o, Phylogeny, RootSet>(std::forward<Roots>(R));
      else return AllEdgesTraversal<o, Phylogeny, RootSet>(std::forward<Roots>(R));
    }

    template<class... Args> static auto edges_below_preorder(Args&&... args)  { return edges_below<preorder>(std::forward<Args>(args)...); }
    //template<class... Args> static auto edges_below_inorder(Args&&... args)   { return edges_below<inorder>(std::forward<Args>(args)...); }
    template<class... Args> static auto edges_below_postorder(Args&&... args) { return edges_below<postorder>(std::forward<Args>(args)...); }

    // --------------- relative reverse edge traversals (above) ------------------
    template<TraversalType o = preorder, class... Args>
    static auto edges_above(Args&&... args) { return edges_below<TraversalType(o | reverse_traversal)>(std::forward<Args>(args)...); }
    template<class... Args> static auto edges_above_preorder(Args&&... args)  { return edges_above<preorder>(std::forward<Args>(args)...); }
    //template<class... Args> static auto edges_above_inorder(Args&&... args)   { return edges_above<inorder>(std::forward<Args>(args)...); }
    template<class... Args> static auto edges_above_postorder(Args&&... args) { return edges_above<postorder>(std::forward<Args>(args)...); }

    // --------------- absolute edge traversals (below (default) or above roots) ------------------
    template<TraversalType o = postorder, DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    auto edges(const DIR_tag dt, Roots&& rt, Args&&...args) const {
      if constexpr (mstd::is_convertible_v<DIR_tag, below_tag>)
        return edges_below<o>(std::forward<Roots>(rt), std::forward<Args>(args)...);
      else return edges_above<o>(std::forward<Roots>(rt), std::forward<Args>(args)...);
    }

    template<TraversalType o = postorder, class First, class... Args> requires (not DirectionTag<First>)
    auto edges(First&& first, Args&&... args) const { return edges_below<o>(_roots, std::forward<First>(first), std::forward<Args>(args)...); }
    
    template<TraversalType o = postorder>
    auto edges() const { return edges_below<o>(_roots); }


    template<class... Args> auto edges_preorder(Args&&... args) const  { return edges<preorder>(std::forward<Args>(args)...); }
    //template<class... Args> auto edges_inorder(Args&&... args) const   { return edges<inorder>(std::forward<Args>(args)...); }
    template<class... Args> auto edges_postorder(Args&&... args) const { return edges<postorder>(std::forward<Args>(args)...); }

    // --------------- edge traversals (with pred) ------------------
    // NOTE: this cannot be static since we may need to grab the _roots of the current network
    template<TraversalType o = preorder, EdgePredicateType<Phylogeny> Predicate>
    auto edges_with(Predicate&& pred) { return mstd::make_filtered_factory(edges<o>().begin(), std::forward<Predicate>(pred)); }

    template<TraversalType o = preorder, EdgePredicateType<Phylogeny> Predicate, class First, class... Args> requires (not DirectionTag<First>)
    auto edges_with(Predicate&& pred, First&& first, Args&&... args) const {
      return mstd::make_filtered_factory(edges<o>(std::forward<First>(first), std::forward<Args>(args)...).begin(), std::forward<Predicate>(pred));
    }
    // NOTE: however, if we are given a root-set, then we can be static
    template<TraversalType o = preorder, EdgePredicateType<Phylogeny> Predicate, DirectionTag DIR_tag, NodeOrIterableType Roots, class... Args>
    static auto edges_with(Predicate&& pred, const DIR_tag dt, Roots&& rt, Args&&... args) {
      return mstd::make_filtered_factory(
          edges<o>(dt, std::forward<Roots>(rt), std::forward<Args>(args)...).begin(), std::forward<Predicate>(pred));
    }

    template<TraversalType o = preorder, EdgePredicateType<Phylogeny> Predicate, NodeOrIterableType Roots, class... Args>
    static auto edges_with_below(Predicate&& pred, Roots&& rt, Args&&... args) {
      return edges_with<o>(std::forward<Predicate>(pred), below_tag{}, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }

    template<TraversalType o = preorder, EdgePredicateType<Phylogeny> Predicate, NodeOrIterableType Roots, class... Args>
    static auto edges_with_above(Predicate&& pred, Roots&& rt, Args&&... args) {
      return edges_with<o>(std::forward<Predicate>(pred), above_tag{}, std::forward<Roots>(rt), std::forward<Args>(args)...);
    }


  protected:
    static bool cyclic_below(const NodeDesc start, NodeSet& current_path, NodeSet& seen) {
      if(append(current_path, start).second) {
        if(append(seen, start).second)
          for(const NodeDesc u: children(start))
            if(cyclic_below(u, current_path, seen)) return true;
        erase(current_path, start);
      } else return true; // if we've reached someone on our current path, then we have found a cycle :(
      return false;
    }
  public:
    //! for sanity checks: test if there is a directed cycle in the data structure (more useful for networks, but definable for trees too)
    bool has_cycle() const {
      if(!empty()) {
        NodeSet current_path, seen;
        for(const NodeDesc r: _roots)
          if(cyclic_below(r, current_path, seen)) return true;
      }
      return false;
    }

    // return whether y and z are have a common parent, or are the same node
    static bool are_siblings(const NodeDesc y, const NodeDesc z) {
      if(y != z) {
        const Node& y_node = node_of(y);
        const Node& z_node = node_of(z);
        if((not y_node.is_root()) and (not z_node.is_root())) {
          return mstd::are_disjoint(y_node.parents(), z_node.parents());
        } else return false;
      } else return true;
    }

    // return a common parent of y and z if they have one, and NoNode, otherwise
    static NodeDesc common_parent(const NodeDesc y, const NodeDesc z) {
      const Node& y_node = node_of(y);
      if(y != z) {
        const Node& z_node = node_of(z);
      	if((not y_node.is_root()) and (not z_node.is_root())) {
          const auto iter = mstd::common_element(y_node.parents(), z_node.parents());
					return iter ? *iter : NoNode;
        } else return NoNode;
      } else return y_node.is_root() ? NoNode : y_node.any_parent();
    }

    // return all common parents of y and z in a NodeVec
    static NodeVec common_parents(const NodeDesc y, const NodeDesc z) {
      NodeVec result;
      const Node& y_node = node_of(y);
      if(y != z) {
        const Node& z_node = node_of(z);
      	if(!y_node.is_root() && !z_node.is_root()) {
          if(y_node.in_degree() > z_node.in_degree()) {
            for(const auto& u: y_node.parents())
              if(mstd::test(z_node.parents(), u))
                result.push_back(u);
          } else {
            for(const auto& u: z_node.parents())
              if(mstd::test(y_node.parents(), u))
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
    template<StrictPhylogenyType, bool, OptionalMapsToNode> friend struct EdgeEmplacementHelper;


    // increase our node-/edge- counts by the number of nodes/edges below other_x and decrease other's by the same amount
    // in short: update node- and edge counts in reaction of moving other_x into us
    template<StrictPhylogenyType Phylo> requires std::is_same_v<Node, typename Phylo::Node>
    void update_node_and_edge_numbers(Phylo& other, const NodeDesc other_x) {
      std::pair<size_t,size_t> node_and_edge_count;

      // if other_x was the only root of other, then we'll use other's node- and edge- numbers, otherwise we'll have to count them :/
      if(other.is_root(other_x) && (other.num_roots() == 1)) {
        node_and_edge_count = {other.num_nodes(), other.num_edges()};
      } else {
        node_and_edge_count = Phylo::node_of(other_x).count_nodes_and_edges_below();
      }
      count_node(node_and_edge_count.first);
      count_edge(node_and_edge_count.second);
      other.count_node(-node_and_edge_count.first);
      other.count_edge(-node_and_edge_count.second);
    }
  public:

    // move the subtree below other_x into us by changing the parents of other_x to {x}
    // NOTE: the edge x --> other_x will be initialized using "args"
    // NOTE: Phylo_ needs to have the same NodeType as we do!
    // NOTE: if keep_other_x is true, then other_x survives in the other phylogeny
    template<bool count = true, StrictPhylogenyType Phylo, class... Args> requires std::is_same_v<Node, typename Phylo::Node>
    void place_below_by_move(Phylo&& other, const NodeDesc other_x, const NodeDesc x, Args&&... args) {
      assert(other_x != NoNode);
      std::cout << "moving subtree below "<<other_x<<"...\n";
      // step 1: move other_x over to us
      auto& other_x_parents = parents(other_x);
     
      while(!other_x_parents.empty())
        other.remove_edge_no_cleanup(mstd::front(other_x_parents), other_x);

      if constexpr (count) update_node_and_edge_numbers(other, other_x);
      if(x != NoNode) {
        const bool success = add_child(x, other_x, std::forward<Args>(args)...).second;
        assert(success);
      } else add_root(other_x);
    }
    // same as above, but keep other_x in tact
    template<bool count = true, StrictPhylogenyType Phylo, class... Args> requires std::is_same_v<Node, typename Phylo::Node>
    void place_below_by_move_children(Phylo&& other, const NodeDesc other_x, const NodeDesc x, Args&&... args) {
      assert(other_x != NoNode);
      std::cout << "moving children and subtree below "<<other_x<<"...\n";

//#warning "TODO: uncomment this once bug 106046 is fixed: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106046"
      const NodeDesc target = create_node(other_x);
      //const NodeDesc target = create_node();

      if constexpr (count) update_node_and_edge_numbers(other, other_x);
      if(x != NoNode) {
        const bool success = add_child(x, target, std::forward<Args>(args)...).second;
        assert(success);
      } else add_root(target);

      // we'll slightly abuse the internal transfer function to transfer BETWEEN phylogenies
      std::cout << "transfering children "<<children(other_x)<<" below "<<target<<"...\n";
      transfer_children(other_x, target);

    }


    // move a number of nodes below a node x (or as new roots if x is NoNode)
    // NOTE: to not erase the roots from the other phylogeny, pass false as second template parameter
    template<bool count = true, bool remove_foreign_roots = true, StrictPhylogenyType Phylo, NodeIterableType RContainer, class... Args>
      requires std::is_same_v<Node, typename Phylo::Node>
    void place_below_by_move(Phylo&& other, const RContainer& in_roots, const NodeDesc x, Args&&... args) {
      assert(&in_roots != &(other._roots) && "pleaase avoid passing the root-set manually when move-constructing a phylogeny");
      for(const NodeDesc r: in_roots)
        place_below_by_move(std::move(other), r, NoNode);
      if constexpr (remove_foreign_roots) {
        mstd::erase(other._roots, in_roots);     
      }
    }


    // emplace a set of new edges into *this
    // NOTE: to extract node/edge data, pass data-extraction functions when constructing the emplacer (see <edge_emplacement.hpp> and <extract_data.hpp>)
    // NOTE: node-/edge- data is passed into the extractor functions by move if the edge-container is not passed by const reference!
    // NOTE: the emplacer_args allow you to pass your own (partial) node translation old_to_new in order to pre-define node-correspondances (powerfull!)
    //       if you do this, make sure that all values in the translation are valid node descriptors (point to constructed nodes of the correct type)
    //       see <edge_emplacement.hpp> and <extract_data.hpp> for more info on passing node-translation functions
    template<mstd::IterableType Edges, EdgeEmplacerType Emplacer>
    void build_from_edges(Edges&& edges, Emplacer&& emplacer) {
      DEBUG3(std::cout << "init Network with edges "<<edges<<"\n");
      for(auto&& e: std::forward<Edges>(edges))
        emplacer.emplace_edge(static_cast<mstd::copy_cvref_t<Edges&&, decltype(e)>>(e));
    }

  public:
    // =============== destruction ==================
    ~Phylogeny() { clear(); }

    // =============================== construction ======================================
    Phylogeny() = default;
    // if we just want to move a phylogeny, we can delegate to the move-construction of the parent since we do not have members
    // NOTE: we have to make sure to remove the other phylogeny's roots, otherwise its destructor will destruct the roots as well
    Phylogeny(Phylogeny&& N) = default;

    Phylogeny(const Phylogeny& N):
      Phylogeny(below_tag{}, policy_copy_tag(), N, N.roots())
    {}


    // initialize tree from any std::IterableType containing edges, for example, std::vector<PT::Edge<>>
    template<mstd::IterableType Edges, class... EmplacerArgs> requires (not PhylogenyType<Edges>)
    explicit Phylogeny(Edges&& edges, EmplacerArgs&&... args) {
      using GivenEdge = mstd::value_type_of_t<Edges>;
      using SourcePhyloFromEdgeData = Phylogeny<PredStorage_, SuccStorage_, void, DataOf<GivenEdge>>;
        
      build_from_edges(std::forward<Edges>(edges), EdgeEmplacers<true, SourcePhyloFromEdgeData>::make_emplacer(*this, std::forward<EmplacerArgs>(args)...));
      DEBUG3(print_summary(std::cout));
    }

    // NOTE: construction from another (sub-)phylogeny largely depends on the data policy:
    // 1. "copy" (default)      - creates new nodes, copying/moving the data from the other nodes
    //                            (move data if the other phylogeny is presented as rvalue reference)
    // 2. "move"                - simply moves the root from the given (sub-)phylogeny into *this as new root
    // NOTE: 2. requires that the given Phylogeny has the same NodeType as us (but may allow more/less roots)


    // -------------------------- construction with explicit tag --------------------------------
    // "copy" construction with root(s)
    template<DirectionTag DIR_tag, PhylogenyType Phylo, NodeIterableType Nodes, class... EmplacerArgs>
    Phylogeny(const DIR_tag dt, const policy_copy_tag, Phylo&& N, const Nodes& some_nodes, EmplacerArgs&&... args) {
      static constexpr bool below = std::is_convertible_v<DIR_tag, below_tag>;
      DEBUG3(std::cout << "copy constructing phylogeny with "<<N.num_nodes()<< " nodes, "<<N.num_edges()<<" edges using roots "<<some_nodes<<"\n");
      if(!some_nodes.empty()) {
        // NOTE: if we're listing edges below, there is no need to track roots (we know them)
        auto emplacer = EdgeEmplacers<not below, Phylo&&>::make_emplacer(*this, std::forward<EmplacerArgs>(args)...);
        if(some_nodes.size() == 1) {
          build_from_edges(N.edges_preorder(dt, mstd::front(some_nodes)), emplacer);
        } else {
          build_from_edges(N.edges_preorder(dt, some_nodes), emplacer);
        }
        // mark the roots if we didn't track them
        if constexpr (below) {
          DEBUG4(std::cout << "marking roots: "<<some_nodes<<"\n");
          emplacer.mark_roots(some_nodes);
        }
        DEBUG2(print_summary(std::cout));
      }
    }
    // "copy" construction without root (using all roots of N)
    template<DirectionTag DIR_tag, PhylogenyType Phylo, class First, class... Args>
      requires (not NodeIterableType<First> and
          not mstd::is_same_v<First, NodeDesc> and
          not std::remove_reference_t<Phylo>::has_unique_root)
    Phylogeny(const DIR_tag dt, const policy_copy_tag pct, Phylo&& N, First&& first, Args&&... args):
      Phylogeny(dt, pct, std::forward<Phylo>(N), N.roots(), std::forward<First>(first), std::forward<Args>(args)...)
    {}

    template<PhylogenyType Phylo, class... Args>
    Phylogeny(const policy_copy_tag, Phylo&& N, Args&&... args):
      Phylogeny(below_tag{}, policy_copy_tag{}, std::forward<Phylo>(N), std::forward<Args>(args)...)
    {}



    // "move" construction with root(s) by unlinking the in_roots from their in_tree and using them as our new _roots
    // NOTE: this requires the other phylogeny to have the same node type as we do; if that's not the case, copying cannot be avoided
    // NOTE: This will go horribly wrong if the sub-network below in_root has incoming arcs from outside the subnetwork!
    //       It is the user's responsibility to make sure this is not the case.
    template<StrictPhylogenyType Phylo, NodeIterableType RContainer> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_tag, Phylo&& in_tree, const RContainer& in_roots) noexcept {
      place_below_by_move<true, true>(std::move(in_tree), in_roots, NoNode);
    }
    // "move" construction with single root
    template<StrictPhylogenyType Phylo, NodeIterableType RContainer> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_tag, Phylo&& in_tree, const NodeDesc in_root) noexcept {
      place_below_by_move(std::move(in_tree), in_root, NoNode);
      // remember to manually remove the in_root from in_tree's root storage
      mstd::erase(in_tree._roots, in_root);
    }
    // "move" construction making a copy of in_root instead of moving it
    template<StrictPhylogenyType Phylo> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_children_tag, Phylo&& in_tree, const NodeDesc in_root) noexcept {
      place_below_by_move_children(std::move(in_tree), in_root, NoNode);
    }
    template<StrictPhylogenyType Phylo, NodeIterableType RContainer> requires std::is_same_v<Node, typename Phylo::Node>
    Phylogeny(const policy_move_children_tag, Phylo&& in_tree, const RContainer& in_roots) noexcept {
      if(in_roots.size() == 1) {
        place_below_by_move_children(std::move(in_tree), mstd::front(in_roots), NoNode);
      } else throw mstd::Unimplemented("move-construction of phylogenies with different root containers");
#warning "TODO: write me"
    }



    // -------------------------- construction without tag delegates to tagged-construction --------------------------------
    template<StrictPhylogenyType Phylo, class... Args> requires (not std::is_same_v<Phylo, Phylogeny>)
    explicit Phylogeny(const Phylo& N, Args&&... args):
      Phylogeny(below_tag{}, policy_copy_tag(), N, N.roots(), std::forward<Args>(args)...)
    {}
    // copy-constructing for the same type is not explicit
    template<class... Args> requires (sizeof...(Args) > 0)
    Phylogeny(const Phylogeny& N, Args&&... args):
      Phylogeny(below_tag{}, policy_copy_tag(), N, N.roots(), std::forward<Args>(args)...)
    {}

    template<StrictPhylogenyType Phylo, class... Args>
      requires (not std::is_same_v<Phylo, Phylogeny> and std::is_same_v<typename Phylo::Node, Node>)
    explicit Phylogeny(Phylo&& N, Args&&... args) noexcept:
      Phylogeny(policy_move_tag(), std::move(N), std::move(N).roots(), std::forward<Args>(args)...)
    {}

    // -------------------------- construction of subnetwork above a set of nodes --------------------------------
    //template<StrictPhylogenyType Phylo, NodeIterableType Leaves>
    //Phylogeny(Phylo&& N, Leaves&& leaves) { TODO   }

    // =================== assignment ======================

    // assigning from a Phylogeny-rval-ref is just stealing their stuff - the ProtoPhylogeny knows how to do this
    void assign_from(Phylogeny&& other) noexcept {
      if(!std::empty(_roots)) clear();
      Parent::operator=(std::move(other));
      assert(std::empty(other.roots())); // make sure other has no more roots now cause they would be free'd
    }

    // an assignment operator to which you can pass more arguments
    // NOTE: passing stuff is useful for providing your own DataExtractor or NodeTranslation
    template<PhylogenyType Phylo, class... Args>
    void assign_from(Phylo&& target, Args&&... args) const noexcept {
      Phylogeny tmp(std::forward<Phylo>(*this), std::forward<Args>(args)...);
      assign_from(std::move(tmp));
    }

    // to swap with another (possibly different) phylogeny, we let tmp steal their stuff, then let them steal our stuff and finally steal tmp's stuff
    // NOTE: you can pass data extractor related stuff as arguments - they will be forwarded to the constructor of tmp from other
    template<PhylogenyType Phylo, class... Args>
    void swap(Phylo&& other, Args&&... args) noexcept {
      Phylogeny tmp(std::move(other), std::forward<Args>(args)...);
      // NOTE: if other is an rvalue reference, there is really no need to restore anything useful to it
      if constexpr (not std::is_rvalue_reference_v<Phylo&&>) {
        other = std::move(*this);
        assert(_roots.empty()); // be sure all roots have been stolen!
      }
      assign_from(std::move(tmp));
    }

    // assignment by copy-and-swap
    template<PhylogenyType Phylo>
    Phylogeny& operator=(Phylo&& other) noexcept {
      Phylogeny tmp(std::forward<Phylo>(other));
      assign_from(std::move(tmp));
      return *this;
    }



    // =================== i/o ======================

    std::ostream& print_summary(std::ostream& os) const {
      DEBUG3(os << "network has "<< num_edges() <<" edges, "<< _num_nodes <<" nodes, "<<num_roots()<<" roots\n");
      DEBUG3(os << "leaves: "<<leaves()<<"\n");
      DEBUG3(os << Parent::num_nodes() << " nodes: "<<nodes()<<'\n');
      DEBUG3(os << Parent::num_edges() << " edges: "<<edges()<<'\n');
      for(const NodeDesc u: nodes())
        os << u << ":" << "\tIN: "<< in_edges(u) << "\tOUT: "<< out_edges(u) << '\n';
      return os << "End Summary\n";
    }

    template<class NodeDataToString = mstd::IgnoreFunction<std::string>>
    static void print_subtree(std::ostream& os,
                              const NodeDesc u,
                              std::string prefix,
                              auto& seen,
                              NodeDataToString&& node_data_to_string = NodeDataToString()) 
    {
      const Node& u_node = PT::node_of<Phylogeny>(u);
      const bool u_reti = u_node.is_reti();

      std::string u_name = mstd::config::locale.char_no_branch_hori;
      const size_t old_len = u_name.size();
      u_name += std::to_string(name(u));

      if constexpr (Node::has_label){
        const std::string u_label = std::to_string(u_node.label());
        if(!u_label.empty())
          u_name += '[' + u_label + ']';
      }
      if constexpr (Node::has_data){
        const std::string u_data = node_data_to_string(u_node.data());
        if(!u_data.empty())
          u_name += '(' + u_data + ')';
      }
      if(u_name.size() == old_len) {
        if(u_reti)
          u_name += std::string('(' + std::to_string(u) + ')');
        else if(out_degree(u) > 1) u_name += mstd::config::locale.char_branch_low;
      }
      if(u_reti) u_name += mstd::config::locale.char_reti;
      os << u_name;
      
      bool u_seen = true;
      if(!u_reti || !(u_seen = mstd::test(seen, u))) {
        const auto& u_childs = children(u);
        if(u_reti) mstd::append(seen, u);
        switch(u_childs.size()){
          case 0:
            os << '\n';
            break;
          case 1:
            prefix += std::string(utf8_len(u_name), ' ');
            print_subtree(os, mstd::front(children(u)), prefix, seen, node_data_to_string);
            break;
          default:
            prefix += std::string(utf8_len(u_name) - 1, ' ');

            uint32_t count = u_childs.size();
            for(const NodeDesc c: u_childs){
              const char* last_char = (count >= 2) ? mstd::config::locale.char_no_branch_vert : " ";
              print_subtree(os, c, prefix + last_char, seen, node_data_to_string);
              if(count >= 3) last_char = mstd::config::locale.char_branch_right;
              if(count == 2) last_char = mstd::config::locale.char_last_child;
              if(--count > 0) os << prefix << last_char;
            }
        }
      } else os << '\n';
    }

    template<class NodeDataToString = mstd::IgnoreFunction<std::string>>
    static void print_subtree(std::ostream& os, const NodeDesc u, NodeDataToString&& node_data_to_string = NodeDataToString()) {
      NodeSet tmp;
      print_subtree(os, u, "", tmp, std::forward<NodeDataToString>(node_data_to_string));
    }
    
    template<class NodeDataToString = mstd::IgnoreFunction<std::string>>
    void print_subtree(std::ostream& os, NodeDataToString&& node_data_to_string = NodeDataToString()) const {
      if(!_roots.empty()) {
        print_subtree(os, mstd::front(_roots), std::forward<NodeDataToString>(node_data_to_string));
      } else os << "(empty)\n";
    }

    void print_subtree_with_data(std::ostream& os = std::cout) const {
      if(!_roots.empty()) {
        if constexpr (Parent::has_node_data) {
          print_subtree(os, mstd::front(_roots), [](const auto& x){ return std::to_string(x); });
        } else print_subtree(os, mstd::front(_roots));
      } else os << "(empty)\n";
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

  // ------- Phylogeny: factories ---------
  
  // ------- Phylogeny: concepts ---------
  
  // ------- Phylogeny: deduction guides ---------
  
  // ------- Phylogeny: defaults ---------



  template<StrictPhylogenyType Phylo_>
  std::ostream& operator<<(std::ostream& os, const Phylo_& T) {
    if(!T.empty()) {
      T.print_subtree(os);
      return os;
    } else return os << "{}";
  }


  enum { DISPLAY_DATA = 1, DISPLAY_NEWICK = 2 };

#warning "TODO: turning this into a lambda crashes gcc up to (excluding) version 12"
  struct my_to_string {
    auto operator()(const auto& x) { return std::to_string(x); }
  };

  template<int flags>
  using DefaultDataToString = std::conditional_t<flags & DISPLAY_DATA,
                                                 my_to_string,
                                                 mstd::IgnoreFunction<std::string>>;

  template<int flags = DISPLAY_DATA, class NodeDataToString = DefaultDataToString<flags>, StrictPhylogenyType Phylo>
  std::string ExtendedDisplay(const Phylo& N, NodeDataToString nd_to_string = NodeDataToString()) {
    if(!N.empty()) {
      std::ostringstream out;
      N.print_subtree(out, nd_to_string);
      if constexpr (flags & DISPLAY_NEWICK)
        out << '\n' << get_extended_newick(N) << '\n';
      return std::move(out).str();
    } else return "{};";
  };


  template<StrictPhylogenyType Net>
  struct DefaultDegrees {
    constexpr Degree operator()(const NodeDesc, const NodeDesc) const { return 1; }
    Degrees operator()(const NodeDesc u) const { return Net::degrees(u); }
  };

}
