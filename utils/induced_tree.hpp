
#pragma once

/* compute the subtree induced by a list of leaves */

#include <memory>

namespace PT{

  // for each node, we'll need its number in any ordering (pre-/in-/post-)order, as well as its distance to the root
  struct LeavesInducedSubtreeInfo{
    size_t dist_to_root;
    size_t order_number;
    LeavesInducedSubtreeInfo(const size_t _dtr = 0, const size_t _on = 0):
      dist_to_root(_dtr), order_number(_on) {}
  };
  template<class Tree>
  using LeavesInducedSubtreeInfoMap = typename Tree::template NodeMap<LeavesInducedSubtreeInfo>;

  struct SparseLeavesInducedSubtreeInfo{
    size_t dist_to_root;
    SparseLeavesInducedSubtreeInfo(const size_t _dtr = 0, const size_t _ignore = 0):
      dist_to_root(_dtr) {}
  };
  template<class Tree>
  using SparseLeavesInducedSubtreeInfoMap = typename Tree::template NodeMap<SparseLeavesInducedSubtreeInfo>;

  // compute necessary information from a tree
  template<class Tree, class NodeInfoMap = LeavesInducedSubtreeInfoMap<Tree>>
  NodeInfoMap get_leaves_induced_subtree_infos(const Tree& t)
  {
    NodeInfoMap node_infos;
    size_t counter = 0;
    // get a preorder because we need to access the distance to the root of the parent
    const auto preorder_dfs = t.dfs().preorder(); // const since our preorder doesn't need to track seen nodes (we're in a tree)
    auto iter = preorder_dfs.begin(); // this is the root now
    node_infos.try_emplace(*iter, 0, 0); // order number 0, distance to root 0
    for(++iter;iter.is_valid(); ++iter){
      const Node u = *iter;
      node_infos.try_emplace(u, node_infos.at(t.parent(u)).dist_to_root + 1, ++counter);
    }
    std::cout << "done node-traversing\n";
    return node_infos;
  }

  // compute the list of edges present in the tree induced by a list L of leaves
  //NOTE: we'll need to know for each node u its distance to the root, and we'd also like L to be in some order (any of pre-/in-/post- will do)
  //      so if the caller doesn't provide them, we'll compute them from the given supertree
  //NOTE: if the infos are provided, then this runs in O(|L| * LCA-query in supertree), otherwise, an O(|supertree|) DFS is prepended
  //NOTE: a slight speedup may be attained by passing the leaves in order (obviously in an ordered container)
  //NOTE: we force the given __Tree to be declared a tree (tree_tag)
  template<class Tree, class LeafList = std::vector<Node>, class NodeInfoMap = LeavesInducedSubtreeInfoMap<Tree>>
  class leaves_induced_subtree_edges {
  protected:
    using EdgeVec = std::vector<PT::Edge<>>;
    using NodeWithDepth = std::pair<Node,size_t>;

    const Tree& supertree;
    const std::shared_ptr<NodeInfoMap> node_infos;
    const std::shared_ptr<const LeafList> leaves_sorted;
    std::vector<NodeWithDepth> inner_nodes;
    std::vector<ssize_t> v_left_idx, v_right_idx;

  public:

    // NOTE: in all constructors, the caller can give us an empty shared_ptr to node_infos and we'll fill it

    // construct from lvalue-referenced leaves - use them as is, assume they are sorted
    //NOTE: be sure that your leaves are sorted, the constructor will not doublecheck
    //NOTE: this also permits the user to instanciate the whole class with a NodeInfoMap that does NOT contain order_numbers...
    leaves_induced_subtree_edges(const Tree& _supertree,
        const LeafList& _leaves,
        std::shared_ptr<NodeInfoMap> _node_infos = std::make_shared<NodeInfoMap>()):
      supertree(_supertree),
      // compute node infos if they are not provided by the caller
      node_infos(_node_infos->empty() ? std::make_shared<NodeInfoMap>(get_leaves_induced_subtree_infos(_supertree)) : _node_infos),
      leaves_sorted(&_leaves, std::NoDeleter())
    { prepare_nodes(); }


    // construct from rvalue-referenced leaves - move them into local storage
    template<class _LeafList,
            class _NodeInfoMap = NodeInfoMap, // this template is so the user may not pass order_numbers if constructing from const LeafList&
            class LocalLeafList = LeafList,
            class = std::enable_if_t<std::is_sortable<LocalLeafList>>> // disable if our local LeafList is not sortable
    leaves_induced_subtree_edges(const Tree& _supertree,
        std::add_rvalue_reference<_LeafList> _leaves,
        std::shared_ptr<_NodeInfoMap> _node_infos = std::make_shared<_NodeInfoMap>()):
      supertree(_supertree),
      // compute node infos if they are not provided by the caller
      node_infos(_node_infos->empty() ? std::make_shared<_NodeInfoMap>(get_leaves_induced_subtree_infos(_supertree)) : _node_infos)
    {
      sort_incoming_leaves(std::make_unique<LeafList>(std::move(_leaves)));
      prepare_nodes();
    }

    // construct from lvalue-referenced leaves - sort them in-place
    template<class _NodeInfoMap = NodeInfoMap, // this template is so the user may not pass order_numbers if constructing from const LeafList&
            class LocalLeafList = LeafList,
            class = std::enable_if_t<std::is_sortable<LocalLeafList>>> // disable if our local LeafList is not sortable
    leaves_induced_subtree_edges(const Tree& _supertree,
        LeafList& _leaves,
        std::shared_ptr<_NodeInfoMap> _node_infos = std::make_shared<_NodeInfoMap>()):
      supertree(_supertree),
      // compute node infos if they are not provided by the caller
      node_infos(_node_infos->empty() ? std::make_shared<_NodeInfoMap>(get_leaves_induced_subtree_infos(_supertree)) : _node_infos)
    {
      sort_inomcing_leaves(std::make_unique<LeafList>(&_leaves, std::NoDeleter()));
      prepare_nodes();
    }

    EdgeVec get_edges()
    {
      EdgeVec result;
      if(!inner_nodes.empty()){
        assert(leaves_sorted->size() > 1);
        result.reserve(leaves_sorted->size() * 2 - 2);
        // step 1: get edges incoming to leaves
        size_t i = 0;
        auto leaf_iter = leaves_sorted->begin();
        append(result, front(inner_nodes).first, *leaf_iter);
        for(++leaf_iter; i + 1 != inner_nodes.size(); ++leaf_iter, ++i){
          const Node parent = inner_nodes[choose_parent(i, i + 1)].first;
          append(result, parent, *leaf_iter);
        }
        append(result, inner_nodes.back().first, *leaf_iter);
        // step 2: get edges incoming to inner nodes
        for(i = 0; i != inner_nodes.size(); ++i){
          const ssize_t parent_idx = choose_parent(v_left_idx[i], v_right_idx[i]);
          if(parent_idx != -1)
            append(result, inner_nodes[parent_idx].first, inner_nodes[i].first);
        }
      }
      std::cout << "returning edgelist "<<result<<"\n";
      return result;
    }


  protected:
    // choose the right parent between a choice of two indices in inner_nodes (which may be -1)
    ssize_t choose_parent(const ssize_t u_idx, const ssize_t v_idx) const
    {
      if(u_idx == -1) return v_idx;
      if(v_idx == -1) return u_idx;
      return (inner_nodes[u_idx].second < inner_nodes[v_idx].second) ? v_idx : u_idx;
    }


    template<class _LeafList>
    void sort_incoming_leaves(std::unique_ptr<_LeafList>& _leaves)
    {
      auto sort_by_order = [&](const auto& a, const auto& b) -> bool {  return node_infos->at(a).order_number > node_infos->at(b).order_number; };
      // move the given leaflist into a sortable range & sort
      std::sort(_leaves->begin(), _leaves->end(), sort_by_order);
      // swap the pointers into our local, const storage
      leaves_sorted = std::move(_leaves);
    }


    void compute_inner_nodes()
    {
      inner_nodes.reserve(leaves_sorted->size() - 1);
      for(auto iter = leaves_sorted->begin(); iter != leaves_sorted->end();){
        const Node u = *iter;
        if(++iter == leaves_sorted->end()) break;
        const Node l = supertree.LCA(u, *iter);
        std::cout << "adding inner node: "<<l<<" with dist "<<node_infos->at(l).dist_to_root<<"\n";
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
        const NodeWithDepth u = inner_nodes[index];
        while(!depth_stack.empty() && (depth_stack.top().second > u.second)) depth_stack.pop();
        result[index] = (depth_stack.empty()) ? -1 : depth_stack.top().first;
        depth_stack.emplace(index, u.second);
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


}
