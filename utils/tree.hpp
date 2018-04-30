
#pragma once

#include <vector>
#include "utils.hpp"
#include "types.hpp"
#include "label_iter.hpp"
#include "except.hpp"

namespace TC{

#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!

#define NODE_TYPE_LEAF 0x00
#define NODE_TYPE_TREE 0x01

  struct NeighborList{
    uint32_t* start;
    uint32_t count;

    // convenience functions to not have to write .start[] all the time
    uint32_t& operator[](const uint32_t i) { return start[i]; }
    const uint32_t& operator[](const uint32_t i) const { return start[i]; }

    /* for now, we assume that the neighborlists are sorted, so removal or addition is not supported
    // removing an item from the neighbor list
    bool remove_vertex(const uint32_t vertex){
      for(uint32_t i = 0; i < count; ++i)
        if(start[i] == vertex) {
          remove_index(i);
          return true;
        }
      return false;
    }
    void remove_index(const uint32_t index){
      assert(index < count);
      --count;
      start[index] = start[count];
    }
    */
  };

  struct AnyVertex{
    NeighborList succ;

    bool is_leaf() const
    {
      return (succ.count == 0);
    }
    unsigned char get_type() const
    {
      return is_leaf() ? NODE_TYPE_LEAF : NODE_TYPE_TREE;
    }
    bool is_bifurcating() const
    {
      return succ.count == 2;
    }
  };

  struct TreeVertex: public AnyVertex{
    uint32_t pred;
  };

  // a tree consists of a list of vertices, each having out--neighbors and one in-neighbor
  // the out-neighbor list are represented by a pointer into an out-neighbor list that is handled by the network
  template<typename VertexT = TreeVertex>
  class TreeT {
  public:
    typedef VertexT Vertex;
  protected:
    const NameVec& names;
    Vertex* vertices = nullptr;
    uint32_t* const edges;        // < heads of all edges
    uint32_t num_vertices;
    uint32_t num_edges;

    uint32_t root;
    IndexVec leaves;

    bool is_sorted;
    uint32_t max_outdeg;

    //! add an edge
    //NOTE: edges are supposed to be added in sorted order
    void add_edge(const uint32_t u_idx, const uint32_t v_idx)
    {
      assert(u_idx < num_vertices);
      assert(v_idx < num_vertices);
      if(u_idx > v_idx) is_sorted = false;

      Vertex& u = vertices[u_idx];
      Vertex& v = vertices[v_idx];

      std::cout << "adding edge "<<u_idx<<"->"<<v_idx<<" (previous successor: "<< (int)(u.succ.count ? u.succ[u.succ.count-1] : -1) <<")"<<std::endl;
      if(u.succ.count && (u.succ[u.succ.count-1] > v_idx)) is_sorted = false; // insert edges in sorted order
      u.succ[u.succ.count++] = v_idx;
      v.pred = u_idx;
    }


  public:
 
    // =============== variable query ======================
    const IndexVec& get_leaves() const
    {
      return leaves;
    }
    const Vertex& get_vertex(const uint32_t u_idx) const
    {
      assert(u_idx < num_vertices);
      return vertices[u_idx];
    }
    const Vertex& operator[](const uint32_t u_idx) const
    {
      assert(u_idx < num_vertices);
      return vertices[u_idx];
    }

    const std::string& get_name(const uint32_t u) const
    {
      assert(u < num_vertices);
      return names.at(u);
    }

    uint32_t get_root() const { return root; }
    uint32_t get_num_vertices() const {return num_vertices;}
    uint32_t get_num_edges() const {return num_edges; }

    LabeledNodeIterFactory<> get_leaves_labeled() const
    {
      return LabeledNodeIterFactory<>(names, leaves.begin(), leaves.end());
    }
    LabeledNodeIterFactory<uint32_t> get_nodes_labeled() const
    {
      return LabeledNodeIterFactory<uint32_t>(names, 0, num_vertices);
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

    void update_max_degrees()
    {
      max_outdeg = 0;
      for(uint32_t u_idx = 0; u_idx < num_vertices; ++u_idx){
        Vertex& u = vertices[u_idx];
        if(u.succ.count > max_outdeg) max_outdeg = u.succ.count;
      }
    }
    
    uint32_t naiveLCA(uint32_t x, uint32_t y) const
    {
      assert(is_preordered());
      assert(x < num_vertices);
      assert(y < num_vertices);
      while(x != y){
        if(x > y) 
          x = vertices[x].pred;
        else
          y = vertices[y].pred;
      }
      return x;
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
      if(sub_root < counter) return false;
      counter = sub_root;
      const Vertex& u = vertices[sub_root];
      for(uint32_t i = 0; i < u.succ.count; ++i)
        if(!is_preordered(u.succ[i], counter)) return false;
      return true;
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

    //! O(log n)-time edge lookup
    bool is_edge_bin_search(const uint32_t u_idx, const uint32_t v_idx) const 
    {
      if(!is_sorted) throw NeedSorted("is_edge_bin_search");
      const Vertex& u(vertices[u_idx]);
      return binary_search(u.succ, v_idx, 0, u.succ.count) != UINT32_MAX;
    }

    //! O(n)-time edge lookup
    bool is_edge_linear(const uint32_t u_idx, const uint32_t v_idx) const 
    {
      const Vertex& u(vertices[u_idx]);
      for(uint32_t x = 0; x < u.succ.count; ++x) 
        if(u.succ[x] == v_idx)
          return true;
      return false;
    }

    bool is_edge(const uint32_t u_idx, const uint32_t v_idx) const 
    {
      if(is_sorted)
        return is_edge_bin_search(u_idx, v_idx);
      else
        return is_edge_linear(u_idx, v_idx);
    }


    // =================== modification ====================




    // ================== construction =====================

    TreeT(const NameVec& _names, const uint32_t _num_edges):
      names(_names),
      vertices((Vertex*)calloc(_names.size(), sizeof(Vertex))),
      edges((uint32_t*)malloc(_num_edges * sizeof(uint32_t))),
      num_vertices(names.size()),
      num_edges(_num_edges),
      is_sorted(true)
    {
    }

    //! read a network from a (not neccessarily sorted) list of pairs of uint32_t (aka edges) in which each vertex is less than num_vertices
    // NOTE: make sure that the indices of the leaves are increasing from left to right (that is, leaves are ordered)!
    //       This is the case for inputs in newick format
    TreeT(const Edgelist& edgelist, const NameVec& _names):
      TreeT(_names, edgelist.size())
    {
      // get memory & initialize class variables
      DEBUG3(std::cout << "constructing tree from "<<num_edges<<" edges" << std::endl);
      assert(num_vertices == num_edges + 1);

      // compute out-degrees
      uint32_t* out_deg = (uint32_t*)calloc(num_vertices, sizeof(uint32_t));
      for(const auto &edge : edgelist){
        const uint32_t u_idx = edge.first;
        assert(u_idx < num_vertices);
        ++out_deg[u_idx];
      }
      // compute start points in the edge list
      uint32_t* e_start = edges;
      for(uint32_t u_idx = 0; u_idx < num_vertices; ++u_idx){
        const uint32_t u_out_deg = out_deg[u_idx];
        Vertex& u = vertices[u_idx];

        u.succ.start = e_start;
        e_start += u_out_deg;
        if(u_out_deg > 0){
          if(u_out_deg > max_outdeg) max_outdeg = u_out_deg;
        } else leaves.push_back(u_idx);
        u.pred = UINT32_MAX;
      }
      free(out_deg);
     
      // actually read the edgelist
      for(const auto &edge : edgelist) add_edge(edge.first, edge.second);

      // finally, find the root
      for(uint32_t u_idx = 0; u_idx < num_vertices; ++u_idx)
        if(vertices[u_idx].pred == UINT32_MAX){
          root = u_idx;
          break;
        }
    }
 
    // =================== destruction ====================
    //! destructor frees everything
    ~TreeT()
    {
      if(vertices != nullptr){
        free(vertices);
        free(edges);
      }
    }

    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u_idx, std::string prefix) const
    {
      std::string name = names[u_idx];
      DEBUG3(name += "[" + std::to_string(u_idx) + "]");
      if(name == "") name = "+";
      

      const Vertex& u = vertices[u_idx];
      os << '-' << name;
      switch(u.succ.count){
        case 0:
          os << std::endl;
          break;
        case 1:
          print_subtree(os, u.succ[0], prefix + std::string(name.length() + 1, ' '));
          break;
        default:
          prefix += std::string(name.length(), ' ') + '|';
          
          print_subtree(os, u.succ[0], prefix);
          for(uint32_t i = 1; i < u.succ.count; ++i){
            os << prefix;
            if(i == u.succ.count - 1)
              prefix.back() = ' ';
            
            print_subtree(os, u.succ[i], prefix);
          }
      }
    }
    
    friend std::ostream& operator<<(std::ostream& os, const TreeT<>& T);
  };

 
  
  typedef TreeT<> Tree;

  std::ostream& operator<<(std::ostream& os, const TreeT<>& T)
  {
    std::string prefix = "";
    T.print_subtree(os, T.root, prefix);
    return os;
  }

}
