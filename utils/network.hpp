
#pragma once

#include "tree.hpp"
#include "bridges.hpp"

namespace PT{

  // the main network class
  // note: the order of template arguments is such as to minimize expected number of arguments that have to be given
  template<class _NodeData,
           class _EdgeData,
           class _LabelTag = single_label_tag,
           class _EdgeStorage = MutableNetworkAdjacencyStorage<_EdgeData>,
           class _LabelMap = typename _EdgeStorage::template NodeMap<std::string>>
  class Network : public Tree<_NodeData, _EdgeData, _LabelTag, _EdgeStorage, _LabelMap, network_tag>
  {
    using Parent = Tree<_NodeData, _EdgeData, _LabelTag, _EdgeStorage, _LabelMap, network_tag>;
    template<class, class, class, class> friend class _Tree;
  public:
    using NetworkTag = network_tag;
    using typename Parent::Edge;
    using typename Parent::EdgeRef;
    
    using Parent::Parent;
    using Parent::children;
    using Parent::parents;
    using Parent::is_leaf;
    using Parent::out_degree;
    using Parent::in_degree;
    using Parent::out_edges;
    using Parent::nodes;
    using Parent::num_nodes;
    using Parent::root;
    using Parent::is_tree;
    using Parent::label;
    
    static constexpr bool is_declared_network = std::is_same_v<NetworkTag, network_tag>;

    // ================== cast back to tree (f.ex. to use Tree::LCA on a network without reticulation) ===============
    Parent& as_tree() { return *this; }
    const Parent& as_tree() const { return *this; }
    // =================== information query ==============

    inline bool is_tree_node(const Node u) const { return in_degree(u) == 1; }
    inline bool is_reti(const Node u) const { return in_degree(u) > 1; }
    inline bool is_inner_tree_node(const Node u) const { return is_tree_node(u) && !is_leaf(u); }

    node_type type_of(const Node u) const
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

    // if the edges currently in the network actually describe a tree, we can cast it up to tree
    // NOTE: the EdgeStorage will be unaffected, potentially allowing you to insert edges that could not be inserted into a normal Tree
    //       (unless your network has been declared with a Tree Storage in the first place, which is uncommon, but possible)
    Parent& use_as_tree()
    {
      assert(is_tree());
      return *(reinterpret_cast<Parent*>(*this));
    }
    const Parent& use_as_tree() const
    {
      assert(is_tree());
      return *(reinterpret_cast<const Parent*>(*this));
    }

    inline std::vector<Edge> get_bridges_below(const Node u) const { return list_bridges(*this, u); }
    inline std::vector<Edge> get_bridges() const { return list_bridges(*this, root()); }
    inline std::vector<Edge> get_bridges_below_postorder(const Node u) const { return list_bridges(*this, u); }
    inline std::vector<Edge> get_bridges_postorder() const { return list_bridges(*this, root()); }


    // return whether there is an x-y-path in the network
    bool has_path(const Node x, Node y) const
    {
      while(1){
        while(!is_reti(y)){
          if(x == y) return true;
          if(Parent::is_root(y)) return false;
          y = std::front(parents(y));
        }
        typename Parent::NodeSet seen;
        std::queue<Node> search_front;
        // add the parents of y to the search front
        for(const Node p: parents(y)) search_front.push(p);
        // if, at some point, the search-front shrinks back to a single strand, restart the simpler has_path()
        while((search_front.size() != 1) || is_reti(y)){
          y = search_front.front(); search_front.pop();
          if(x != y){
            if(append(seen, y).second)
              for(const Node p: parents(y)) search_front.push(p);
          } else return true;
        }
        y = search_front.front();
      }
    }

    // return a list of lowest ancestors of x that are non-suppressible tree nodes (that is, skip all outdeg-1 nodes above x)
    template<class Container>
    void get_lowest_tree_ancestors(const Node x, Container& c)
    {
      for(const Node y: parents(x)){
        if(out_degree(y) == 1)
          get_lowest_tree_ancestors(y, c);
        else
          append(c, y);
      }
    }
    template<class Container>
    Container get_lowest_tree_ancestors(const Node x)
    {
      Container result;
      get_lowest_tree_ancestors(x, result);
      return result;
    }

    // =================== modification ====================



    // ================== construction =====================

    // =================== i/o ======================
    
    void print_subtree(std::ostream& os, const Node u, std::string prefix, std::unordered_bitset& seen) const
    {
      std::string name = label(u);
      if(name == "") name = (is_reti(u)) ? std::string("(R" + std::to_string(u) + ")") : std::string("+");
      DEBUG3(name += "[" + std::to_string(u) + "]");
      os << '-' << name;
      
      const bool u_seen = test(seen, u);
      const auto& u_childs = children(u);
      if(!u_seen || !is_reti(u)){
        if(!u_seen) append(seen, u);
        switch(u_childs.size()){
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

  template<class _NodeData, class _EdgeData, class _LabelTag, class _EdgeStorage, class _LabelMap>
  std::ostream& operator<<(std::ostream& os, const Network<_NodeData, _EdgeData, _LabelTag, _EdgeStorage, _LabelMap>& N)
  {
    if(!N.empty()){
      N.print_subtree(os, N.root());
    } else os << "{}";
    return os;
  }


  // these two types should cover 95% of all (non-internal) use cases
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = single_label_tag, class _LabelMap = HashMap<Node, std::string>>
  using RWNetwork = Network<_NodeData, _EdgeData, _LabelTag, MutableNetworkAdjacencyStorage<_EdgeData>, _LabelMap>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = single_label_tag, class _LabelMap = RawConsecutiveMap<Node,std::string>>
  using RONetwork = Network<_NodeData, _EdgeData, _LabelTag, ConsecutiveNetworkAdjacencyStorage<_EdgeData>, _LabelMap>;

  // here's 2 for multi-label convenience
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = multi_label_tag, class _LabelMap = HashMap<Node, std::string>>
  using RWMulNetwork = RWNetwork<_NodeData, _EdgeData, _LabelTag, _LabelMap>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelTag = multi_label_tag, class _LabelMap = RawConsecutiveMap<Node, std::string>>
  using ROMulNetwork = RONetwork<_NodeData, _EdgeData, _LabelTag, _LabelMap>;

  // use these two if you have declared a tree and need a different type of tree which should interact with the first one (i.e. needs the same label-map)
  //NOTE: if __Network is const, then this gets a const LabelMap
  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _LabelTag = typename __Network::LabelTag,
    class _MutabilityTag = typename __Network::MutabilityTag,
    class _LabelMap = std::copy_cv_t<__Network, LabelMapOf<__Network>>>
  using CompatibleNetwork = std::conditional_t<std::is_same_v<_MutabilityTag, mutable_tag>,
                                RWNetwork<_NodeData, _EdgeData, _LabelTag, _LabelMap>,
                                RONetwork<_NodeData, _EdgeData, _LabelTag, _LabelMap>>;

  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _LabelTag = typename __Network::LabelTag>
  using CompatibleRWNetwork = CompatibleNetwork<__Network, _NodeData, _EdgeData, _LabelTag, mutable_tag>;
  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _LabelTag = typename __Network::LabelTag>
  using CompatibleRONetwork = CompatibleNetwork<__Network, _NodeData, _EdgeData, _LabelTag, immutable_tag>;
  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _MutabilityTag = typename __Network::MutabilityTag>
  using CompatibleMulNetwork = CompatibleNetwork<__Network, _NodeData, _EdgeData, multi_label_tag, _MutabilityTag>;
  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _MutabilityTag = typename __Network::MutabilityTag>
  using CompatibleSilNetwork = CompatibleNetwork<__Network, _NodeData, _EdgeData, single_label_tag, _MutabilityTag>;



                                  
  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _LabelTag = typename __Network::LabelTag,
    class _NetworkTag = typename __Network::NetworkTag>
  using CompatibleRO = std::conditional_t<std::is_same_v<_NetworkTag, tree_tag>,
                                  CompatibleROTree<__Network, _NodeData, _EdgeData, _LabelTag>,
                                  CompatibleRONetwork<__Network, _NodeData, _EdgeData, _LabelTag>>;

  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _LabelTag = typename __Network::LabelTag,
    class _NetworkTag = typename __Network::NetworkTag>
  using CompatibleRW = std::conditional_t<std::is_same_v<_NetworkTag, tree_tag>,
                                  CompatibleRWTree<__Network, _NodeData, _EdgeData, _LabelTag>,
                                  CompatibleRWNetwork<__Network, _NodeData, _EdgeData, _LabelTag>>;

  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _MutabilityTag = typename __Network::MutabilityTag,
    class _NetworkTag = typename __Network::NetworkTag>
  using CompatibleMul = std::conditional_t<std::is_same_v<_NetworkTag, tree_tag>,
                                  CompatibleMulTree<__Network, _NodeData, _EdgeData, _MutabilityTag>,
                                  CompatibleMulNetwork<__Network, _NodeData, _EdgeData, _MutabilityTag>>;

  template<class __Network,
    class _NodeData = typename __Network::NodeData,
    class _EdgeData = typename __Network::EdgeData,
    class _MutabilityTag = typename __Network::MutabilityTag,
    class _NetworkTag = typename __Network::NetworkTag>
  using CompatibleSil = std::conditional_t<std::is_same_v<_NetworkTag, tree_tag>,
                                  CompatibleSilTree<__Network, _NodeData, _EdgeData, _MutabilityTag>,
                                  CompatibleSilNetwork<__Network, _NodeData, _EdgeData, _MutabilityTag>>;


} // namespace
