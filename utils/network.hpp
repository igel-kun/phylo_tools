
#pragma once

#include "tree.hpp"
#include "bridges.hpp"

namespace PT{

  // a network consists of a list of nodes, each having out- and in-neighbors
  template<class _EdgeStorage = GrowingNetworkAdjacencyStorage<>,
           class _NodeList = std::vector<uint32_t>,
           class _NodeData = void*,
           class _EdgeData = void*>
  class Network : public _Tree<_EdgeStorage, _NodeList, _NodeData, _EdgeData, false> 
  {
    using Parent = _Tree<_EdgeStorage, _NodeList, _NodeData, _EdgeData, false>; 
  protected:
    using Parent::names;
    using Parent::nodes;
    using Parent::edges;

    bool is_tree_type() const { return false; }
  public:

    using typename Parent::Node;
    using typename Parent::Edge;
    using Parent::_Tree;
    using Parent::children;
    using Parent::is_leaf;
    using Parent::out_degree;
    using Parent::out_edges;

    using OutEdgeContainer =      typename Parent::OutEdgeContainer;
    using ConstOutEdgeContainer = typename Parent::ConstOutEdgeContainer;

    using SuccContainer = typename Parent::SuccContainer;
    using ConstSuccContainer = typename Parent::ConstSuccContainer;

    using PredContainer = typename _EdgeStorage::PredContainer;
    using ConstPredContainer = typename _EdgeStorage::ConstPredContainer;
    
    using InEdgeContainer = typename _EdgeStorage::InEdgeContainer;
    using ConstInEdgeContainer = typename _EdgeStorage::ConstInEdgeContainer;

    // =================== information query ==============


    uint32_t in_degree(const Node u) const { return edges.in_degree(u); }
    bool is_tree_node(const Node u) const { return in_degree(u) == 1; }
    bool is_reti(const Node u) const { return in_degree(u) > 1; }
    bool is_inner_tree_node(const Node u) const { return is_tree_node(u) && !is_leaf(u); }

    uint32_t root() const { return edges.root(); }
    uint32_t type_of(const Node u) const
    {
      if(is_reti(u)) return NODE_TYPE_RETI;
      return Parent::type_of(u);
    }

    PredContainer         parents(Node u)       { return edges.predecessors(u); }
    ConstPredContainer      parents(Node u) const { return edges.predecessors(u); }
    InEdgeContainer      in_edges(Node u) {return edges.in_edges(u); }
    ConstInEdgeContainer in_edges(Node u) const {return edges.in_edges(u); }

    //! return whether the tree indices are in pre-order (modulo gaps) (they should always be) 
    bool is_preordered(const Node sub_root, Node& counter) const
    {
      if(is_reti(sub_root)) return true; // edges to reticualtions don't need to abide by the pre-order condition
      if(sub_root < counter) return false;
      counter = sub_root;
      for(Node v: children(sub_root))
        if(!is_preordered(v, counter)) return false;
      return true;
    }
    bool is_preordered() const
    {
      Node counter = root;
      return is_preordered(root, counter);
    }

  public:

    inline std::vector<Edge> get_bridges_below(const Node u) const { return list_bridges(*this, u); }
    inline std::vector<Edge> get_bridges() const { return list_bridges(*this, root()); }
    inline std::vector<Edge> get_bridges_below_postorder(const Node u) const { return list_bridges(*this, u); }
    inline std::vector<Edge> get_bridges_postorder() const { return list_bridges(*this, root()); }

    //! get a list of component roots in preorder
    void get_comp_roots(IndexVec& comp_roots) const
    {
      for(uint32_t r: nodes){
        if(is_inner_tree_node(r)){
          const uint32_t v = parents(r)[0];
          if(is_reti(v)) comp_roots.push_back(v);
        }
      }
    }

    // =================== modification ====================



    // ================== construction =====================

    Network(const NameVec& _names, const uint32_t _num_nodes):
      Parent(_names, _num_nodes)
    {}

    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const uint32_t u, std::string prefix, std::unordered_bitset& seen) const
    {
      std::string name = names.at(u);
      if(name == "") name = (is_reti(u)) ? std::string("(R" + std::to_string(u) + ")") : std::string("+");
      DEBUG3(name += "[" + std::to_string(u) + "]");
      os << '-' << name;
      
      const bool u_seen = test(seen, u);
      const auto& u_childs = children(u);
      if(!u_seen || !is_reti(u)){
        if(!u_seen) append(seen, u);
        switch(out_degree(u)){
          case 0:
            os << std::endl;
            break;
          case 1:
            prefix += std::string(name.length() + 1, ' ');
            print_subtree(os, std::front(children(u)), prefix, seen);
            break;
          default:
            prefix += std::string(name.length(), ' ') + '|';

            uint32_t count = u_childs.size();
            for(const auto c: u_childs){
              print_subtree(os, c, prefix, seen);
              if(--count > 0) os << prefix;
              if(count == 1) prefix.back() = ' ';
            }

        }
      } else os << std::endl;
    }

    void print_subtree(std::ostream& os, const uint32_t u) const
    {
      std::unordered_bitset seen(nodes.size());
      print_subtree(os, u, "", seen);
    }

    friend class TreeComponentInfo;
  };


  template<class _NodeData = void*,
           class _EdgeData = void*,
           class _NodeList = IndexSet,
           class _EdgeStorage = NonGrowingNetworkAdjacencyStorage<> >
  using RONetwork = Network<_EdgeStorage, _NodeList, _NodeData, _EdgeData>;


  template<class _NodeData = void*,
           class _NodeList = IndexSet,
           class _EdgeStorage = NonGrowingNetworkAdjacencyStorage<> >
  std::ostream& operator<<(std::ostream& os, const Network<_EdgeStorage, _NodeList, _NodeData>& N)
  {
    if(!N.empty()){
      for(auto u: N.get_nodes())
        std::cout << "children of "<<u<<": "<<N.children(u)<<std::endl;
      for(auto u: N.get_nodes())
        std::cout << "out-edges of "<<u<<": "<<N.out_edges(u)<<std::endl;
      N.print_subtree(os, N.root());
    } else os << "{}";
    return os;
  }

}
