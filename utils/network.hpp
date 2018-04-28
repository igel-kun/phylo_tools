
#pragma once

#include <vector>
#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"

namespace TC{

  struct NetworkVertex: public AnyVertex{
    NeighborList pred;

    bool is_reti() const
    {
      return (pred.count > 1) && (succ.count == 1);
    }
    bool is_root() const
    {
      return pred.count == 0;
    }
    bool is_inner_tree() const
    {
      return (pred.count == 1) && (succ.count > 0);
    }
    bool is_isolated() const
    {
      return (succ.count == 0) && (pred.count == 0);
    }
    bool is_leaf() const
    {
      return (succ.count == 0) && (pred.count > 0);
    }

  };

  // a network consists of a list of vertices, each having out- and in-neighbors
  // the out- and in- neighbor lists are represented by pointers into an out- and an in- neighbor list that is handled by the network
  template<typename VertexT = NetworkVertex>
  class NetworkT : public TreeT<VertexT> {
  public:
    typedef VertexT Vertex;
  protected:
    using Parent = TreeT<Vertex>;
    using Parent::vertices;
    using Parent::num_vertices;
    using Parent::edges;
    using Parent::num_edges;
    using Parent::names;
    using Parent::root;
    using Parent::leaves;
    using Parent::max_outdeg;
    using Parent::is_sorted;
    using Parent::operator[];

    uint32_t* rev_edges;    // < tails of all edges
    uint32_t max_indeg;

    //! add an edge
    void add_edge(const uint32_t u_idx, const uint32_t v_idx)
    {
      assert(u_idx < num_vertices);
      assert(v_idx < num_vertices);

      Vertex& u = vertices[u_idx];
      Vertex& v = vertices[v_idx];
      
      if(!v.pred.count && (u_idx > v_idx)) is_sorted = false;  // we want all non-reticulation edges to be monotone for algorithmic pleasure
      if(u.succ.count && (u.succ[u.succ.count-1] > v_idx)) is_sorted = false; // insert edges in sorted order
      u.succ[u.succ.count++] = v_idx;
      if(v.pred.count && (v.pred[v.pred.count-1] > u_idx)) is_sorted = false; // insert edges in sorted order
      v.pred[v.pred.count++] = u_idx;
    }

  public:
 
    // =================== information query ==============

    bool is_bicombining() const {
      return max_indeg <= 2;
    }
    bool is_binary() const {
      return Parent::is_bifurcating() && is_bicombining();
    }

    void update_max_degrees()
    {
      max_outdeg = 0;
      max_indeg = 0;
      for(uint32_t u_idx = 0; u_idx < num_vertices; ++u_idx){
        Vertex& u = vertices[u_idx];
        if(u.succ.count > max_outdeg) max_outdeg = u.succ.count;
        if(u.pred.count > max_indeg)  max_indeg = u.pred.count;
      }
    }

    //! return whether the tree indices are in pre-order (modulo gaps) (they should always be) 
    bool is_preordered(const uint32_t sub_root, uint32_t& counter) const
    {
      const Vertex& u = vertices[sub_root];
      if(u.is_reti()) return true; // edges to reticualtions don't need to abide by the pre-order condition
      if(sub_root < counter) return false;
      counter = sub_root;
      for(uint32_t i = 0; i < u.succ.count; ++i)
        if(!is_preordered(u.succ[i], counter)) return false;
      return true;
    }
    bool is_preordered() const
    {
      uint32_t counter = root;
      return is_preordered(root, counter);
    }

    //! get a list of component roots in preorder
    void get_comp_roots_ordered(IndexVec& comp_roots) const
    {
      if(!is_sorted()) throw NeedSorted("get_comp_roots_ordered");
      for(uint32_t r_idx = 0; r_idx < num_vertices; ++r_idx){
        const Vertex& r = vertices[r_idx];
        if(r.is_inner_tree()){
          const uint32_t v_idx = r.pred[0];
          const Vertex& v = vertices[v_idx];
          if(v.is_reti()) comp_roots.push_back(v_idx);
        }
      }
    }



    // =================== modification ====================

    void remove_leaf(const uint32_t u_idx)
    {
      assert(u_idx < num_vertices);
      Vertex& u = vertices[u_idx];
      assert(u.succ.count == 0);

      // remove u from leaves
      leaves.erase(u_idx);
      // remove edges incoming to u
      for(uint32_t i = 0; i < u.pred.count; ++i)
        vertices[i].succ.remove_vertex(u_idx);
    }


    // ================== construction =====================

    //! read a network from a (not neccessarily sorted) list of pairs of uint32_t (aka edges) in which each vertex is less than num_vertices
    // NOTE: make sure that the indices of the leaves are increasing from left to right (that is, leaves are ordered)!
    //       This is the case for inputs in newick format
    NetworkT(const Edgelist& edgelist, const NameVec& _names):
      Parent(_names, edgelist.size()),
      rev_edges((uint32_t*)malloc(num_edges * sizeof(uint32_t)))
    {
      // get memory & initialize class variables
      DEBUG3(std::cout << "constructing network from "<<num_edges<<" edges" << std::endl);
      assert(num_vertices <= num_edges + 1);

      // compute out-degrees
      uint32_t* out_deg = (uint32_t*)calloc(num_vertices, sizeof(uint32_t));
      uint32_t*  in_deg = (uint32_t*)calloc(num_vertices, sizeof(uint32_t));
      for(const auto &edge : edgelist){
        const uint32_t u_idx = edge.first;
        const uint32_t v_idx = edge.second;
        assert(u_idx < num_vertices);
        assert(v_idx < num_vertices);
        ++out_deg[u_idx];
        ++in_deg[v_idx];
      }
      // compute start points in the edge list
      uint32_t* e_start = edges;
      uint32_t* r_start = rev_edges;
      for(uint32_t u_idx = 0; u_idx < num_vertices; ++u_idx){
        Vertex& u = vertices[u_idx];
        const uint32_t u_out_deg = out_deg[u_idx];

        u.succ.start = e_start;
        e_start += u_out_deg;
        if(u_out_deg > 0){
          if(u_out_deg > max_outdeg) max_outdeg = u_out_deg;
        } else leaves.push_back(u_idx);

        const uint32_t u_in_deg = in_deg[u_idx];
        u.pred.start = r_start;
        r_start += u_in_deg;
        if(u_in_deg > 0){
          if(u_in_deg > max_indeg) max_indeg = u.pred.count;
        } else root = u_idx;
      }
      free(in_deg);
      free(out_deg);
     
      // actually read the edgelist
      for(const auto &edge : edgelist) add_edge(edge.first, edge.second);
    }
 
    // =================== destruction ====================
    //! destructor frees everything
    ~NetworkT()
    {
      if(vertices != nullptr){
        free(rev_edges);
      }
    }


    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u_idx, std::string prefix, IndexSet* seen = nullptr) const
    {
      const bool inited_seen = (seen == nullptr);
      if(inited_seen) seen = new IndexSet();

      std::string name = names[u_idx];
      DEBUG3(name += "[" + std::to_string(u_idx) + "]");
      if(name == "") name = "+";
      
      const Vertex& u = vertices[u_idx];
      os << '-' << name;

      if(!u.is_reti() || !contains(*seen, u_idx)){
        if(u.is_reti()) seen->insert(u_idx);
        switch(u.succ.count){
          case 0:
            os << std::endl;
            break;
          case 1:
            print_subtree(os, u.succ[0], prefix + std::string(name.length() + 1, ' '), seen);
            break;
          default:
            prefix += std::string(name.length(), ' ') + '|';
            
            print_subtree(os, u.succ[0], prefix, seen);
            for(uint32_t i = 1; i < u.succ.count; ++i){
              os << prefix;
              if(i == u.succ.count - 1)
                prefix.back() = ' ';
              
              print_subtree(os, u.succ[i], prefix, seen);
            }
        }
      } else os << std::endl;
      if(inited_seen) delete seen;
    }

    friend std::ostream& operator<<(std::ostream& os, const NetworkT<>& N);
    friend class TreeComponentInfo;
  };


  
  typedef NetworkT<> Network;


  std::ostream& operator<<(std::ostream& os, const Network& N)
  {
    std::string prefix = "";
    N.print_subtree(os, N.root, prefix);
    return os;
  }

  /*
  std::ostream& operator<<(std::ostream& os, const Network& N)
  {
    for(uint32_t i = 0; i < N.num_vertices; ++i){
      const Network::Vertex& u = N.vertices[i];
      for(uint32_t j = 0; j < u.succ.count; ++j)
        os << i << " " << u.succ[j] << std::endl;
    }
    return os;
  }
*/

}
