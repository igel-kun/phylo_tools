
#pragma once

#include <vector>
#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"

namespace PT{

  // a network consists of a list of nodes, each having out- and in-neighbors
  // the out- and in- neighbor lists are represented by pointers into an out- and an in- neighbor list that is handled by the network
  template<typename NodeT = NetworkNode<SortedFixNeighborList, SortedFixNeighborList>>
  class NetworkT : public TreeT<NodeT> 
  {
  public:
    typedef NodeT Node;
  protected:
    using Parent = TreeT<Node>;
    using Parent::nodes;
    using Parent::edges;
    using Parent::num_edges;
    using Parent::names;
    using Parent::root;
    using Parent::leaves;
    using Parent::max_outdeg;

    uint32_t* rev_edges;    // < tails of all edges
    uint32_t max_indeg;

    //! add an edge
    void add_edge(const uint32_t u_idx, const uint32_t v_idx)
    {
      assert(u_idx < nodes.size());
      assert(v_idx < nodes.size());

      Node& u = nodes[u_idx];
      Node& v = nodes[v_idx];
      
      u.succ[u.succ.count++] = v_idx;
      v.pred[v.pred.count++] = u_idx;
    }

  public:
    using Parent::operator[];
    using Parent::has_cycle;

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
      for(Node& u: nodes){
        if(u.succ.size() > max_outdeg) max_outdeg = u.succ.size();
        if(u.pred.size() > max_indeg)  max_indeg = u.pred.size();
      }
    }

    //! return whether the tree indices are in pre-order (modulo gaps) (they should always be) 
    bool is_preordered(const uint32_t sub_root, uint32_t& counter) const
    {
      const Node& u = nodes[sub_root];
      if(u.is_reti()) return true; // edges to reticualtions don't need to abide by the pre-order condition
      if(sub_root < counter) return false;
      counter = sub_root;
      for(uint32_t v: u.succ)
        if(!is_preordered(v, counter)) return false;
      return true;
    }
    bool is_preordered() const
    {
      uint32_t counter = root;
      return is_preordered(root, counter);
    }

    //! get a list of component roots in preorder
    void get_comp_roots(IndexVec& comp_roots) const
    {
      for(Node& r: nodes){
        if(r.is_inner_tree()){
          const uint32_t v_idx = r.pred[0];
          const Node& v = nodes[v_idx];
          if(v.is_reti()) comp_roots.push_back(v_idx);
        }
      }
    }

    // =================== modification ====================

    void remove_node(const uint32_t u_idx)
    {
      assert(u_idx < nodes.size());
      Node& u = nodes[u_idx];

      // remove u from leaves
      if(u.is_leaf()) leaves.erase(u_idx);
      // remove edges incoming to u
      for(uint32_t v: u.pred)
        nodes[v].succ.remove(u_idx);
      for(uint32_t v: u.succ)
        nodes[v].pred.remove(u_idx);
    }


    // ================== construction =====================

    //! read a network from a (not neccessarily sorted) list of pairs of uint32_t (aka edges) in which each vertex is less than num_nodes
    // NOTE: make sure that the indices of the leaves are increasing from left to right (that is, leaves are ordered)!
    //       This is the case for inputs in newick format
    template<class EdgeContainer = EdgeVec>
    NetworkT(const EdgeContainer& edgelist, const NameVec& _names, const uint32_t num_nodes, const bool check_cyclic = true):
      Parent(_names, edgelist.size()),
      rev_edges((uint32_t*)malloc(num_edges * sizeof(uint32_t)))
    {
      // get memory & initialize class variables
      DEBUG3(std::cout << "constructing network from "<<num_edges<<" edges" << std::endl);
      assert(num_nodes <= num_edges + 1);

      // compute out-degrees
      uint32_t* out_deg = (uint32_t*)calloc(num_nodes, sizeof(uint32_t));
      uint32_t*  in_deg = (uint32_t*)calloc(num_nodes, sizeof(uint32_t));
      for(const auto &edge : edgelist){
        const uint32_t u_idx = edge.first;
        const uint32_t v_idx = edge.second;
        assert(u_idx < num_nodes);
        assert(v_idx < num_nodes);
        ++out_deg[u_idx];
        ++in_deg[v_idx];
      }
      // compute start points in the edge list
      uint32_t* e_start = edges;
      uint32_t* r_start = rev_edges;
      for(uint32_t u_idx = 0; u_idx < num_nodes; ++u_idx){
        const uint32_t u_out_deg = out_deg[u_idx];
        nodes.emplace_back();
        Node& u = nodes[u_idx];

        u.succ.start = e_start;
        e_start += u_out_deg;
        if(u_out_deg > 0){
          if(u_out_deg > max_outdeg) max_outdeg = u_out_deg;
        } else leaves.push_back(u_idx);

        const uint32_t u_in_deg = in_deg[u_idx];
        u.pred.start = r_start;
        r_start += u_in_deg;
        if(u_in_deg > 0){
          if(u_in_deg > max_indeg) max_indeg = u.pred.size();
        } else root = u_idx;
      }
      free(in_deg);
      free(out_deg);
     
      // actually read the edgelist
      for(const auto &edge : edgelist) add_edge(edge.first, edge.second);
      // finally, if requested, check if N is acyclic
      if(check_cyclic && has_cycle()) throw std::logic_error("network contains a cycle");
    }
    template<class EdgeContainer = EdgeVec>
    NetworkT(const EdgeContainer& edgelist, const NameVec& _names):
      NetworkT(edgelist, _names, _names.size())
    {}

    // =================== destruction ====================
    //! destructor frees everything
    ~NetworkT()
    {
      if(rev_edges) free(rev_edges);
    }


    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u_idx, std::string prefix, std::iterable_bitset* seen = nullptr) const
    {
      const bool inited_seen = (seen == nullptr);
      if(inited_seen) seen = new std::iterable_bitset(nodes.size());

      std::string name = names[u_idx];
      const Node& u = nodes[u_idx];
      
      if(name == "") name = (u.is_reti()) ? std::string("(R" + std::to_string(u_idx) + ")") : std::string("+");
      DEBUG3(name += "[" + std::to_string(u_idx) + "]");

      os << '-' << name;
      
      if(!u.is_reti() || !seen->test(u_idx)){
        if(u.is_reti()) seen->insert(u_idx);
        switch(u.succ.size()){
          case 0:
            os << std::endl;
            break;
          case 1:
            print_subtree(os, u.succ[0], prefix + std::string(name.length() + 1, ' '), seen);
            break;
          default:
            prefix += std::string(name.length(), ' ') + '|';
            
            print_subtree(os, u.succ[0], prefix, seen);
            for(uint32_t i = 1; i < u.succ.size(); ++i){
              os << prefix;
              if(i + 1 == u.succ.size())
                prefix.back() = ' ';
              
              print_subtree(os, u.succ[i], prefix, seen);
            }
        }
      } else os << std::endl;
      if(inited_seen) delete seen;
    }

    friend class TreeComponentInfo;
  };


  
  typedef NetworkT<> Network;


  std::ostream& operator<<(std::ostream& os, const Network& N)
  {
    if(!N.empty()){
      std::string prefix = "";
      N.print_subtree(os, N.get_root(), prefix);
    }
    return os;
  }


}
