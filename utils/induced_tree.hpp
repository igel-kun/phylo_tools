#pragma once

/* compute the smallest subtree spanning a list L of nodes in O(|L|) LCA-queries, see Cole et al., SIAM J. Comput., 1996 */

#include <memory>
#include <vector>
#include "utils.hpp"

namespace PT{

  struct SparseInducedSubtreeInfo
  {
    size_t dist_to_root;

    SparseInducedSubtreeInfo(const size_t _dtr = 0, const size_t _ignore = 0):
      dist_to_root(_dtr)
    {}
  };
  template<class Tree>
  using SparseInducedSubtreeInfoMap = typename Tree::template NodeMap<SparseInducedSubtreeInfo>;

  // for each node, we'll need its number in any ordering (pre-/in-/post-)order, as well as its distance to the root
  struct InducedSubtreeInfo: public SparseInducedSubtreeInfo
  {
    size_t order_number;
    
    InducedSubtreeInfo(const size_t _dtr = 0, const size_t _on = 0):
      SparseInducedSubtreeInfo(_dtr), order_number(_on)
    {}
    
    friend std::ostream& operator<<(std::ostream& os, const InducedSubtreeInfo& si)
    { return os << "{order#: "<<si.order_number<<", dist: "<<si.dist_to_root<<"}"; }
  };
  template<class Tree>
  using InducedSubtreeInfoMap = typename Tree::template NodeMap<InducedSubtreeInfo>;


  // compute necessary information from a tree
  template<class Tree, class NodeInfoMap = InducedSubtreeInfoMap<Tree>>
  void get_induced_subtree_infos(const Tree& t, NodeInfoMap& node_infos)
  {
    size_t counter = 0;
    // get a preorder because we need to access the distance to the root of the parent
    const auto preorder_dfs = t.dfs().preorder(); // const since our preorder doesn't need to track seen nodes (we're in a tree)
    auto iter = preorder_dfs.begin(); // this is the root now
    node_infos.try_emplace(*iter, 0, 0); // order number 0, distance to root 0
    for(++iter;iter.is_valid(); ++iter){
      const Node u = *iter;
      node_infos.try_emplace(u, node_infos.at(t.parent(u)).dist_to_root + 1, ++counter);
    }
  }

  template<class Tree, class NodeInfoMap>
  void make_node_infos(const Tree& supertree, std::shared_ptr<NodeInfoMap>& node_infos)
  {
    if(!node_infos) node_infos = std::make_shared<NodeInfoMap>();
    if(node_infos->empty()) get_induced_subtree_infos(supertree, *node_infos);
  }


  //NOTE: we'll need to know for each node u its distance to the root, and we'd also like L to be in some order (any of pre-/in-/post- will do)
  //      so if the caller doesn't provide them, we'll compute them from the given supertree
  //NOTE: if the infos are provided, then this runs in O(|L| * LCA-query in supertree), otherwise, an O(|supertree|) DFS is prepended
  //NOTE: when passed as a const container, the nodes are assumed to be in order; ATTENTION: this is not verified!
  template<class Tree, class LeafList, class NodeInfoMap>
  class _induced_subtree_edges {
  protected:
    using EdgeVec = std::vector<PT::Edge<>>;
    using NodeWithDepth = std::pair<Node,size_t>;

    const Tree& supertree;
    const std::shared_ptr<NodeInfoMap> node_infos;
    const std::shared_ptr<const LeafList> leaves_sorted;
    std::vector<NodeWithDepth> inner_nodes;
    std::vector<ssize_t> v_left_idx, v_right_idx;

  public:

    _induced_subtree_edges(const Tree& _supertree,
        std::shared_ptr<const LeafList> _leaves_sorted,
        std::shared_ptr<NodeInfoMap> _node_infos):
      supertree(_supertree),
      node_infos(std::move(_node_infos)),
      leaves_sorted(std::move(_leaves_sorted))
    { prepare_nodes(); }


    EdgeVec get_edges()
    {
      EdgeVec result;
      if(!inner_nodes.empty()){
        assert(leaves_sorted->size() > 1);
        result.reserve(leaves_sorted->size() * 2 - 2);
        // step 1: get edges incoming to leaves
        size_t i = 0;
        auto leaf_iter = leaves_sorted->begin();
        append_unless_equal(result, front(inner_nodes).first, *leaf_iter);
        for(++leaf_iter; i + 1 != inner_nodes.size(); ++leaf_iter, ++i){
          const Node parent = inner_nodes[choose_parent(i, i + 1)].first;
          // if the input contains a "leaf" that is ancestor of another, we may see parent == *leaf_iter here, so don't add those edgees
          append_unless_equal(result, parent, *leaf_iter);
        }
        append_unless_equal(result, inner_nodes.back().first, *leaf_iter);
        // step 2: get edges incoming to inner nodes
        Node last_node = NoNode;
        for(i = 0; i != inner_nodes.size(); ++i) if(last_node != inner_nodes[i].first){
          const ssize_t parent_idx = choose_parent(v_left_idx[i], v_right_idx[i]);
          // for nodes of high-degree, we can see parent == u here, so don't add those edges
          if(parent_idx != -1)
            append_unless_equal(result, inner_nodes[parent_idx].first, last_node = inner_nodes[i].first);
        }
      }
      std::cout << "returning edgelist "<<result<<"\n";
      return result;
    }


  protected:
    inline void append_unless_equal(EdgeVec& ev, const Node u, const Node v) { if(u != v) append(ev, u, v); }
    // choose the right parent between a choice of two indices in inner_nodes (which may be -1)
    ssize_t choose_parent(const ssize_t u_idx, const ssize_t v_idx) const
    {
      if(u_idx == -1) return v_idx;
      if(v_idx == -1) return u_idx;
      return (inner_nodes[u_idx].second < inner_nodes[v_idx].second) ? v_idx : u_idx;
    }

    void compute_inner_nodes()
    {
      inner_nodes.reserve(leaves_sorted->size() - 1);
      for(auto iter = leaves_sorted->begin(); iter != leaves_sorted->end();){
        const Node u = *iter;
        if(++iter == leaves_sorted->end()) break;
        const Node l = supertree.LCA(u, *iter);
        std::cout << "adding LCA("<<u<<", "<<*iter<<") = "<<l<<" as inner node with dist "<<node_infos->at(l).dist_to_root<<"\n";
        append(inner_nodes, l, node_infos->at(l).dist_to_root);
      }
    }

    template<bool direction> // forward = true, backward = false
    void compute_nearest_above(std::vector<ssize_t>& result) const
    {
      result.resize(inner_nodes.size());
      // keep indices of inner_nodes along with their depths on a stack
      std::stack<std::pair<size_t, size_t>> depth_stack;
      for(size_t i = 0; i != inner_nodes.size(); ++i){
        const size_t index = direction ? i : inner_nodes.size() - i - 1;
        const size_t u_depth = inner_nodes[index].second;
        while(!depth_stack.empty() && (depth_stack.top().second >= u_depth)) depth_stack.pop();
        result[index] = (depth_stack.empty()) ? -1 : depth_stack.top().first;
        depth_stack.emplace(index, u_depth);
      }
    }

    void prepare_nodes()
    {
      if(leaves_sorted->size() > 1){
        std::cout << "leaves:\t"<<*leaves_sorted<<"\n";
        // step 2: compute stepwise LCAs
        compute_inner_nodes();
        std::cout << "inner nodes:\t"<<inner_nodes<<"\n";
        // step 3: compute v_left and v_right for each internal node v (v_x = the closest node on the x of v whose dist to root is strictly smaller than v's)
        compute_nearest_above<1>(v_left_idx);
        compute_nearest_above<0>(v_right_idx);
      }
    }

  };

  template<class LeafList, class NodeInfoMap>
  void sort_by_order_number(LeafList&& _leaves, const NodeInfoMap& node_infos)
  {
    auto sort_by_order = [&](const auto& a, const auto& b) -> bool {  return node_infos.at(a).order_number > node_infos.at(b).order_number; };
    // move the given leaflist into a sortable range & sort
    std::flexible_sort(_leaves.begin(), _leaves.end(), sort_by_order);
  }


  // the following functions manage the, frankly, complicated construction variants for the induced-suptree computation class
  // NOTE: in all variants, the caller can give us an empty shared_ptr to node_infos and we'll fill it

  // policy_noop - assume the leaves are already sorted
  //NOTE: most efficient, but the caller has to make sure that they really are sorted (this is not verified by the algorithm)
  //NOTE: this also permits the user to instanciate the whole class with a sparse NodeInfoMap (does not store order_numbers)
  template<class Tree, class LeafList, class NodeInfoMap = SparseInducedSubtreeInfoMap<Tree>>
  EdgeVec get_induced_edges(const policy_noop_t, const Tree& supertree, const LeafList& leaves, std::shared_ptr<NodeInfoMap> node_infos = {})
  {
    make_node_infos(supertree, node_infos);
    return _induced_subtree_edges<Tree, LeafList, NodeInfoMap>
      (supertree, std::shared_ptr<const LeafList>(&leaves, std::NoDeleter()), std::move(node_infos)).get_edges();
  }

  // policy_copy - make a copy of the leaves (into a vector) and sort them
  //NOTE: slowest version, but the caller might not have a choice (f.ex. if only an assorted const container is available)
  template<class Tree, class LeafList, class NodeInfoMap = InducedSubtreeInfoMap<Tree>>
  EdgeVec get_induced_edges(const policy_copy_t, const Tree& supertree, const LeafList& leaves, std::shared_ptr<NodeInfoMap> node_infos = {})
  {
    NodeVec leaves_local; // we could call the range constructor, but it would have no idea of the target size; it's more efficient to reserve first
    leaves_local.reserve(leaves.size());
    leaves_local.insert(leaves_local.end(), leaves.begin(), leaves.end());
    return get_induced_edges(policy_inplace, supertree, leaves_local, std::move(node_infos));
  }

  // policy_inplace - sort the leaves in-place
  //NOTE: needs write access to the leaf-container
  //NOTE: on some containers, sort is more efficient than on others...
  template<class Tree, class LeafList, class NodeInfoMap = InducedSubtreeInfoMap<Tree>, class = std::enable_if_t<!std::is_const_ref<LeafList>>>
  EdgeVec get_induced_edges(const policy_inplace_t, const Tree& supertree, LeafList&& leaves, std::shared_ptr<NodeInfoMap> node_infos = {})
  {
    make_node_infos(supertree, node_infos);
    sort_by_order_number(leaves, *node_infos);
    return _induced_subtree_edges<Tree, LeafList, NodeInfoMap>
      (supertree, std::shared_ptr<const std::remove_cvref_t<LeafList>>(&leaves, std::NoDeleter()), std::move(node_infos)).get_edges();
  }


  // general function, guessing the data_policy (pessimistically!)
  //NOTE: in particular, this assumees that your leaves ARE NOT SORTED! If they are, call the policy_noop version for more speed :)
  template<class Tree, class LeafList, class NodeInfoMap = SparseInducedSubtreeInfoMap<Tree>>
  EdgeVec get_induced_edges(const Tree& supertree, LeafList&& leaves, std::shared_ptr<NodeInfoMap> node_infos = {})
  {
    if(std::is_const_ref<LeafList>)
      return get_induced_edges(policy_copy, supertree, leaves, std::move(node_infos));
    else
      return get_induced_edges(policy_inplace, supertree, leaves, std::move(node_infos));
  }

}
