
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

namespace PT{

#define NODE_TYPE_LEAF 0x00
#define NODE_TYPE_TREE 0x01
#define NODE_TYPE_RETI 0x02
#define NODE_TYPE_ISOL 0x03

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
  class _Tree 
  {
  public:
    using NetworkTypeTag = _NetworkTag;
    
    static constexpr bool is_network = std::is_same_v<_NetworkTag, network_tag>;
    static constexpr bool is_tree    = std::is_same_v<_NetworkTag, tree_tag>;

    using MutabilityTag = typename _EdgeStorage::MutabilityTag;
    static constexpr bool is_mutable = std::is_same_v<MutabilityTag, mutable_tag>;
    static constexpr bool has_consecutive_nodes = is_mutable;

    using LabelTag = _LabelTag;
    static constexpr bool is_single_labeled = std::is_same_v<LabelTag, single_label_tag>;

    template<class T>
    using NodeMap = typename _EdgeStorage::template NodeMap<T>;

    using EdgeStorage = _EdgeStorage;
    using Edge = typename EdgeStorage::Edge;
    using EdgeRef = typename EdgeStorage::EdgeRef;
    using EdgeData  = typename EdgeStorage::EdgeData;

    using EdgeContainer     = typename EdgeStorage::ConstEdgeContainer;
    using EdgeContainerRef  = typename EdgeStorage::ConstEdgeContainerRef;
    using NodeContainer     = typename EdgeStorage::ConstNodeContainer;
    using NodeContainerRef  = typename EdgeStorage::ConstNodeContainerRef;
    using NodeIterator      = typename NodeContainer::const_iterator;
    
    using OutEdgeContainer  = typename EdgeStorage::ConstOutEdgeContainer;
    using OutEdgeContainerRef = typename EdgeStorage::ConstOutEdgeContainerRef;
    using InEdgeContainer   = typename EdgeStorage::ConstInEdgeContainer;
    using InEdgeContainerRef= typename EdgeStorage::ConstInEdgeContainerRef;
    using SuccContainer     = typename EdgeStorage::ConstSuccContainer;
    using SuccContainerRef  = typename EdgeStorage::ConstSuccContainerRef;
    using PredContainer     = typename EdgeStorage::ConstPredContainer;
    using PredContainerRef  = typename EdgeStorage::ConstPredContainerRef;
    using LeafContainer     = typename EdgeStorage::ConstLeafContainer;
    using LeafContainerRef  = typename EdgeStorage::ConstLeafContainerRef;

    using LabelMap = _LabelMap;
    using LabelType = typename LabelMap::mapped_type;
    using LabeledNodeContainer = LabeledNodeIterFactory<NodeContainer, std::MapGetter<const LabelMap>>;
    using LabeledNodeContainerRef = LabeledNodeContainer;
    
    using LabeledLeafContainer = LabeledNodeIterFactory<LeafContainer, std::MapGetter<const LabelMap>>;
    using LabeledLeafContainerRef = LabeledLeafContainer;

  protected:
    const LabelMap& node_labels;
    _EdgeStorage _edges;

  public:

    // =============== variable query ======================
    LeafContainerRef leaves() const { return _edges.leaves(); }
    NodeContainerRef nodes() const { return _edges.nodes(); }
    EdgeContainerRef edges() const { return _edges.successor_map(); }
    const LabelMap& labels() const { return node_labels; }
    NodeIterator begin() const { return _edges.nodes_begin(); }
    NodeIterator end() const { return _edges.nodes_end(); }

    Node root() const { return _edges.root(); }
    size_t num_nodes() const {return _edges.num_nodes();}
    //size_t num_leaves() const {return _edges.num_leaves();}
    size_t num_edges() const {return _edges.num_edges();}
    const LabelType& get_label(const Node u) const { return node_labels.at(u); }

    //inline NodeVec nodes_preorder(const Node u = root()) const { return tree_traversal<preorder>(*this, u); }
    //inline NodeVec nodes_postorder(const Node u = root()) const { return tree_traversal<postorder>(*this, u); }
    //inline NodeVec nodes_inorder(const Node u = root()) const { return tree_traversal<inorder>(*this, u); }

    // list all nodes below u in order o (preorder, inorder, postorder)
    template<TraversalType o = postorder, class _Container = NodeVec>
    inline _Container get_nodes_below(const Node u) const
    { return node_traversal<o, is_network, _Container>(*this, u); }
    template<TraversalType o = postorder, class _Container = NodeVec>
    inline _Container get_nodes() const
    { return node_traversal<o, is_network, _Container>(*this, root()); }
#define get_nodes_postorder template get_nodes<postorder>
#define get_nodes_preorder template get_nodes<preorder>
#define get_nodes_inorder template get_nodes<inorder>
#define get_nodes_below_postorder template get_nodes_below<postorder>
#define get_nodes_below_preorder template get_nodes_below<preorder>
#define get_nodes_below_inorder template get_nodes_below<inorder>

    // list all get_nodes below u in order o (preorder, inorder, postorder) avoiding the get_nodes given in except
    template<TraversalType o = postorder, class _Container = NodeVec, class _ExceptContainer = NodeSet>
    inline _Container get_nodes_below_except(const _ExceptContainer& except, const Node u) const
    { return node_traversal<o, _Container, _ExceptContainer>(*this, except, u); }
    template<TraversalType o = postorder, class _Container = NodeVec, class _ExceptContainer = NodeSet>
    inline _Container get_nodes_except(const _ExceptContainer& except) const
    { return node_traversal<o, _Container, _ExceptContainer>(*this, except, root()); }
#define get_nodes_except_postorder template get_nodes_except<postorder>
#define get_nodes_except_preorder template get_nodes_except<preorder>
#define get_nodes_except_inorder template get_nodes_except<inorder>
#define get_nodes_below_except_postorder template get_nodes_below_except<postorder>
#define get_nodes_below_except_preorder template get_nodes_below_except<preorder>
#define get_nodes_below_except_inorder template get_nodes_below_except<inorder>

  
    // list all edges below u in order o (preorder, inorder, postorder)
    template<TraversalType o = postorder, class _Container = std::vector<Edge>>
    inline _Container get_edges_below(const Node u) const
    { return edge_traversal<o, is_network, _Container>(*this, u); }
    template<TraversalType o = postorder, class _Container = std::vector<Edge>>
    inline _Container get_edges() const
    { return edge_traversal<o, is_network, _Container>(*this, root()); }
#define get_edges_postorder template get_edges<postorder>
#define get_edges_preorder template get_edges<preorder>
#define get_edges_inorder template get_edges<inorder>
#define get_edges_below_postorder template get_edges_below<postorder>
#define get_edges_below_preorder template get_edges_below<preorder>
#define get_edges_below_inorder template get_edges_below<inorder>



    // list all edges below u in order o (preorder, inorder, postorder) avoiding the nodes given in except
    template<TraversalType o = postorder, class _Container = std::vector<Edge>, class _ExceptContainer = NodeSet>
    inline _Container get_edges_below_except(const _ExceptContainer& except, const Node u) const
    { return edge_traversal<o, _Container, _ExceptContainer>(*this, except, u); }
    template<TraversalType o = postorder, class _Container = std::vector<Edge>, class _ExceptContainer = NodeSet>
    inline _Container get_edges_except(const _ExceptContainer& except) const
    { return edge_traversal<o, _Container, _ExceptContainer>(*this, except, root()); }
#define get_edges_except_postorder template get_edges_except<postorder>
#define get_edges_except_preorder template get_edges_except<preorder>
#define get_edges_except_inorder template get_edges_except<inorder>
#define get_edges_below_except_postorder template get_edges_below_except<postorder>
#define get_edges_below_except_preorder template get_edges_below_except<preorder>
#define get_edges_below_except_inorder template get_edges_below_except<inorder>


    LabeledNodeContainerRef nodes_labeled() const { return LabeledNodeContainerRef(nodes(), node_labels); }
    LabeledLeafContainerRef leaves_labeled() const { return LabeledLeafContainerRef(leaves(), node_labels); }




    // =================== information query ==============

    bool empty() const { return num_nodes() == 0; }
    bool edgeless() const { return num_edges() == 0; }
    Degree in_degree(const Node u) const { return _edges.in_degree(u); }
    Degree out_degree(const Node u) const { return _edges.out_degree(u); }
    InOutDegree in_out_degree(const Node u) const { return _edges.in_out_degree(u); }
    bool is_leaf(const Node u) const { return out_degree(u) == 0; }
    bool is_root(const Node u) const { return u == root(); }
    bool is_tree_node(const Node u) const { return out_degree(u) > 0; }
    bool is_suppressible(const Node u) const { return (in_degree(u) == 1) && (out_degree(u) == 1); }

    Node type_of(const Node u) const
    {
      if(is_leaf(u)) return NODE_TYPE_LEAF;
      if(is_tree_node(u)) return NODE_TYPE_TREE;
      return NODE_TYPE_ISOL;
    }


    PredContainerRef    parents(const Node u) const { return _edges.predecessors(u); }
    SuccContainerRef    children(const Node u) const { return _edges.successors(u); }
    OutEdgeContainerRef out_edges(const Node u) const {return _edges.out_edges(u); }
    InEdgeContainerRef  in_edges(const Node u) const {return _edges.in_edges(u); }
    
    Node parent(const Node u) const { return std::front(parents(u)); }

    Node naiveLCA_preordered(Node x, Node y) const
    {
      assert(is_preordered());
      assert(x < num_nodes());
      assert(y < num_nodes());
      while(x != y){
        if(x > y) 
          x = parent(x);
        else
          y = parent(y);
      }
      return x;
    }
    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    Node naiveLCA(Node x, Node y) const
    {
      assert(x < num_nodes());
      assert(y < num_nodes());
      std::unordered_bitset seen(num_nodes());
      while(x != y){
        if(update_for_LCA(seen, x)) return x;
        if(update_for_LCA(seen, y)) return y;
      }
    }
  protected:
    // helper function for the LCA
    bool update_for_LCA(std::unordered_bitset& seen, Node& z) const
    {
      if((z != root) && !test(seen, (size_t)z)){
        seen.insert((size_t)z);
        z = parent(z);
        return test(seen, (size_t)z);
      } else return false;
    }
  public:

    Node LCA(const Node x, const Node y) const
    {
#warning TODO: use more efficient LCA
      return naiveLCA(x,y);
    }

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
        if(contains(seen, node_labels[u])) return true;
      return false;
    }

    bool is_edge(const Node u, const Node v) const 
    {
      return test(_edges.out_edges(u), v);
    }

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

    //! add an edge
    bool add_edge(const Edge& e) { return _edges.add_edge(e); }
    bool remove_edge(const Edge& e) { return _edges.remove_edge(e); }
    void remove_node(const Node u) { _edges.remove_node(u); }

    void remove_node(const Node u) const
    {
      assert(false && "cannot remove node from const tree/network");
    }

    // copy the subtree rooted at u into t
    template<class __Tree = _Tree<_LabelTag, _EdgeStorage, _LabelMap, _NetworkTag>>
    __Tree get_rooted_subtree(const Node u) const { return {get_edges_below(u), node_labels}; }
    // copy the subtree rooted at u into t, but ignore subtrees rooted at nodes in 'except'
    template<class __Tree = _Tree<_LabelTag, _EdgeStorage, _LabelMap, _NetworkTag>>
    __Tree get_rooted_subtree_except(const Node u, const NodeContainer& except) const { return {get_edges_below_except(u, except), node_labels}; }


    template<class _Edgelist>
    _Edgelist& get_edges_below(const Node u, _Edgelist& el) const
    {
      for(const auto& uv: out_edges(u)){
        append(el, uv);
        get_edges_below(uv.head(), el);
      }
      return el;
    }

    template<class _Edgelist = std::vector<Edge>>
    _Edgelist get_edges_below(const Node u) const
    {
      _Edgelist result;
      return get_edges_below(u, result);
    }

    template<class _Edgelist, class _NodeContainer>
    _Edgelist& get_edges_below_except(const Node u, _Edgelist& el, const _NodeContainer& except) const
    {
      for(const auto& uv: out_edges(u)) if(!contains(except, uv.head())) {
        append(el, uv);
        get_edges_below_except(uv.head(), el, except);
      }
      return el;
    }

    template<class _Edgelist = std::vector<Edge>, class _NodeContainer>
    _Edgelist get_edges_below_except(const Node u, const _NodeContainer& except) const
    {
      _Edgelist result;
      return get_edges_below_except(u, result, except);
    }


    // ================== construction =====================

    void tree_summary(const std::ostream& os) const
    {
      DEBUG3(std::cout << "tree has "<<num_edges()<<" edges and "<<num_nodes()<<" nodes; leaves: "<<leaves()<<"\n");
      for(const Node i: nodes()){
        std::cout << i << ":\n";
        std::cout << " IN: "<< in_edges(i);
        std::cout << " OUT: "<< out_edges(i) << std::endl;
      }

    }

    // initialize tree from any iterable edge-container, for example, std::vector<PT::Edge<>>
    // NOTE: this will move edge data out of your container! If you want to copy it instead, pass a const container (e.g. using std::as_const())
    template<class GivenEdgeContainer>
    _Tree(GivenEdgeContainer&& given_edges, const LabelMap& _node_labels):
      node_labels(_node_labels),
      _edges(std::forward<GivenEdgeContainer>(given_edges))
    {
      DEBUG3(std::cout << "init Tree with edges "<<given_edges<<"\n and "<<_node_labels.size()<<" node_labels: "<<_node_labels<<std::endl);
      DEBUG2(tree_summary(std::cout));
    }

    // initialize tree from any iterable edge-container, indicating that the edges in the container do not have consecutive nodes (starting from 0)
    // NOTE: this will move edge data out of your container! If you want to copy it instead, pass a const container (e.g. using std::as_const())
    template<class GivenEdgeContainer>
    _Tree(const non_consecutive_tag_t, GivenEdgeContainer&& given_edges, const LabelMap& _node_labels):
      node_labels(_node_labels),
      _edges(non_consecutive_tag, std::forward<GivenEdgeContainer>(given_edges))
    {
      DEBUG3(std::cout << "init Tree with non-consecutive edges "<<given_edges<<"\n and "<<_node_labels.size()<<" node_labels: "<<_node_labels<<std::endl);
      DEBUG2(tree_summary(std::cout));
    }



    // initialize a subtree rooted at a node of the given tree
    // note: we have to force the label-maps to be compatible since they are just references to a global map stored somewhere else
    // note: the label-types CAN be different, but you have to make sure that they fit - for example, if you construct a single-labeled
    //       tree as a subtree of a multi-labeled tree, you'll have to make sure that this subtree is indeed single-labeled, as otherwise,
    //       some code later on may fail!
    // note: Likewise, if you construct a subtree with _NetworkTag = tree_tag of a network, you better make sure there are no "loops" in that subtree!
    template<class __LabelTag, class __EdgeStorage, class __NetworkTag>
    _Tree(const _Tree<__LabelTag, __EdgeStorage, LabelMap, __NetworkTag>& supertree, const Node _root):
      node_labels(supertree.node_labels),
      _edges(supertree.get_edges_below(_root))
    {}

  public:


    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const Node u, std::string prefix) const
    {
      LabelType name = node_labels.at(u);
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
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = single_label_tag, class _LabelMap = ConsecutiveMap<Node, std::string>>
  using ROTree = Tree<_NodeData, _EdgeData, _LabelTag, ConsecutiveTreeAdjacencyStorage<_EdgeData>, _LabelMap>;

  // use these two if you have declared a tree and need a different type of tree which should interact with the first one (i.e. needs the same label-map)
  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _LabelTag = typename __Tree::LabelTag>
  using CompatibleRWTree = RWTree<_NodeData, _EdgeData, _LabelTag, typename __Tree::LabelMap>;
  template<class __Tree,
    class _NodeData = typename __Tree::NodeData,
    class _EdgeData = typename __Tree::EdgeData,
    class _LabelTag = typename __Tree::LabelTag>
  using CompatibleROTree = ROTree<_NodeData, _EdgeData, _LabelTag, typename __Tree::LabelMap>;

  template<class __Network>
  using LabelMapOf = typename std::remove_reference_t<__Network>::LabelMap;

  template<class __TreeA, class __TreeB>
  constexpr bool are_compatible_v = std::is_same_v<std::remove_cvref_t<LabelMapOf<__TreeA>>, std::remove_cvref_t<LabelMapOf<__TreeB>>>;

}
