
#pragma once

#include "tree.hpp"
#include "bridges.hpp"

namespace PT{

  // the main network class
  // note: the order of template arguments is such as to minimize expected number of arguments that have to be given
  template<class _NodeData,
           class _EdgeData,
           class _label_type = single_label_tag,
           class _EdgeStorage = MutableNetworkAdjacencyStorage<_NodeData, _EdgeData>,
           class _LabelMap = typename _EdgeStorage::template NodeMap<std::string>>
  class Network : public Tree<_NodeData, _EdgeData, _label_type, _EdgeStorage, _LabelMap>
  {
    using Parent = Tree<_NodeData, _EdgeData, _label_type, _EdgeStorage, _LabelMap>;
  protected:
    using Parent::node_labels;
    using Parent::_edges;
  public:
    using NetworkTypeTag = network_tag;
    using typename Parent::Edge;
    using Parent::Parent;
    using Parent::children;
    using Parent::parents;
    using Parent::is_leaf;
    using Parent::out_degree;
    using Parent::in_degree;
    using Parent::out_edges;
    using Parent::get_nodes;
    using Parent::num_nodes;
    using Parent::root;

    // =================== information query ==============

    inline bool is_tree_node(const Node u) const { return in_degree(u) == 1; }
    inline bool is_reti(const Node u) const { return in_degree(u) > 1; }
    inline bool is_inner_tree_node(const Node u) const { return is_tree_node(u) && !is_leaf(u); }

    uint32_t type_of(const Node u) const
    {
      if(is_reti(u)) return NODE_TYPE_RETI; else return Parent::type_of(u);
    }

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
      Node counter = root();
      return is_preordered(root(), counter);
    }

  public:

    inline std::vector<Edge> get_bridges_below(const Node u) const { return list_bridges(*this, u); }
    inline std::vector<Edge> get_bridges() const { return list_bridges(*this, root()); }
    inline std::vector<Edge> get_bridges_below_postorder(const Node u) const { return list_bridges(*this, u); }
    inline std::vector<Edge> get_bridges_postorder() const { return list_bridges(*this, root()); }

    //! get a list of component roots in preorder
    NodeVec get_comp_roots() const
    {
      NodeVec comp_roots;
      for(const Node u: get_nodes()){
        if(is_inner_tree_node(u)){
          const Node v = parents(u).front();
          if(is_reti(v)) comp_roots.push_back(v);
        }
      }
      return comp_roots;
    }

    // =================== modification ====================



    // ================== construction =====================

    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const Node u, std::string prefix, std::unordered_bitset& seen) const
    {
      std::string name = node_labels.at(u);
      if(name == "") name = (is_reti(u)) ? std::string("(R" + std::to_string(u) + ")") : std::string("+");
      DEBUG3(name += "[" + std::to_string(u) + "]");
      os << '-' << name;
      
      const bool u_seen = test(seen, (size_t)u);
      const auto& u_childs = children(u);
      if(!u_seen || !is_reti(u)){
        if(!u_seen) append(seen, (size_t)u);
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

    void print_subtree(std::ostream& os, const Node u) const
    {
      std::unordered_bitset seen(num_nodes());
      print_subtree(os, u, "", seen);
    }

    friend class TreeComponentInfo;
  };

  template<class _NodeData, class _EdgeData, class _label_type, class _EdgeStorage, class _LabelMap>
  std::ostream& operator<<(std::ostream& os, const Network<_NodeData, _EdgeData, _label_type, _EdgeStorage, _LabelMap>& N)
  {
    if(!N.empty()){
      N.print_subtree(os, N.root());
    } else os << "{}";
    return os;
  }


  // convenience aliases
  template<class _NodeData = void, class _EdgeData = void, class _label_type = single_label_tag, class _LabelMap = HashMap<Node, std::string>>
  using MutableNetwork = Network<_NodeData, _EdgeData, _label_type, MutableNetworkAdjacencyStorage<_NodeData, _EdgeData>, _LabelMap>;
  template<class _NodeData = void, class _EdgeData = void, class _label_type = single_label_tag, class _LabelMap = ConsecutiveMap<Node,std::string>>
  using ConstNetwork = Network<_NodeData, _EdgeData, _label_type, ConsecutiveNetworkAdjacencyStorage<_NodeData, _EdgeData>, _LabelMap>;

  // these two types should cover 95% of all (non-internal) use cases
  template<class _NodeData = void, class _EdgeData = void, class _label_type = single_label_tag>
  using RWNetwork = MutableNetwork<_NodeData, _EdgeData, _label_type>;
  template<class _NodeData = void, class _EdgeData = void, class _label_type = single_label_tag>
  using RONetwork = ConstNetwork<_NodeData, _EdgeData, _label_type>;



} // namespace
