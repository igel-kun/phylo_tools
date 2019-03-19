
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

namespace PT{

#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!

  // a tree consists of a list of nodes, each having out-neighbors and one in-neighbor
  template<class _Edge = Edge,
           class _Node = TreeNodeT<_Edge>,
           class _NodeList = std::vector<_Node>>
  class MutableTreeT 
  {
  public:
    typedef _Node Node;
    typedef _Edge Edge;
    typedef typename _Node::SuccList SuccList;
    typedef typename SuccList::iterator EdgeIter;

  protected:

    const NameVec& names;
    _NodeList nodes;
    std::list<Edge> edges;

    uint32_t root;
    IndexVec leaves;

    uint32_t max_outdeg;

    //! add an edge
    void add_edge(const Edge& e)
    {
      assert(e.head() < nodes.size());
      assert(e.tail() < nodes.size());

      _Node& u = nodes[e.tail()];
      _Node& v = nodes[e.head()];

      const typename _Node::SuccList::iterator it = u.out.emplace_back(e);
      v.in = it;
    }

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
    // helper function for the LCA
    bool update_for_LCA(std::iterable_bitset& seen, uint32_t& z) const
    {
      if((z != root) && !test(seen,z)){
        seen.insert(z);
        z = nodes[z].parent();;
        return test(seen,z);
      } else return false;
    }
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

    uint32_t LCA(const uint32_t x, const uint32_t y) const
    {
#warning TODO: use more efficient LCA
      return naiveLCA(x,y);
    }

    //! return if there is a directed path from x to y in the tree
    bool has_path(const uint32_t x, const uint32_t y) const
    {
      if(x < y) return false;
      if(x == y) return true;
      return LCA(x,y) == x;
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
      return contains(nodes[u].out, v);
    }

    //! for sanity checks: test if there is a cycle in the data structure (more useful for networks, but definable for trees too)
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
    bool has_cycle(const uint32_t sub_root, uint32_t* depth_at, const uint32_t depth) const
    {
      if(depth_at[sub_root] == 0){
        depth_at[sub_root] = depth;
        for(uint32_t w: nodes[sub_root].children())
          if(has_cycle(w, depth_at, depth + 1)) return true;
        depth_at[sub_root] = UINT32_MAX; // mark as seen and not cyclic
        return false;
      } else return (depth_at[sub_root] < depth);
    }

    // =================== modification ====================




    // ================== construction =====================

    TreeT(const NameVec& _names, const uint32_t edgenum):
      names(_names),
      nodes(),
      edges((Edge*)malloc(edgenum * sizeof(Edge))),
      _num_edges(edgenum)
    {}

    // read all nodes and prepare the edge storage to receive edges; return the root
    template<class EdgeContainer = std::vector<Edge>>
    uint32_t read_nodes_and_prepare_edge_storage(const EdgeContainer& given_edges, const uint32_t num_nodes)
    {
      nodes.reserve(num_nodes);
      // compute out-degrees and the root
      std::iterable_bitset root_poss(num_nodes);
      root_poss.set_all();
      uint32_t* out_deg = (uint32_t*)calloc(num_nodes, sizeof(uint32_t));
      for(const auto &edge: given_edges){
        assert(edge.tail() < num_nodes);
        assert(edge.head() < num_nodes);
        ++out_deg[edge.tail()];
        root_poss.clear(edge.head());
        DEBUG5(std::cout << "treated edge "<<edge<<", root poss: "<<root_poss<<std::endl);
      }
      // compute start points in the edge list
      EdgeIter e_start = edges;
      for(uint32_t u_idx = 0; u_idx < num_nodes; ++u_idx){
        const uint32_t u_outdeg = out_deg[u_idx];
        // emplace a tree node without parent whose outessors start at e_start
        nodes.emplace_back();
        _Node& u = nodes.back();

        u.out.start = e_start;
        std::advance(e_start, u_outdeg);
        if(u_outdeg > 0){
          max_outdeg = std::max(max_outdeg, u_outdeg);
        } else leaves.push_back(u_idx);
      }
      free(out_deg);

      // finally, set the root
      if(root_poss.size() != 1) throw std::logic_error("cannot create tree with "+std::to_string(root_poss.size())+" roots");
      return front(root_poss);
    }

    //! read a tree from a (not neccessarily sorted) list of pairs of uint32_t (aka edges) in which each vertex is less than num_nodes
    // NOTE: make sure that the indices of the leaves are increasing from left to right (that is, leaves are ordered)!
    //       This is the case for inputs in newick format
    template<class EdgeContainer = std::vector<Edge>>
    TreeT(const EdgeContainer& given_edges, const NameVec& _names, const uint32_t num_nodes):
      TreeT(_names, given_edges.size())
    {
      // get memory & initialize class variables
      DEBUG3(std::cout << "constructing tree from "<<_num_edges<<" edges" << std::endl);
      assert(num_nodes == _num_edges + 1);

      root = read_nodes_and_prepare_edge_storage(given_edges, num_nodes);
      // actually read the edges
      for(const auto &e: given_edges) add_edge(e);
    }

    // for some reason gcc wont let me put _names.size() as default value for the last argument, so I have to duplicate code...
    template<class EdgeContainer = EdgeVec>
    TreeT(const EdgeContainer& given_edges, const NameVec& _names):
      TreeT(given_edges, _names, _names.size())
    {}

    // =================== destruction ====================
    //! destructor frees everything
    ~TreeT()
    {
      if(edges) free(edges);
    }

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


}
