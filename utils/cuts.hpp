
#pragma once

#include "types.hpp"
#include "set_interface.hpp"

// this is my own bridge finder
// NOTE: most bridge finders out there work only on undirected graphs, so I modified Tarjan's ideas slightly...
// NOTE: we'll find so-called "vertical cut nodes", that is, nodes u that have a child v such that all descendants of v only have neighbors below u
// NOTE: to this end, each node v communicates the smallest and highest preorder number of any of its neighbors to its parent u in the preorder spanning tree

namespace PT{

  enum CutObject { CO_vertical_cut_node = 0, CO_bridge = 1, CO_BCC = 2 };

  struct CutInfo {
    using IntervalAndNode = std::pair<std::linear_interval<>, NodeDesc>;

    uint32_t DFS_descendants; // number of non-strict (!) descendants in the DFS subtree (always >= 1)
    uint32_t disc_time;
    std::linear_interval<> neighbors; // smallest and highest preprder number of any neighbor of any non-strict descendant
    bool up_to_date = false; // TODO: find a better way to track bottom-up updates!

    CutInfo(const uint32_t _disc_time):
      DFS_descendants(1), disc_time(_disc_time), neighbors(_disc_time)
    {
      DEBUG5(std::cout << "CUT: making new info entry: " << *this << "\n");
    }
 
    // the DFS interval of v is [disc_time : disc_time + DFS_descendants]
    std::linear_interval<> get_DFS_interval() const { return { disc_time, disc_time + DFS_descendants - 1 }; }
    uint32_t first_outside_subtree() const { return disc_time + DFS_descendants; }
    void update_lowest_neighbor(const uint32_t u) { neighbors.update_lo(u); }
    void update_highest_neighbor(const uint32_t u) { neighbors.update_hi(u); }
    void update_neighbors(const uint32_t x) { neighbors.update(x); }
    void update_from_child(const CutInfo& other) { neighbors.merge(other.neighbors); }

    // return true if the given CutInfo has a descendant that "sees" someone outside our DFS-subtree
    bool child_has_outside_neighbor(const std::linear_interval<>& child_neighbors) const {
      return (child_neighbors[0] < disc_time) || (child_neighbors[1] >= first_outside_subtree());
    }
    bool child_has_outside_neighbor(const CutInfo& child) const {
      return child_has_outside_neighbor(child.neighbors);
    }

    // return true if we ourselves have a descendant that "sees" someone outside our DFS-subtree
    bool has_outside_neighbor() const {
      DEBUG5(std::cout << "CUT: infos: "<<*this << "\n");
      return child_has_outside_neighbor(*this);
    }

    template<PhylogenyType Network, class... Args>
    bool compute_mark(Args&&... args) const { return get_mark<Network>(); }

    bool get_mark() const { return !has_outside_neighbor(); }

    // NOTE: to decide if a node u is a cut-node, we will have to consider the neighbors seen in the DFS-subtrees of the children of u (in the DFS-tree):
    //       if the DFS-subtree of child x has a neighbor outside the DFS-subtree of u, then u does not cut x from the rest
    //       if the DFS-subtree of child y has a neighbor in the DFS-subtree of x, then u also does not cut y
    //       Thus, we have to 'merge' children of u using a union-find data structure in order to decide if u is a cut-node or not
    template<NodeType Node>
    static constexpr void for_each_cut_children(const Node& u_node, const auto& node_infos, auto&& child_cut_callback) {
      const NodeDesc u = u_node.get_desc();
      const auto& u_info = node_infos.at(u);
      const auto& u_children = u_node.children();
      // our main data-structure is a vector of nodes with their neighbor interval; we will sort this by interval and then progressively merge
      std::vector<IntervalAndNode> tmp;
      tmp.reserve(u_children.size());
      std::DisjointSetForest<NodeDesc> child_partition;
      // step 2.1: add all intervals and nodes to tmp - NOTE: keep in mind that DFS-intervals are DISJOINT!
      for(const NodeDesc v: u_children) {
        tmp.emplace_back(node_infos.at(v).get_DFS_interval(), v);
        child_partition.add_new_set(v);
      }
      assert(!tmp.empty()); // the following steps fail if tmp is empty, but leaves will never be cut-nodes so that's OK
      // step 2.2: sort by interval (first by start, then by end)
      std::sort(tmp.begin(), tmp.end(), std::greater<IntervalAndNode>());
      DEBUG4(std::cout << "\tnode "<<u<<" with info "<<u_info<<"\n");
      DEBUG4(std::cout << "\tsorted children "<<tmp<<"\n");
      
      // step 2.3: now, "merge" each node v whose DFS subtree has a neighbor in the DFS subtree of another node x into that node x
      for(auto& [interval, v]: tmp) {
        const auto& v_info = node_infos.at(v);
        // step 2.3.1: find the nodes x & y whose DFS interval includes the smallest & highest neighbor of v's DFS subtree, respectively
        for(const auto i: v_info.neighbors) {
          const auto i_iter = std::partition_point(tmp.begin(), tmp.end(), [i](const auto& ele){ return ele.first.low() > i; });
          if((i_iter != tmp.end()) && i_iter->first.contains(i))
            child_partition.merge_sets_of(i_iter->second, v);
        }
      }
      // step 2.4: remove from tmp all nodes v s.t. either
      // (a) v's DFS tree has a neighbor outside u's DFS tree or
      // (b) v has been merged into another node in 2.3
      for(const auto& [interval, v]: tmp)
        if(!u_info.child_has_outside_neighbor(node_infos.at(v)) && child_partition.is_root(v))
          if(child_cut_callback(v)) return;
    }

    friend std::ostream& operator<<(std::ostream& os, const CutInfo& b) {
      return os << "(disc: "<<b.disc_time<<" desc: "<<b.DFS_descendants<<" NH: "<<b.neighbors<<')';
    }
  };

  struct CutNodeInfo: public CutInfo {
    using CutInfo::CutInfo;

    char cut_node_mark = -1;

    template<NodeType Node>
    bool compute_mark(const Node& u_node, const auto& node_infos) {
      assert(cut_node_mark == -1);
      if(!u_node.is_leaf()) {
        if(u_node.is_root()) { // if u's in-degree is 0, we need to see at least 2 cut-node children for u to be a cut-node
          CutInfo::for_each_cut_children(u_node, node_infos, [&](const NodeDesc v){ ++cut_node_mark; return cut_node_mark; });
        } else {
          CutInfo::for_each_cut_children(u_node, node_infos, [&](const NodeDesc v){ return (cut_node_mark = 1); });
        }
        if(cut_node_mark == -1) cut_node_mark = 0;
      } else cut_node_mark = 0;
      return cut_node_mark;
    }

    bool get_mark() const { assert(cut_node_mark != -1); return cut_node_mark; }

    friend std::ostream& operator<<(std::ostream& os, const CutNodeInfo& b) {
      return os << "(disc: "<<b.disc_time<<" desc: "<<b.DFS_descendants<<" NH: "<<b.neighbors<< (b.cut_node_mark ? " *)" : ")");
    }
  };

  struct BCCInfo: public CutNodeInfo {
    using Parent = CutNodeInfo;
    using Parent::Parent;
    using Parent::cut_node_mark;

    NodeVec cut_children;

    template<NodeType Node>
    bool compute_mark(const Node& u_node, const auto& node_infos) {
      if(!u_node.is_leaf()) {
        // use the callback to put all cut-children of u into our cut_children vector
        CutInfo::for_each_cut_children(u_node, node_infos, [&](const NodeDesc v){ append(cut_children, v); return false; });
        cut_node_mark = !cut_children.empty();
      } else cut_node_mark = 0;
      return cut_node_mark;
    }

    friend std::ostream& operator<<(std::ostream& os, const BCCInfo& b) {
      return os << "(disc: "<<b.disc_time<<" desc: "<<b.DFS_descendants<<" NH: "<<b.neighbors<< " cut-children: "<<b.cut_children<<")";
    }
  };

  template<CutObject i> struct _choose_info_struct {};
  template<> struct _choose_info_struct<CO_bridge> { using type = CutInfo; };
  template<> struct _choose_info_struct<CO_vertical_cut_node> { using type = CutNodeInfo; };
  template<> struct _choose_info_struct<CO_BCC> { using type = BCCInfo; };
  template<CutObject i> using choose_info_struct = typename _choose_info_struct<i>::type;


  // this is a base for iterators listing vertical cut-nodes or -edges (aka bridges)
  template<PhylogenyType Network, CutObject cut_object, std::HasIterTraits _Traits>
  class CutIter: public _Traits {
  protected:
    using Traits = _Traits;
    using InfoStruct = choose_info_struct<cut_object>;
    using InfoMap = NodeMap<InfoStruct>;
    using NodeTraversal = Traversal<postorder, Network, NodeDesc>;
    using EdgeTraversal = Traversal<all_edge_tail_postorder, Network, NodeDesc>;
    using POTraversal = std::conditional_t<cut_object == CO_bridge, EdgeTraversal, NodeTraversal>;
    using DFSIter = typename POTraversal::OwningIter;

    NodeDesc root;
    InfoMap node_infos;
    DFSIter iter;

    // the first DFS sets up preorder numbers and numbers of descendants in the node_infos
    // NOTE: we use node_infos to indicate which nodes we have already visited
    std::emplace_result<InfoMap> setup_DFS(const NodeDesc u, uint32_t& time) {
      const auto emp_res = node_infos.emplace(u, time);
      if(emp_res.second){
        ++time;
        auto& u_info = emp_res.first->second;
        for(const NodeDesc v: node_of<Network>(u).children()) {
          const auto [v_iter, success] = setup_DFS(v, time);
          auto& v_info = v_iter->second;
          DEBUG5(std::cout << "CUT: first DFS for edge "<<u<<" "<<u_info<<" -----> "<<v<<" "<<v_info<<"\n");
          if(!success) {
            u_info.update_lowest_neighbor(v_info.disc_time);
            v_info.update_neighbors(u_info.disc_time);
            DEBUG5(std::cout << "CUT: 1st: updated infos for "<< v <<": "<<v_info<<'\n');
          } else u_info.DFS_descendants += v_info.DFS_descendants;
          DEBUG5(std::cout << "CUT: 1st: updated infos for "<< u <<": "<<u_info<<'\n');
        }
      }
      return emp_res;
    }

    // the second DFS updates the lowest and highest seen neighbors upwards
    InfoStruct& second_DFS(const NodeDesc u, const size_t time_limit = 0) {
      InfoStruct& u_info = node_infos.at(u);
      if(u_info.disc_time >=  time_limit) {
        for(const NodeDesc v: children_of<Network>(u)) {
          const auto& v_info = second_DFS(v, u_info.disc_time);
          u_info.update_from_child(v_info);
          DEBUG5(std::cout << "CUT: 2nd: updated infos for "<< u <<": "<<u_info<<'\n');
        }
      }
      return u_info;
    }

    // return true if we want to yield iter->second as next cut node (or iter has reached the end)
    bool is_yieldable() {
      if(iter.is_valid()){
        const auto& u = *iter;
        if constexpr (cut_object != CO_bridge) node_infos.at(u).compute_mark(node_of<Network>(u), node_infos);
        return operator()(u);
      } else return true;
    }

    // the second DFS propagates the lowest and highest neighbors upwards in the DFS tree
    void advance() {
      if(iter.is_valid()) {
        do { ++iter; } while(!is_yieldable());
      }
    }



  public:
    CutIter(const NodeDesc _root):
      root(_root), iter(POTraversal(_root).begin())
    {
      if((_root != NoNode) && iter.is_valid()) {
        uint32_t time = 0;
        setup_DFS(root, time);
        second_DFS(root);
        // the first node of iter is a leaf (iter is postorder) which is never a cut-node, so we can savely advance() to the first cut-node
        if(!is_yieldable()) advance();
      }
    }
    CutIter(const Network& N): CutIter(N.root()) {}

    CutIter& operator++() { advance(); return *this; }
    CutIter  operator++(int) { CutIter result = *this; ++(*this); return result; }

    CutIter(const CutIter&) = default;
    CutIter& operator=(const CutIter&) = default;
    CutIter(CutIter&&) = default;
    CutIter& operator=(CutIter&&) = default;

    bool operator==(const CutIter& other) const { return iter == other.iter; }

    template<class Iter = DFSIter, class Ref = typename Iter::reference>
    Ref operator*() const { return *iter; }
    typename DFSIter::pointer operator->() const { return operator*(); }

    bool operator==(const std::GenericEndIterator&) const { return !is_valid(); }
    bool is_valid() const { return iter.is_valid(); }

    // NOTE: this can be used as predicate, deciding whether a node is a vertical cut-node or an edge is a bridge
    // NOTE: if you want to use this as a Node predicate, be sure to pass over all nodes BEFOREHAND in order to fill the cache!
    bool is_cut_node(const NodeDesc u) const {
      return node_infos.at(u).get_mark();
    }
    bool operator()(const NodeDesc u) const { return is_cut_node(u); }
    template<EdgeType E> bool is_bridge(const E& uv) const { return node_infos.at(uv.head()).get_mark(); }
    template<EdgeType E> bool operator()(const E& uv) const { return is_bridge(uv); }
  };


  template<PhylogenyType Network>
  using CutNodeIter = CutIter<Network, CO_vertical_cut_node, std::iter_traits_from_reference<NodeDesc>>;

  template<PhylogenyType Network>
  using BridgeIter = CutIter<Network, CO_bridge, std::my_iterator_traits<Traversal<all_edge_tail_postorder, Network, NodeDesc>>>;

  template<StrictPhylogenyType Network> using CutNodeIterFactory = std::IterFactory<CutNodeIter<Network>>;
  template<StrictPhylogenyType Network> using BridgeIterFactory = std::IterFactory<BridgeIter<Network>>;

  template<StrictPhylogenyType Network> CutNodeIterFactory<Network> get_cut_nodes(const NodeDesc rt) { return rt; }
  template<StrictPhylogenyType Network> BridgeIterFactory<Network> get_bridges(const NodeDesc rt) { return rt; }
  template<StrictPhylogenyType Network> CutNodeIterFactory<Network> get_cut_nodes(const Network& N) { return N.root(); }
  template<StrictPhylogenyType Network> BridgeIterFactory<Network> get_bridges(const Network& N) { return N.root(); }

}// namespace
