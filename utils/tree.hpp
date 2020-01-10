
#pragma once

#include <vector>
#include "utils.hpp"
#include "iter_bitset.hpp"
#include "types.hpp"
#include "label_iter.hpp"
#include "dfs.hpp"
#include "except.hpp"
#include "edge.hpp"
#include "set_interface.hpp"
#include "storage_edge_growing.hpp"
#include "storage_edge_nongrowing.hpp"
#include "storage_adj_growing.hpp"
#include "storage_adj_nongrowing.hpp"

namespace PT{

#define NODE_TYPE_LEAF 0x00
#define NODE_TYPE_TREE 0x01
#define NODE_TYPE_RETI 0x02
#define NODE_TYPE_ISOL 0x03

#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!


  template<class _EdgeStorage = GrowingTreeAdjacencyStorage<>,
           class _NodeList = IndexSet,
           class _NodeData = void*,
           class _EdgeData = void*,
           bool _is_tree_type = true>
  class _Tree 
  {
  public:
    using NodeList = _NodeList;
    using EdgeStorage = _EdgeStorage;

    using Node = typename _NodeList::value_type;
    using Edge = typename _EdgeStorage::Edge;

  protected:

    const NameVec& names;

    _NodeList nodes;
    _NodeList leaves;
    _EdgeStorage edges;
    
    std::unordered_map<Node, _NodeData> node_data;
    std::unordered_map<Edge, _EdgeData> edge_data;

  public:
    using NodeIterator = typename _NodeList::iterator;
    using ConstNodeIterator = typename _NodeList::const_iterator;
    
    using OutEdgeContainer =      typename _EdgeStorage::OutEdgeContainer;
    using ConstOutEdgeContainer = typename _EdgeStorage::ConstOutEdgeContainer;
    using SuccContainer =      typename _EdgeStorage::SuccContainer;
    using ConstSuccContainer = typename _EdgeStorage::ConstSuccContainer;
    using PredContainer =      typename _EdgeStorage::PredContainer;
    using ConstPredContainer = typename _EdgeStorage::ConstPredContainer;


    // =============== variable query ======================
    const _NodeList& get_leaves() const { return leaves; }
    const _NodeList& get_nodes() const { return nodes; }
    const _EdgeStorage& get_edges() const { return edges; }
    const NameVec& get_names() const { return names; }
    const std::string& get_name(const uint32_t u) const { return names.at(u); }
    const _NodeData& get_data(const uint32_t u_idx) const { return node_data.at(u_idx); }
    const _NodeData& operator[](const uint32_t u_idx) const { return node_data.at(u_idx); }
    _NodeData& operator[](const uint32_t u_idx) { return node_data.at(u_idx); }
    ConstNodeIterator  begin() const { return nodes.begin(); }
    ConstNodeIterator end() const { return nodes.end(); }
    NodeIterator begin() { return nodes.begin(); }
    NodeIterator end() { return nodes.end(); }

    Node root() const { return edges.root(); }
    uint32_t num_nodes() const {return nodes.size();}
    uint32_t num_edges() const {return edges.size();}

    //inline std::vector<Node> get_nodes_preorder(const Node u = root()) const { return tree_traversal<preorder>(*this, u); }
    //inline std::vector<Node> get_nodes_postorder(const Node u = root()) const { return tree_traversal<postorder>(*this, u); }
    //inline std::vector<Node> get_nodes_inorder(const Node u = root()) const { return tree_traversal<inorder>(*this, u); }

    // Why oh why, C++, can I not initialize with non-static members/methods???


    // list all nodes below u in order o (preorder, inorder, postorder)
    template<TraversalType o = postorder, class _Container = std::vector<Node>>
    inline _Container get_nodes_below(const Node u) const
    { return node_traversal<o, !_is_tree_type>(*this, u); }
    template<TraversalType o = postorder, class _Container = std::vector<Node>>
    inline _Container get_nodes() const
    { return node_traversal<o, !_is_tree_type>(*this, root()); }
#define get_nodes_postorder template get_nodes<postorder>
#define get_nodes_preorder template get_nodes<preorder>
#define get_nodes_inorder template get_nodes<inorder>
#define get_nodes_below_postorder template get_nodes_below<postorder>
#define get_nodes_below_preorder template get_nodes_below<preorder>
#define get_nodes_below_inorder template get_nodes_below<inorder>

    // list all nodes below u in order o (preorder, inorder, postorder) avoiding the nodes given in except
    template<TraversalType o = postorder, class _Container = std::vector<Node>, class _ExceptContainer = std::unordered_set<Node>>
    inline _Container get_nodes_below_except(const _ExceptContainer& except, const Node u) const
    { return node_traversal<o>(*this, except, u); }
    template<TraversalType o = postorder, class _Container = std::vector<Node>, class _ExceptContainer = std::unordered_set<Node>>
    inline _Container get_nodes_except(const _ExceptContainer& except) const
    { return node_traversal<o>(*this, except, root()); }
#define get_nodes_except_postorder template get_nodes_except<postorder>
#define get_nodes_except_preorder template get_nodes_except<preorder>
#define get_nodes_except_inorder template get_nodes_except<inorder>
#define get_nodes_below_except_postorder template get_nodes_below_except<postorder>
#define get_nodes_below_except_preorder template get_nodes_below_except<preorder>
#define get_nodes_below_except_inorder template get_nodes_below_except<inorder>

  
    // list all edges below u in order o (preorder, inorder, postorder)
    template<TraversalType o = postorder, class _Container = std::vector<Edge>>
    inline _Container get_edges_below(const Node u) const
    { return edge_traversal<o, !_is_tree_type>(*this, u); }
    template<TraversalType o = postorder, class _Container = std::vector<Edge>>
    inline _Container get_edges() const
    { return edge_traversal<o, !_is_tree_type>(*this, root()); }
#define get_edges_postorder template get_edges<postorder>
#define get_edges_preorder template get_edges<preorder>
#define get_edges_inorder template get_edges<inorder>
#define get_edges_below_postorder template get_edges_below<postorder>
#define get_edges_below_preorder template get_edges_below<preorder>
#define get_edges_below_inorder template get_edges_below<inorder>



    // list all edges below u in order o (preorder, inorder, postorder) avoiding the nodes given in except
    template<TraversalType o = postorder, class _Container = std::vector<Edge>, class _ExceptContainer = std::unordered_set<Node>>
    inline _Container get_edges_below_except(const _ExceptContainer& except, const Node u) const
    { return edge_traversal<o>(*this, except, u); }
    template<TraversalType o = postorder, class _Container = std::vector<Edge>, class _ExceptContainer = std::unordered_set<Node>>
    inline _Container get_edges_except(const _ExceptContainer& except) const
    { return edge_traversal<o>(*this, except, root()); }
#define get_edges_except_postorder template get_edges_except<postorder>
#define get_edges_except_preorder template get_edges_except<preorder>
#define get_edges_except_inorder template get_edges_except<inorder>
#define get_edges_below_except_postorder template get_edges_below_except<postorder>
#define get_edges_below_except_preorder template get_edges_below_except<preorder>
#define get_edges_below_except_inorder template get_edges_below_except<inorder>



    LabeledNodeIterFactory<> get_leaves_labeled() const
    {
      return get_labeled_nodes(names, leaves);
    }
    LabeledNodeIterFactory<uint32_t> get_nodes_labeled() const
    {
      return LabeledNodeIterFactory<uint32_t>(names, 0, nodes.size());
    }



    // =================== information query ==============

    bool empty() const { return nodes.empty(); }
    Node out_degree(const Node u) const { return edges.out_degree(u); }
    bool is_leaf(const Node u) const { return out_degree(u) == 0; }
    bool is_root(const Node u) const { return u == root(); }
    bool is_tree_node(const Node u) const { return out_degree(u) > 0; }

    Node type_of(const Node u) const
    {
      if(is_leaf(u)) return NODE_TYPE_LEAF;
      if(is_tree_node(u)) return NODE_TYPE_TREE;
      return NODE_TYPE_ISOL;
    }


    ConstPredContainer parents(Node u) const { return edges.predecessors(u); }
    ConstSuccContainer children(Node u) const { return edges.successors(u); }
    ConstOutEdgeContainer out_edges(Node u) const {return edges.const_out_edges(u); }
    ConstOutEdgeContainer const_out_edges(Node u) const {return edges.const_out_edges(u); }
    
    Node parent(Node u) const { return std::front(parents(u)); }

    Node naiveLCA_preordered(Node x, Node y) const
    {
      assert(is_preordered());
      assert(x < nodes.size());
      assert(y < nodes.size());
      while(x != y){
        if(x > y) 
          x = parent(x);
        else
          y = parent(y);
      }
      return x;
    }
    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    uint32_t naiveLCA(uint32_t x, uint32_t y) const
    {
      assert(x < nodes.size());
      assert(y < nodes.size());
      std::unordered_bitset seen(num_nodes());
      while(x != y){
        if(update_for_LCA(seen, x)) return x;
        if(update_for_LCA(seen, y)) return y;
      }
    }
  protected:
    // helper function for the LCA
    bool update_for_LCA(std::unordered_bitset& seen, uint32_t& z) const
    {
      if((z != root) && !test(seen,z)){
        seen.insert(z);
        z = parent(z);
        return test(seen,z);
      } else return false;
    }
  public:

    uint32_t LCA(const uint32_t x, const uint32_t y) const
    {
#warning TODO: use more efficient LCA
      return naiveLCA(x,y);
    }

    //! return if there is a directed path from x to y in the tree; requires the tree to be pre-ordered
    bool has_path(uint32_t x, const uint32_t y) const
    {
      if(x < y) return false;
      while(1){
        if(x == y) return true;
        if(x == root) return false;
        x = parent(x);
      } 
    }

    // return the descendant among x and y unless they are incomparable; return UINT32_MAX in this case
    uint32_t get_minimum(const uint32_t x, const uint32_t y) const
    {
      const uint32_t lca = LCA(x,y);
      if(lca == x) return y;
      if(lca == y) return x;
      return UINT32_MAX;
    }


    //! return whether the tree indices are in pre-order (modulo gaps) (they should always be) 
    bool is_preordered(const uint32_t sub_root, uint32_t& counter) const
    {
      if(sub_root >= counter) {
        counter = sub_root;
        for(uint32_t v: children(sub_root))
          if(!is_preordered(v, counter)) return false;
        return true;
      } else return false;
    }

    bool is_preordered() const
    {
      uint32_t counter = root;
      return is_preordered(root, counter);
    }

    bool is_multi_labeled() const
    {
      std::unordered_set<std::string> seen;
      for(const uint32_t u_idx: leaves){
        const std::string& _name = names[u_idx];
        if(contains(seen, _name)) return true;
      }
      return false;
    }

    bool is_edge(const uint32_t u, const uint32_t v) const 
    {
      return test(edges.out_edges(u), v);
    }

    //! for sanity checks: test if there is a directed cycle in the data structure (more useful for networks, but definable for trees too)
    bool has_cycle() const
    {
      if(!empty()){
        //need 0-initialized array
        uint32_t* depth_at = (uint32_t*)calloc(nodes.size(), sizeof(uint32_t));
        const bool result = has_cycle(root, depth_at, 1);
        free(depth_at);
        return result;
      } else return false;
    }

  protected:
    bool has_cycle(const uint32_t sub_root, uint32_t* depth_at, const uint32_t depth) const
    {
      if(depth_at[sub_root] == 0){
        depth_at[sub_root] = depth;
        for(uint32_t w: children(sub_root))
          if(has_cycle(w, depth_at, depth + 1)) return true;
        depth_at[sub_root] = UINT32_MAX; // mark as seen and not cyclic
        return false;
      } else return (depth_at[sub_root] <= depth);
    }
  public:
    // =================== modification ====================

    //! add an edge
    bool add_edge(const Edge& e)
    {
      return edges.add_edge(e);
    }

    bool remove_edge(const Edge& e)
    {
      return edges.remove_edge(e);
    }

    void remove_node(const Node u)
    {
      leaves.erase(u);
      edges.remove_node(u);
    }

    void remove_node(const Node u) const
    {
      assert(false && "cannot remove node from const tree/network");
    }

    template<class __Tree = _Tree<_EdgeStorage, _NodeList, _NodeData>>
    __Tree remove_rooted_subtree(const Node u) const
    {
      const std::vector<Edge> el = remove_edges_below(u);
      return {names, el};
    }
    // copy the subtree rooted at u into t, but ignore subtrees rooted at nodes in 'except'
    template<class __Tree = _Tree<_EdgeStorage, _NodeList, _NodeData>, class _NodeContainer>
    __Tree remove_rooted_subtree_except(const Node u, const _NodeContainer& except) const
    {
      const std::vector<Edge> el = remove_edges_below_except(except, u);
      return {names, el};
    }


    // copy the subtree rooted at u into t
    template<class __Tree = _Tree<_EdgeStorage, _NodeList, _NodeData>>
    __Tree get_rooted_subtree(const Node u) const
    {
      const std::vector<Edge> el = get_edges_below(u);
      return {names, el};
    }
    // copy the subtree rooted at u into t, but ignore subtrees rooted at nodes in 'except'
    template<class __Tree = _Tree<_EdgeStorage, _NodeList, _NodeData>, class _NodeContainer>
    __Tree get_rooted_subtree_except(const Node u, const _NodeContainer& except) const
    {
      const std::vector<Edge> el = get_edges_below_except(u, except);
      return {names, el};
    }


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
      DEBUG3(std::cout << "tree has "<<edges.size()<<" edges and "<<nodes.size()<<" nodes, "<<leaves.size()<<" leaves: "<<leaves<<std::endl);

      for(uint32_t i: nodes){
        std::cout << i << ":";
        std::cout << " IN: "<< edges.in_edges(i);
        std::cout << " OUT: "<<edges.out_edges(i) << std::endl;
      }

    }

    // read all nodes and prepare the edge storage to receive edges; return the root
    template<class EdgeContainer = std::vector<Edge>>
    _Tree(const consecutive_edgelist_tag& tag, const EdgeContainer& given_edges, const NameVec& _names):
      names(_names),
      nodes(),
      leaves(),
      edges(tag, given_edges, _names.size(), &leaves)
    {
      DEBUG3(std::cout << "init Tree with consecutive EdgeContainer "<<given_edges<<std::endl<<" and "<<_names.size()<<" names: "<<_names<<std::endl);
      
      for(uint32_t i = 0; i < _names.size(); ++i) append(nodes, i);
      tree_summary(std::cout);
    }

    // read all nodes and prepare the edge storage to receive edges; return the root
    template<class EdgeContainer = std::vector<Edge>>
    _Tree(const EdgeContainer& given_edges, const NameVec& _names):
      names(_names),
      nodes(),
      leaves(),
      edges(given_edges, nodes, &leaves)
    {
      DEBUG3(std::cout << "init Tree with non-consecutive EdgeContainer "<<given_edges<<std::endl<<" and "<<_names.size()<<" names: "<<_names<<" and "<<nodes.size()<<" nodes: "<<nodes<<std::endl);
      tree_summary(std::cout);
    }

    // initialize a subtree rooted at a node of the given tree
    template<class __Tree = _Tree<_EdgeStorage, _NodeList, _NodeData>>
    _Tree(const __Tree& supertree, const NameVec& _names, const typename __Tree::Node _root):
      names(_names),
      nodes(),
      leaves(),
      edges(supertree.get_edges_below(_root), &nodes, &leaves)
    {
    }



  protected:
    _Tree(const NameVec& _names):
      names(_names)
    {}

  public:


    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u, std::string prefix) const
    {
      std::string name = names.at(u);
      DEBUG3(name += "[" + std::to_string(u) + "]");
      if(name == "") name = "+";
      os << '-' << name;

      const auto& u_childs = children(u);
      switch(u_childs.size()){
        case 0:
          os << std::endl;
          break;
        case 1:
          print_subtree(os, front(u_childs), prefix + std::string(name.length() + 1u, ' '));
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


  template<class _EdgeStorage = GrowingTreeAdjacencyStorage<>,
           class _NodeList = IndexSet,
           class _NodeData = void*,
           class _EdgeData = void*>
  using Tree = _Tree<_EdgeStorage, _NodeList, _NodeData, _EdgeData, true>;


  template<class _EdgeStorage = NonGrowingTreeAdjacencyStorage<>,
           class _NodeList = IndexVec,
           class _NodeData = void*,
           class _EdgeData = void*>
  using ROTree = _Tree<_EdgeStorage, _NodeList, _NodeData, _EdgeData, true>;

  
  template<class _EdgeStorage = NonGrowingTreeAdjacencyStorage<>,
           class _NodeList = IndexVec,
           class _NodeData = void*,
           class _EdgeData = void*>
  std::ostream& operator<<(std::ostream& os, const _Tree<_EdgeStorage, _NodeList, _NodeData, _EdgeData, true>& T)
  {
    if(!T.empty()){
      std::string prefix = "";
      T.print_subtree(os, T.root(), prefix);
    } else os << "{}";
    return os;
  }



}
