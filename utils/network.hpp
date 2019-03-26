
#pragma once

#include "ro_tree.hpp"

namespace PT{

  // a network consists of a list of nodes, each having out- and in-neighbors
  // the out- and in- neighbor lists are represented by pointers into an out- and an in- neighbor list that is handled by the network
  template<class _Edge = Edge,
           class _Node = NetworkNodeT<_Edge>,
           class _NodeList = std::vector<_Node>>
  class ROProtoNetworkT : public ROProtoTreeT<_Edge, _Node, _NodeList> 
  {
    using Parent = ROProtoTreeT<_Edge, _Node, _NodeList>;
  public:
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::SuccList;
    using typename Parent::EdgeIter;
  protected:
    using Parent::names;
    using Parent::nodes;
    using Parent::_num_edges;
    using Parent::root;
    using Parent::leaves;
    using Parent::max_outdeg;
  public:
    using Parent::get_leaves;
    using Parent::get_nodes;
    using Parent::get_node;
    using Parent::get_names;
    using Parent::operator[];
    using Parent::get_name;
    using Parent::begin;
    using Parent::end;
    using Parent::get_root;
    using Parent::num_nodes;
    using Parent::num_edges;
    using Parent::get_leaves_labeled;
    using Parent::get_nodes_labeled;
    using Parent::is_bifurcating;
    using Parent::is_binary;
    using Parent::empty;
    using Parent::update_max_degrees;
    using Parent::LCA;
    using Parent::has_path;
    using Parent::get_minimum;
    using Parent::is_preordered;
    using Parent::is_multi_labeled;
    using Parent::is_edge;
    using Parent::has_cycle;
    using Parent::print_subtree;

  protected:
    uint32_t max_indeg;


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
      for(_Node& u: nodes){
        if(u.out.size() > max_outdeg) max_outdeg = u.out.size();
        if(u.in.size() > max_indeg)  max_indeg = u.in.size();
      }
    }

    //! return whether the tree indices are in pre-order (modulo gaps) (they should always be) 
    bool is_preordered(const uint32_t sub_root, uint32_t& counter) const
    {
      const _Node& u = nodes[sub_root];
      if(u.is_reti()) return true; // edges to reticualtions don't need to abide by the pre-order condition
      if(sub_root < counter) return false;
      counter = sub_root;
      for(uint32_t v: u.out)
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
      for(_Node& r: nodes){
        if(r.is_inner_tree()){
          const uint32_t v_idx = r.in[0];
          const _Node& v = nodes[v_idx];
          if(v.is_reti()) comp_roots.push_back(v_idx);
        }
      }
    }

    // =================== modification ====================

    void remove_node(const uint32_t u_idx)
    {
      assert(u_idx < nodes.size());
      _Node& u = nodes[u_idx];

      // remove u from leaves
      if(u.is_leaf()) leaves.erase(u_idx);
      // remove edges incoming to u
      for(uint32_t v: u.in)
        nodes[v].out.remove(u_idx);
      for(uint32_t v: u.out)
        nodes[v].in.remove(u_idx);
    }


    // ================== construction =====================

    ROProtoNetworkT(const NameVec& _names, const uint32_t _num_nodes):
      Parent(_names, _num_nodes)
    {}

    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u_idx, std::string prefix, std::iterable_bitset* seen = nullptr) const
    {
      const bool inited_seen = (seen == nullptr);
      if(inited_seen) seen = new std::iterable_bitset(nodes.size());

      std::string name = names[u_idx];
      const _Node& u = nodes[u_idx];
      
      if(name == "") name = (u.is_reti()) ? std::string("(R" + std::to_string(u_idx) + ")") : std::string("+");
      DEBUG3(name += "[" + std::to_string(u_idx) + "]");

      os << '-' << name;
      
      if(!u.is_reti() || !seen->test(u_idx)){
        if(u.is_reti()) seen->insert(u_idx);
        switch(u.out.size()){
          case 0:
            os << std::endl;
            break;
          case 1:
            print_subtree(os, u.out[0].head(), prefix + std::string(name.length() + 1, ' '), seen);
            break;
          default:
            prefix += std::string(name.length(), ' ') + '|';
            
            print_subtree(os, u.out[0].head(), prefix, seen);
            for(uint32_t i = 1; i < u.out.size(); ++i){
              os << prefix;
              if(i + 1 == u.out.size())
                prefix.back() = ' ';
              
              print_subtree(os, u.out[i].head(), prefix, seen);
            }
        }
      } else os << std::endl;
      if(inited_seen) delete seen;
    }

    friend class TreeComponentInfo;
  };



  template<class _Edge = Edge,
           class _Node = NetworkNodeT<_Edge>,
           class _NodeList = std::vector<_Node>>
  class NetworkT : public ROProtoNetworkT<_Edge, _Node, _NodeList>
  {
  protected:
    using Parent = ROProtoNetworkT<_Edge, _Node, _NodeList>;
    using typename Parent::EdgeIter;
    using Parent::nodes;
    using Parent::ROProtoNetworkT;
    using Parent::root;
    using Parent::has_cycle;
    using Parent::_num_edges;

    //! add an edge
    void add_edge(const _Edge& e)
    {
      assert(e.head() < nodes.size());
      assert(e.tail() < nodes.size());

      _Node& u = nodes[e.tail()];
      _Node& v = nodes[e.head()];

      const typename _Node::SuccList::iterator it = u.out.emplace_back(e);
      v.in.emplace_back(*it);
    }

  public:
    //! read a network from a (not neccessarily sorted) list of pairs of uint32_t (aka edges) in which each vertex is less than num_nodes
    // if check_cyclic is set, check the input for cycles after reading it
    // NOTE: make sure that the indices of the leaves are increasing from left to right (that is, leaves are ordered)!
    //       This is the case for inputs in newick format
    template<class EdgeContainer = EdgeVec>
    NetworkT(const EdgeContainer& given_edges, const NameVec& _names, const uint32_t num_nodes, const bool check_cyclic = true):
      Parent::ROProtoNetworkT(_names, given_edges.size())
    {
      DEBUG3(std::cout << "constructing network from "<<_num_edges<<" edges" << std::endl);
      assert(num_nodes <= _num_edges + 1);
   
      root = Parent::read_nodes_and_prepare_edge_storage(given_edges, num_nodes);
      // actually read the edges
      for(const auto &e: given_edges) add_edge(e);
      // finally, if requested, check if N is acyclic
      if(check_cyclic && has_cycle()) throw std::logic_error("network contains a cycle");
    }

    template<class EdgeContainer = EdgeVec>
    NetworkT(const EdgeContainer& given_edges, const NameVec& names):
      NetworkT(given_edges, names, names.size())
    {}

  };


  typedef NetworkT<> Network;


  template<class _Edge = Edge,
           class _Node = NetworkNodeT<_Edge>,
           class _NodeList = std::vector<_Node>>
  std::ostream& operator<<(std::ostream& os, const NetworkT<_Edge, _Node, _NodeList>& N)
  {
    if(!N.empty()){
      std::string prefix = "";
      N.print_subtree(os, N.get_root(), prefix);
    }
    return os;
  }


}
