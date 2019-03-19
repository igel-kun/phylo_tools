
#pragma once

#include <vector>
#include "utils.hpp"
#include "iter_bitset.hpp"
#include "types.hpp"
#include "label_iter.hpp"
#include "except.hpp"
#include "node.hpp"
#include "edge.hpp"
#include "set_interface.hpp"
#include "edge_storage.hpp"

namespace PT{

#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!

  // this is a virtual prototype for a tree that can be made mutable or immutable
  template<class _Edge = Edge,
           class _Node = TreeNodeT<_Edge>,
           class _NodeList = std::vector<_Node>,
           class _EdgeStorage>
  class ProtoTreeT 
  {
  public:
    typedef _Node Node;
    typedef _Edge Edge;
    typedef typename _Node::SuccList SuccList;
    typedef typename SuccList::iterator EdgeIter;

  protected:

    const NameVec& names;
    _NodeList nodes;
    uint32_t _num_edges;

    uint32_t root;
    IndexVec leaves;

    uint32_t max_outdeg;

    //! add an edge
    virtual void add_edge(const Edge& e) = 0;

  public:
 
    // =============== variable query ======================
    const IndexVec& get_leaves() const
    {
      return leaves;
    }
    const _NodeList& get_nodes() const
    {
      return nodes;
    }
    const _Node& get_node(const uint32_t u_idx) const
    {
      return nodes.at(u_idx);
    }
    const NameVec& get_names() const
    {
      return names;
    }
    const _Node& operator[](const uint32_t u_idx) const
    {
      return nodes.at(u_idx);
    }
    _Node& operator[](const uint32_t u_idx)
    {
      assert(u_idx < nodes.size());
      return nodes[u_idx];
    }
    typename _NodeList::const_iterator begin() const { return nodes.begin(); }
    typename _NodeList::const_iterator end() const { return nodes.end(); }
    typename _NodeList::iterator begin() { return nodes.begin(); }
    typename _NodeList::iterator end() { return nodes.end(); }

    const std::string& get_name(const uint32_t u) const
    {
      return names.at(u);
    }

    uint32_t get_root() const { return root; }
    uint32_t num_nodes() const {return nodes.size();}
    uint32_t num_edges() const {return _num_edges; }

    LabeledNodeIterFactory<> get_leaves_labeled() const
    {
      return LabeledNodeIterFactory<>(names, leaves.begin(), leaves.end());
    }
    LabeledNodeIterFactory<uint32_t> get_nodes_labeled() const
    {
      return LabeledNodeIterFactory<uint32_t>(names, 0, nodes.size());
    }


    // =================== information query ==============

    bool is_bifurcating() const 
    {
      return max_outdeg <= 2;
    }
    
    bool is_binary() const 
    {
      return is_bifurcating();
    }

    bool empty() const
    {
      return nodes.empty();
    }

    void update_max_degrees()
    {
      max_outdeg = 0;
      for(const _Node& u : nodes)
        max_outdeg = std::max(max_outdeg, u.out.size());
    }
    
    uint32_t naiveLCA_preordered(uint32_t x, uint32_t y) const
    {
      assert(is_preordered());
      assert(x < nodes.size());
      assert(y < nodes.size());
      while(x != y){
        if(x > y) 
          x = nodes[x].parent();
        else
          y = nodes[y].parent();
      }
      return x;
    }
    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    uint32_t naiveLCA(uint32_t x, uint32_t y) const
    {
      assert(x < nodes.size());
      assert(y < nodes.size());
      std::iterable_bitset seen(num_nodes());
      while(x != y){
        if(update_for_LCA(seen, x)) return x;
        if(update_for_LCA(seen, y)) return y;
      }
    }
  protected:
    // helper function for the LCA
    bool update_for_LCA(std::iterable_bitset& seen, uint32_t& z) const
    {
      if((z != root) && !test(seen,z)){
        seen.insert(z);
        z = nodes[z].parent();;
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
        x = nodes[x].parent();
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
        for(uint32_t v: nodes[sub_root].children())
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
      return test(nodes[u].out, v);
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
        for(uint32_t w: nodes[sub_root].children())
          if(has_cycle(w, depth_at, depth + 1)) return true;
        depth_at[sub_root] = UINT32_MAX; // mark as seen and not cyclic
        return false;
      } else return (depth_at[sub_root] <= depth);
    }
  public:
    // =================== modification ====================




    // ================== construction =====================

    ProtoTreeT(const NameVec& _names, const uint32_t edgenum):
      names(_names),
      nodes(),
      _num_edges(edgenum)
    {}

    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u_idx, std::string prefix) const
    {
      std::string name = names[u_idx];
      DEBUG3(name += "[" + std::to_string(u_idx) + "]");
      if(name == "") name = "+";
      os << '-' << name;

      const _Node& u = nodes[u_idx];
      switch(u.out.size()){
        case 0:
          os << std::endl;
          break;
        case 1:
          print_subtree(os, u.out[0].head(), prefix + std::string(name.length() + 1, ' '));
          break;
        default:
          prefix += std::string(name.length(), ' ') + '|';
          
          print_subtree(os, u.out[0].head(), prefix);
          for(uint32_t i = 1; i < u.out.size(); ++i){
            os << prefix;
            if(i == u.out.size() - 1)
              prefix.back() = ' ';
            
            print_subtree(os, u.out[i].head(), prefix);
          }
      }
    }
    
  };

  
  std::ostream& operator<<(std::ostream& os, const ProtoTreeT<>& T)
  {
    if(!T.empty()){
      std::string prefix = "";
      T.print_subtree(os, T.get_root(), prefix);
    }
    return os;
  }

}
