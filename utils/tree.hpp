
#pragma once

#include <vector>
#include "utils.hpp"
#include "label_iter.hpp"
#include "types.hpp"
#include "dfs.hpp"
#include "except.hpp"
#include "edge.hpp"
#include "set_interface.hpp"
#include "storage_adj_mutable.hpp"
#include "storage_adj_immutable.hpp"
#include "induced_tree.hpp"

namespace PT{

  enum node_type { NODE_TYPE_LEAF=0x00, NODE_TYPE_TREE=0x01, NODE_TYPE_RETI=0x02, NODE_TYPE_ISOL=0x03 };

#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!
  struct tree_tag {};
  struct network_tag {};
  struct single_label_tag {};
  struct multi_label_tag {};

  // the central Tree class
  // note: we need to give the label-map type as template in order to allow creating mutable copies of subtrees of immutable trees
  //       keeping a reference to the label-map of the host tree
  template<class _LabelTag = single_label_tag,
           class _EdgeStorage = MutableTreeAdjacencyStorage<void, void>,
           class _LabelMap = typename _EdgeStorage::template NodeMap<std::string>,
           class _NetworkTag = tree_tag>
  class _Tree: public _EdgeStorage
  {
    using Parent = _EdgeStorage;

    template<class, class, class, class> friend class _Tree;

  public:
    using typename Parent::ConstNodeContainer;
    using typename Parent::ConstLeafContainer;
    using typename Parent::Edge;
    using typename Parent::RevAdjacency;
    using typename Parent::MutabilityTag;
    using EdgeStorage = _EdgeStorage;

    using NetworkTag = _NetworkTag;
    // NOTE: something that is declared a network might actually be a tree (is_tree() will reveal the truth),
    //       but is_declared_tree allows setting reasonable defaults (for example, for DFSIterators)
    static constexpr bool is_declared_tree = std::is_same_v<NetworkTag, tree_tag>;

    static constexpr bool is_mutable = std::is_same_v<MutabilityTag, mutable_tag>;
    static constexpr bool has_consecutive_nodes = !is_mutable;

    using LabelTag = _LabelTag;
    using LabelMap = _LabelMap;
    using LabelType = std::copy_cvref_t<LabelMap, typename LabelMap::mapped_type>;
    static constexpr bool is_single_labeled = std::is_same_v<LabelTag, single_label_tag>;

    // we don't want to use the generic container-operator<< for trees, even though we provide an "iterator" type
    using custom_output = std::true_type;


    using LabeledNodeContainer    = LabeledNodeIterFactory<const ConstNodeContainer, std::MapGetter<LabelMap>>;
    using LabeledNodeContainerRef = LabeledNodeContainer;
    
    using LabeledLeafContainer    = LabeledNodeIterFactory<const ConstLeafContainer, std::MapGetter<LabelMap>>;
    using LabeledLeafContainerRef = LabeledLeafContainer;

  protected:
    std::shared_ptr<LabelMap> node_labels;

  public:
    using Parent::num_edges;
    using Parent::num_nodes;
    using Parent::out_degree;
    using Parent::in_degree;
    using Parent::root;
    using Parent::nodes;
    using Parent::leaves;
    using Parent::parents;
    using Parent::children;
    using Parent::out_edges;
    using Parent::in_edges;

    // =============== variable query ======================
    bool is_tree() const { return num_edges() == num_nodes() - 1; }
    bool empty() const { return num_nodes() == 0; }
    bool edgeless() const { return num_edges() == 0; }
    bool is_leaf(const Node u) const { return out_degree(u) == 0; }
    bool is_root(const Node u) const { return u == root(); }
    bool is_tree_node(const Node u) const { return out_degree(u) > 0; }
    bool is_suppressible(const Node u) const { return (in_degree(u) == 1) && (out_degree(u) == 1); }

    LabelType& label(const Node u) { return node_labels->at(u); }
    const LabelType& label(const Node u) const { return node_labels->at(u); }
    LabelMap& labels() { return *node_labels; }
    const LabelMap& labels() const { return *node_labels; }

    node_type type_of(const Node u) const
    {
      if(is_leaf(u)) return NODE_TYPE_LEAF;
      if(is_tree_node(u)) return NODE_TYPE_TREE;
      return NODE_TYPE_ISOL;
    }

    // this asks for the first parent of u; for trees, it must be the only parent of u (but networks might also be interested in "any" parent of u)
    RevAdjacency parent(const Node u) const { return std::front(parents(u)); }
    // this returns the root as parent for the root, instead of segfaulting / throwing out-of-range
    //NOTE: since this must work even if the tree has no edges, this cannot deal with edge-data, so everything is a node here
    Node parent_safe(const Node u) const { return (u == root()) ? u : parent(u); }



    
    // =============== traversals ======================
    // the following functions return a "meta"-traversal object, which can be used as follows:
    // iterate over "dfs().preorder()" to get all nodes in preorder
    // iterate over "edge_dfs().postorder(u)" to get all edges below u in postorder
    // say "NodeVec(dfs_except(forbidden).inorder(u))" to get a vector of nodes below u but strictly above the nodes of "forbidden" in inorder
    //NOTE: refrain from taking "const auto x = dfs();" since const MetaTraversals are useless...
    //NOTE: if you say "const auto x = dfs().preorder();" you won't be able to track seen nodes (which might be OK for trees...)
  protected: // per default, disable tracking of seen nodes iff we're a tree
    using DefaultSeen = std::conditional_t<is_declared_tree, void, typename Parent::NodeSet>;
  public:

    MetaTraversal<      _Tree, DefaultSeen, NodeTraversal> dfs() { return *this; }
    MetaTraversal<const _Tree, DefaultSeen, NodeTraversal> dfs() const { return *this; }
    MetaTraversal<      _Tree, DefaultSeen, EdgeTraversal> edge_dfs() { return *this; }
    MetaTraversal<const _Tree, DefaultSeen, EdgeTraversal> edge_dfs() const { return *this; }
    MetaTraversal<      _Tree, DefaultSeen, AllEdgesTraversal> all_edges_dfs() { return *this; }
    MetaTraversal<const _Tree, DefaultSeen, AllEdgesTraversal> all_edges_dfs() const { return *this; }

    //NOTE: SeenSet must support "append(set, node)" and "test(set, node)" - any set of nodes will do, even (un)ordered_bitset, but not vector<Node> (no test)
    template<class ExceptSet, class SeenSet = std::remove_reference_t<ExceptSet>>
    MetaTraversal<      _Tree, SeenSet, NodeTraversal> dfs_except(ExceptSet&& except) { return {*this, std::forward<ExceptSet>(except)}; }
    template<class ExceptSet, class SeenSet = std::remove_reference_t<ExceptSet>>
    MetaTraversal<const _Tree, SeenSet, NodeTraversal> dfs_except(ExceptSet&& except) const { return {*this, std::forward<ExceptSet>(except)}; }
    template<class ExceptSet, class SeenSet = std::remove_reference_t<ExceptSet>>
    MetaTraversal<      _Tree, SeenSet, EdgeTraversal> edge_dfs_except(ExceptSet&& except) { return {*this, std::forward<ExceptSet>(except)}; }
    template<class ExceptSet, class SeenSet = std::remove_reference_t<ExceptSet>>
    MetaTraversal<const _Tree, SeenSet, EdgeTraversal> edge_dfs_except(ExceptSet&& except) const { return {*this, std::forward<ExceptSet>(except)}; }
    template<class ExceptSet, class SeenSet = std::remove_reference_t<ExceptSet>>
    MetaTraversal<      _Tree, SeenSet, AllEdgesTraversal> all_edges_dfs_except(ExceptSet&& except) { return {*this, std::forward<ExceptSet>(except)}; }
    template<class ExceptSet, class SeenSet = std::remove_reference_t<ExceptSet>>
    MetaTraversal<const _Tree, SeenSet, AllEdgesTraversal> all_edges_dfs_except(ExceptSet&& except) const { return {*this, std::forward<ExceptSet>(except)}; }



    LabeledNodeContainerRef nodes_labeled() const { return LabeledNodeContainerRef(nodes(), *node_labels); }
    LabeledLeafContainerRef leaves_labeled() const { return LabeledLeafContainerRef(leaves(), *node_labels); }




    // ========================= LCA ===========================
    //
    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    Node naiveLCA(Node x, Node y) const
    {
      std::unordered_bitset seen(num_nodes());
      while(x != y){
        if(update_for_LCA(seen, x)) return x;
        if(update_for_LCA(seen, y)) return y;
      }
      return x;
    }
  protected:
    // helper function for the LCA
    inline bool update_for_LCA(std::unordered_bitset& seen, Node& z) const
    {
      if(z == root()) return false;
      if(!seen.set(z)) return true;
      z = parent(z);
      return false;
    }
  public:

#warning TODO: use more efficient LCA
    Node LCA(const Node x, const Node y) const { return naiveLCA(x,y); }

    //! return if there is a directed path from x to y in the tree; requires the tree to be pre-ordered
    bool has_path(Node x, const Node y) const
    {
      if(x < y) return false;
      while(1){
        if(x == y) return true;
        if(x == root) return false;
        x = parent(x);
      } 
    }

    // return the descendant among x and y unless they are incomparable; return NoNode in this case
    Node get_minimum(const Node x, const Node y) const
    {
      const Node lca = LCA(x,y);
      if(lca == x) return y;
      if(lca == y) return x;
      return NoNode;
    }


    //! return whether the tree indices are in pre-order (modulo gaps) (they should always be) 
    bool is_preordered(const Node sub_root, Node& counter) const
    {
      if(sub_root >= counter) {
        counter = sub_root;
        for(const Node v: children(sub_root))
          if(!is_preordered(v, counter)) return false;
        return true;
      } else return false;
    }

    bool is_preordered() const
    {
      Node counter = root;
      return is_preordered(root, counter);
    }

    bool is_multi_labeled() const
    {
      HashSet<LabelType> seen;
      for(const Node u: leaves())
        if(test(seen, (*node_labels)[u])) return true;
      return false;
    }

    bool is_edge(const Node u, const Node v) const  { return test(children(u), v); }
    bool adjacent(const Node u, const Node v) const { return is_edge(u,v) || is_edge(v,u); }

    //! for sanity checks: test if there is a directed cycle in the data structure (more useful for networks, but definable for trees too)
    bool has_cycle() const
    {
      if(!empty())
        return has_cycle(root, NodeVec(num_nodes()), 1);
      else return false;
    }

  protected:
    bool has_cycle(const Node sub_root, uint32_t* depth_at, const uint32_t depth) const
    {
      if(depth_at[(size_t)sub_root] == 0){
        depth_at[(size_t)sub_root] = depth;
        for(const Node w: children(sub_root))
          if(has_cycle(w, depth_at, depth + 1)) return true;
        depth_at[(size_t)sub_root] = UINT32_MAX; // mark as seen and not cyclic
        return false;
      } else return (depth_at[(size_t)sub_root] <= depth);
    }
  public:
    // =================== modification ====================
   
    void remove_node(const Node u, const bool remove_labels = false)
    {
      if(remove_labels) node_labels->erase(u);
      Parent::remove_node(u);
    }

    // remove the subtree rooted at u
    void remove_subtree(const Node u, const bool remove_labels = false)
    {
      const auto& C = children(u);
      while(!C.empty()) remove_subtree(front(C), remove_labels);
      remove_node(u, remove_labels);
    }


    // =================== rooted subtrees ====================

    // copy the subtree rooted at u into t
    template<class __Tree = _Tree<LabelTag, EdgeStorage, LabelMap, NetworkTag>>
    __Tree get_rooted_subtree(const Node u) const
    { return {get_edges_below(u), *node_labels}; }
    // copy the subtree rooted at u into t, but ignore subtrees rooted at nodes in 'except'
    template<class __Tree = _Tree<LabelTag, EdgeStorage, LabelMap, NetworkTag>, class ExceptContainer>
    __Tree get_rooted_subtree_except(const Node u, ExceptContainer&& except) const
    { return {get_edges_below_except(u, except), *node_labels}; }


    template<class _Edgelist>
    _Edgelist& get_edges_below(const Node u, _Edgelist& el) const
    { return all_edges_dfs().postorder(u).append_to(el); }

    template<class _Edgelist, class _NodeContainer>
    _Edgelist& get_edges_below_except(const Node u, _Edgelist& el, const _NodeContainer& except) const
    { return all_edges_dfs_except(except).postorder(u).append_to(el); }

    // ================== construction =====================

    std::ostream& tree_summary(std::ostream& os) const
    {
      DEBUG3(os << "tree is mutable? "<<is_mutable<<" multi-labeled? "<<!is_single_labeled<<"\n");
      DEBUG3(os << "tree has "<<num_edges()<<" edges and "<<num_nodes()<<" nodes, leaves: "<<leaves()<<"\n");
      
      for(const Node i: nodes())
        os << i << " (" << node_labels->at(i) << "):" << "\tIN: "<< in_edges(i) << "\tOUT: "<< out_edges(i) << std::endl;
      return os << "\n";
    }
    
    // initialize tree from any iterable edge-container, for example, std::vector<PT::Edge<>>
    // NOTE: this will move edge data out of your container! If you want to copy it instead, pass a const container (e.g. using std::as_const())
    template<class GivenEdgeContainer>
    _Tree(GivenEdgeContainer&& given_edges, std::shared_ptr<LabelMap> _node_labels):
      Parent(edgelist_tag, std::forward<GivenEdgeContainer>(given_edges)),
      node_labels(std::move(_node_labels))
    {
      DEBUG3(std::cout << "init Tree with edges "<<given_edges<<"\n and "<<node_labels->size()<<" node_labels: "<<*node_labels<<std::endl);
      DEBUG2(tree_summary(std::cout));
    }
    
    template<class GivenEdgeContainer>
    _Tree(GivenEdgeContainer&& given_edges, LabelMap& _node_labels):
      _Tree(std::forward<GivenEdgeContainer>(given_edges), std::make_shared<LabelMap>(_node_labels))
    {}
    
    // initialize tree from any iterable edge-container, indicating that the edges in the container do not have consecutive nodes (starting from 0)
    // NOTE: this will move edge data out of your container! If you want to copy it instead, pass a const container (e.g. using std::as_const())
    template<class GivenEdgeContainer>
    _Tree(const non_consecutive_tag_t, GivenEdgeContainer&& given_edges, std::shared_ptr<LabelMap> _node_labels):
      Parent(non_consecutive_tag, std::forward<GivenEdgeContainer>(given_edges)),
      node_labels(std::move(_node_labels))
    {
      DEBUG3(std::cout << "init Tree with non-consecutive edges "<<given_edges<<"\n and "<<node_labels->size()<<" node_labels: "<<*node_labels<<std::endl);
      DEBUG2(tree_summary(std::cout));
    }
    template<class GivenEdgeContainer>
    _Tree(const non_consecutive_tag_t, GivenEdgeContainer&& given_edges, LabelMap& _node_labels):
      _Tree(non_consecutive_tag, std::forward<GivenEdgeContainer>(given_edges), std::make_shared<LabelMap>(_node_labels))
    {}

    // Copy construction from any tree
    //NOTE: if the LabelMaps are the same, then just create a new reference to it, otherwise construct a copy
    //NOTE: apologies for the evil C-style cast, but I seem to be unable to make reinterpret_cast<> work...
    template<class __Tree>
    _Tree(const __Tree& in_tree):
      Parent(std::is_same_v<EdgeStorage, typename __Tree::EdgeStorage> ? // same EdgeContainer ?
                              EdgeStorage(*((const EdgeStorage*)(&in_tree))) : // yes: move edges if possible
                              EdgeStorage(edgelist_tag, in_tree.edges())), // no: make a copy of the EdgeStorage (and move it into place)
      node_labels(std::is_same_v<LabelMap, typename __Tree::LabelMap> ? // same LabelMap's ?
                          in_tree.node_labels : // yes: just create a new reference
                          std::make_shared<LabelMap>(in_tree.node_labels->begin(), in_tree.node_labels->end())) // no: make a copy
    {
      std::cout << "\tcopy-constructed tree:\n";
      tree_summary(std::cout);
    }

    // Move construction from any tree (see copy construction)
    template<class __Tree, class = std::enable_if_t<std::is_rvalue_reference_v<__Tree&&>>>
    _Tree(__Tree&& in_tree):
      Parent(std::is_same_v<EdgeStorage, typename __Tree::EdgeStorage> ? // same EdgeContainer ?
                              EdgeStorage(std::move(*((EdgeStorage*)(&in_tree)))) : // yes: move edges if possible
                              EdgeStorage(edgelist_tag, in_tree.edges())), // no: make a copy of the EdgeStorage (and move it into place)
      node_labels(std::is_same_v<LabelMap, typename __Tree::LabelMap> ? // same LabelMap's ?
                          std::move(in_tree.node_labels) : // yes: just create a new reference
                          std::make_shared<LabelMap>(in_tree.node_labels->begin(), in_tree.node_labels->end())) // no: make a copy
    {
      std::cout << "\tmove-constructed tree\n";
      tree_summary(std::cout);
    }

    // for some weird reason, the above is not counted as move-constructor, so I have to repeat myself...
    _Tree(_Tree&& in_tree): Parent(std::move(in_tree)), node_labels(std::move(in_tree.node_labels))
    {
      std::cout << "\tmove-constructed tree (non templated)\n";
      tree_summary(std::cout);
    }

    // initialize a tree as the smallest subtree spanning a list L of nodes in a given supertree
    //NOTE: we'll need to know for each node u its distance to the root, and we'd also like L to be in some order (any of pre-/in-/post- will do)
    //      so if the caller doesn't provide them, we'll compute them from the given supertree
    //NOTE: if the infos are provided, then this runs in O(|L| * LCA-query in supertree), otherwise, an O(|supertree|) DFS is prepended
    //NOTE: when passed as a const container, the nodes are assumed to be in order; ATTENTION: this is not verified!
    //NOTE: we force the given __Tree to be declared a tree (tree_tag)
    template<class __LabelTag, class __EdgeStorage, class LeafList, class __NetworkTag,
      class NodeInfoMap = InducedSubtreeInfoMap<_Tree<__LabelTag, __EdgeStorage, LabelMap, __NetworkTag>>>
    _Tree(const _Tree<__LabelTag, __EdgeStorage, LabelMap, __NetworkTag>& supertree,
        LeafList&& _leaves,
        std::shared_ptr<NodeInfoMap> _node_infos = std::make_shared<NodeInfoMap>()):
      Parent(edgelist_tag, get_induced_edges(supertree, std::forward<LeafList>(_leaves), std::move(_node_infos))),
      node_labels(supertree.node_labels)
    {}
    // the caller can be explicit about the data transfer policy (policy_move, policy_copy, policy_inplace)
    template<class data_policy_tag, class __LabelTag, class __EdgeStorage, class LeafList, class __NetworkTag,
      class NodeInfoMap = InducedSubtreeInfoMap<_Tree<__LabelTag, __EdgeStorage, LabelMap, __NetworkTag>>>
    _Tree(const data_policy_tag policy,
          const _Tree<__LabelTag, __EdgeStorage, LabelMap, __NetworkTag>& supertree,
          LeafList&& _leaves,
          std::shared_ptr<NodeInfoMap> _node_infos = std::make_shared<NodeInfoMap>()):
      Parent(edgelist_tag, get_induced_edges(policy, supertree, std::forward<LeafList>(_leaves), std::move(_node_infos))),
      node_labels(supertree.node_labels)
    {}


    // initialize a subtree rooted at a node of the given tree
    // note: we have to force the label-maps to be compatible since they are just references to a global map stored somewhere else
    // note: the label-types CAN be different, but you have to make sure that they fit - for example, if you construct a single-labeled
    //       tree as a subtree of a multi-labeled tree, you'll have to make sure that this subtree is indeed single-labeled, as otherwise,
    //       some code later on may fail!
    // note: Likewise, if you construct a subtree with _NetworkTag = tree_tag of a network, you better make sure there are no "loops" in that subtree!
    template<class __LabelTag, class __EdgeStorage, class __NetworkTag>
    _Tree(const _Tree<__LabelTag, __EdgeStorage, LabelMap, __NetworkTag>& supertree, const Node _root):
      Parent(edgelist_tag, supertree.get_edges_below(_root)),
      node_labels(supertree.labels())
    {}

  public:


    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const Node u, std::string prefix) const
    {
      LabelType name = node_labels->at(u);
      DEBUG3(name += "[" + std::to_string(u) + "]");
      if(name == "") name = "+";
      os << '-' << name;

      const auto& u_childs = children(u);
      switch(u_childs.size()){
        case 0:
          os << std::endl;
          break;
        case 1:
          print_subtree(os, std::front(u_childs), prefix + std::string(name.length() + 1u, ' '));
          break;
        default:
          prefix += std::string(name.length(), ' ') + '|';

          uint32_t count = u_childs.size();
          for(const auto c: u_childs){
            print_subtree(os, c, prefix);
            if(--count > 0) os << prefix;
            if(count == 1) prefix.back() = ' ';
          }
      }
    }
    
  };
  
  template<class _LabelTag, class _EdgeStorage, class _LabelMap>
  std::ostream& operator<<(std::ostream& os, const _Tree<_LabelTag, _EdgeStorage, _LabelMap>& T)
  {
    if(!T.empty()){
      std::string prefix = "";
      T.print_subtree(os, T.root(), prefix);
    } else os << "{}";
    return os;
  }



  template<class _NodeData,
           class _EdgeData = void,
           class _LabelTag = single_label_tag,
           class _EdgeStorage = MutableTreeAdjacencyStorage<_EdgeData>,
           class _LabelMap = typename _EdgeStorage::template NodeMap<std::string>,
           class _NetworkTag = tree_tag>
  using Tree = AddNodeData<_NodeData, _Tree<_LabelTag, _EdgeStorage, _LabelMap, _NetworkTag>>;

  // these two types should cover 95% of all (non-internal) use cases
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = single_label_tag, class _LabelMap = HashMap<Node, std::string>>
  using RWTree = Tree<_NodeData, _EdgeData, _LabelTag, MutableTreeAdjacencyStorage<_EdgeData>, _LabelMap>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = single_label_tag, class _LabelMap = RawConsecutiveMap<Node, std::string>>
  using ROTree = Tree<_NodeData, _EdgeData, _LabelTag, ConsecutiveTreeAdjacencyStorage<_EdgeData>, _LabelMap>;

  // here's 2 for multi-label convenience
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = multi_label_tag, class _LabelMap = HashMap<Node, std::string>>
  using RWMulTree = RWTree<_NodeData, _EdgeData, _LabelTag, _LabelMap>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = multi_label_tag, class _LabelMap = RawConsecutiveMap<Node, std::string>>
  using ROMulTree = ROTree<_NodeData, _EdgeData, _LabelTag, _LabelMap>;

  // use these two if you have declared a tree and need a different type of tree which should interact with the first one (i.e. needs the same label-map)
  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _LabelTag = typename __Tree::LabelTag,
    class _MutabilityTag = typename __Tree::MutabilityTag>
  using CompatibleTree = std::conditional_t<std::is_same_v<_MutabilityTag, mutable_tag>,
                                RWTree<_NodeData, _EdgeData, _LabelTag, typename __Tree::LabelMap>,
                                ROTree<_NodeData, _EdgeData, _LabelTag, typename __Tree::LabelMap>>;

  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _LabelTag = typename __Tree::LabelTag>
  using CompatibleRWTree = CompatibleTree<__Tree, _NodeData, _EdgeData, _LabelTag, mutable_tag>;
  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _LabelTag = typename __Tree::LabelTag>
  using CompatibleROTree = CompatibleTree<__Tree, _NodeData, _EdgeData, _LabelTag, immutable_tag>;
  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _MutabilityTag = typename __Tree::MutabilityTag>
  using CompatibleMulTree = CompatibleTree<__Tree, _NodeData, _EdgeData, multi_label_tag, _MutabilityTag>;
  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _MutabilityTag = typename __Tree::MutabilityTag>
  using CompatibleSilTree = CompatibleTree<__Tree, _NodeData, _EdgeData, single_label_tag, _MutabilityTag>;

  template<class __Tree>
  using LabelMapOf = typename std::remove_reference_t<__Tree>::LabelMap;

  template<class __TreeA, class __TreeB>
  constexpr bool are_compatible_v = std::is_same_v<std::remove_cvref_t<LabelMapOf<__TreeA>>, std::remove_cvref_t<LabelMapOf<__TreeB>>>;

}
