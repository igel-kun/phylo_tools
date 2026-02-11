
#pragma once

#include <ranges>

#include "types.hpp"
#include "node.hpp"
#include "heavy_path_decomp.hpp"
#include "linear_interval.hpp"

namespace PT {

  // Ancestor oracles can be called with (x,y) returning true iff x is an ancestor of y, that is, there is an x->y path

	// LCA & ancestor oracles are classes that answer LCA/ancestor queries in trees/networks
	// NOTE: when the tree/network changes, some of the oracles become invalid, so don't query them!

  // --------------------- Tree ANCESTOR ORACLE 1: no preprocessing, O(n) query  -----------------------
  // this uses naïve tree-climbing
	template<class Tree>
	struct NaiveTreeAncestorOracle {
    NaiveTreeAncestorOracle() = default;
    NaiveTreeAncestorOracle(const Tree&) {}

    static bool is_root(const NodeDesc x) { return Tree::is_root(x); }
  
    //! the naive Ancestor just walks up from y and until we find x or the root
    //NOTE: x may be a set of nodes or a node-predicate
    bool operator()(const auto& x, NodeDesc y) const {
      while(not test(x, y)) {
        if(is_root(y)) return false;
        y = Tree::parent(y);
      }
      return true;
    }
	};


  // --------------------- Tree ANCESTOR ORACLE 2: O(n) preprocessing, O(1) query -----------------------
  // this compares preorder-intervals
  // this works since u is ancestor of v <=> v's preorder interval is entirely contained in v's preorder interval
	template<class Tree>
	struct IntervalTreeAncestorOracle {
    using NodeToInterval = NodeMap<mstd::linear_interval<uint32_t>>;

    // the preorder interval stores all preorder numbers below each node
    // NOTE: the second item (that is, high()) is always equal to the preorder number of the node itself
    NodeToInterval preorder_interval;

    static bool is_root(const NodeDesc x) { return Tree::is_root(x); }

    static NodeToInterval create_preorder_interval(const Tree& T) {
      NodeToInterval tmp;
      for(const NodeDesc v: T.nodes_preorder()) {
        const auto preorder_num = tmp.size();
        const auto [v_interval, v_success] = tmp.try_emplace(v, preorder_num, preorder_num);
        if(not v_success)
          v_interval->second.high() = preorder_num;
        if(not is_root(v))
          tmp.try_emplace(Tree::parent(v), v_interval->second);
      }
      return tmp;
    }
    
    auto preorder_num(const NodeDesc x) const { return preorder_interval.at(x).high(); }

    IntervalTreeAncestorOracle(const Tree& T):
      preorder_interval(create_preorder_interval(T))
    {}
  
    //! x is an ancestor of y if y's preorder number (second item) is between the smallest preorder number below x and x's preorder number
    bool operator()(const NodeDesc x, NodeDesc y) const { return preorder_interval.at(x).contains(preorder_num(y)); }
	};


  // --------------------- Tree LCA ORACLE 1: Naïve oracle ----------------------------
  // the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
  // NOTE: this one does not become invalid when the tree changes
	template<class Tree, NodeContainerType SeenSet = NodeSet> //requires PhylogenyType<Phylo> // NOTE: this will cause 'concept depends on itself'
	struct NaiveTreeLCAOracle {
    NaiveTreeLCAOracle() = default;
    NaiveTreeLCAOracle(const Tree&) {}

    // if z is not seen, then mark it as seen and walk to the parent
    // return whether z had been seen before
    // NOTE: this assumes that x is not the root!
    static bool update_for_LCA(SeenSet& seen, NodeDesc& z) {
      if(not mstd::set_val(seen, z)) return true;
      z = Tree::parent(z);
      return false;
    }

    static bool is_root(const NodeDesc x) { return Tree::is_root(x); }
  
    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    NodeDesc operator()(NodeDesc x, NodeDesc y) const {
      SeenSet seen;
      while(x != y){
        if(is_root(x)) { // if x has become the root, then only y needs to be updated
          while(not is_root(y))
            if(update_for_LCA(seen, y)) return y;
          return x;
        } else if(update_for_LCA(seen, x)) return x;
        std::swap(x,y);
      }
      return x;
    }
	};

  // --------------------- Tree LCA ORACLE 2: O(n) preprocessing, O(1) query ---------------------------
  // this is the "standard" Euler-Tour and RMQ-based oracle

  // --------------------- Tree LCA ORACLE 3: O(n) preprocessing, O(log n) query ---------------------------
  // this uses plain heavy-path decomposition without Gabow's shinanigans
  // NOTE: this can actually return the "characteristic ancestors" of (x,y), that is LCA(x,y) and the children of the LCA leading to x and y
  template<class Tree, NodeOracleType<mstd::TR_PtrVoidOK> SubtreeSizeOracle = void>
  struct HeavyPathsLCAOracle:
    public HeavyPathDecomposition<Tree, SubtreeSizeOracle>
  {
    // ------- static stuff --------
    using Parent = HeavyPathDecomposition<Tree, SubtreeSizeOracle>;

    // to climb heavy paths we'll need to know some things about the two nodes
    struct ClimbInfo {
      NodeDesc node;
      size_t subtree_size;
      size_t apex_subtree_size;

      ClimbInfo(const NodeDesc x):
        node{x}, subtree_size{Parent::subtree_size(x)}, apex_subtree_size{Parent::subtree_size_of_apex(x)}
      {}
      
      // return whether node is an apex
      bool is_apex() const { return subtree_size == apex_subtree_size; }

      // return the higher among node and other.node, assuming that they are on the same heavy-path
      NodeDesc get_higher(const ClimbInfo& other) const { return (subtree_size > other.subtree_size) ? node : other.node; } 
    };

    // ------- members --------
    // ------- construction & desctruction ---------
    using Parent::Parent;

    // ------- operators --------
    // to find the LCA of x and y, use naïve climbing, but remember that we know subtree sizes
    NodeDesc operator()(NodeDesc x, NodeDesc y) const {
      NodeDesc result = x;
      if(x != y) {
        ClimbInfo x_info{x};
        ClimbInfo y_info{y};
        while(true)
          switch(x_info.process(y_info)) {
            case 2: return x_info.node; // both are the same node
            case 1: return x_info.get_higher(y_info); // both are non-apexes on the same heavy path
            default:;
          }
      }
      return result;
    }
    // ------- methods: initialization --------
    // ------- methods: modification --------
    // climb to the parent of node, updating the subtree-size-information
    uint8_t climb(ClimbInfo& info) const {
      info = {
        Parent::parent(info.node),
        (info.is_apex() ? Parent::subtree_size(info.node) : info.apex_subtree_size),
        Parent::subtree_size_of_apex(info.node)};
      return 0;
    }
   
    // figure out if one of the two ClimbInfos may climb and execute the climb
    // return 0 if someone climbed, 1 if they are different non-apexes on the same heavy-path, 2 if they are the same node
    uint8_t process(ClimbInfo& x, ClimbInfo& y) const {
      // if y is already strictly higher than our apex, then the LCA is even higher
      if(x.apex_subtree_size < y.subtree_size) return climb(x);
      if(y.apex_subtree_size < x.subtree_size) return climb(y);
      
      // the one with the smaller apex_subtree_size may climb
      if(x.apex_subtree_size < y.apex_subtree_size) return climb(x);
      if(x.apex_subtree_size > y.apex_subtree_size) return climb(y);
      
      // now, their apexes have the same subtree-size, so the two apexes might be the same node (which might also be equal to x and y)
      if(x.node == y.node) return 2; // if they are the same node, then neither may climb

      // if any of the two is an apex, then the other may climb (recall that their apex_subtree_size is the same, but they are not the same node)
      if(x.is_apex()) return y.climb();
      if(y.is_apex()) return x.climb();

      // now, neither of them is an apex and, if they are both on the same heavy path, then neither may climb
      if(Parent::parent(x.node) == Parent::parent(y.node)) return 1; // both are non-apexes on the same heavy path
      // if they are not on the same heavy path, then both can climb (since the apexes have same subtree sizes)
      climb(x); climb(y);
      return 0;
    }

    // ------- methods: query --------
  };

  // --------------------- Tree LCA ORACLE 4: O(n) preprocessing, O(1) query ---------------------------
  // this uses the heavy-path decomposition, according to [Gabow'18, Gabow'90]
  // NOTE: this can actually return the "characteristic ancestors" of (x,y), that is LCA(x,y) and the children of the LCA leading to x and y
  template<class Tree>
  struct AdvancedHeavyPathsLCAOracle {
    // the nodes of the compressed tree are actually microsets of log n nodes of the input tree and
    // each input node is a node x in a microset, corresponding to a bitstring representation of the
    // smallest DFS of the microset reaching x
    // The LCA of (x,y) within the microset is computed using RMQs (I think; Gabow is not very clear on that)

    // allow microsets up to 33 nodes (implies DFS-bitstrings of (2*33-2)=64 bits
    using MicroSetBitString = uint64_t;

    NodeMap<MicroSetBitString> ms_bitstring;

  };
  

  // --------------------- Tree LCA ORACLE 4: O(n) preprocessing, O(alpha(n)) query  ---------------------------
  // this uses Gabow's dynamic-tree oracle
  // NOTE: this oracle allows adding leaves, but needs to be informed about the addition (duh)

  
  // --------------------- Network ANCESTOR ORACLE 1: no preprocessing, O(m) query -----------------------
  // this just does naïve multipath climbing
  template<class Net, NodeContainerType SeenSet = NodeSet>
	struct NaiveNetworkAncestorOracle {
    // return if any x has a path to y avoiding forbidden
    static bool has_path(const auto& x, const NodeDesc y, auto&& forbidden) {
      if(mstd::test(x, y)) return true;
      if(mstd::test(forbidden, y)) return false;

      for(const NodeDesc p: Net::parents(y))
        if(has_path(x, p, forbidden)) return true;
      // if no parent of y can be reached from x, then y can't either
      mstd::append(forbidden, y);
      return false;
    }

    // return true iff x has a path to y
    // NOTE: x may be a set of nodes or a node-predicate
    template<class... Args>
    bool operator()(const auto& x, const NodeDesc y, Args&&... args) const { return has_path(x, y, SeenSet(std::forward<Args>(args)...)); }

    static void add_leaf(const NodeDesc l, const NodeDesc parent) {}
    static void add_edge(const auto uv) {}
    static void subdivide_edge(const auto uv) {}
    static void remove_edge(const auto uv) {}
	};

  // --------------------- Network ANCESTOR ORACLE 2: O(n) preprocessing, O(1) query -----------------------
  // this uses multidimensional dominance drawings
  // https://link.springer.com/article/10.1007/s42979-021-00713-6



  // ------------ Network LCA ORACLE 0: Meta oracle reducing to ancestor and descendant calls -------------
  // the LCA of a set X is the intersection of the ancestor sets of all x in X,
  // filtered by the predicate that returns whether the strict descendants in the same set are empty
  template<class Net, class Ancestors, NodeContainerType Output = NodeVec>
    requires (std::is_invocable_v<Ancestors, const NodeDesc&>)
  struct MetaNetworkLCAOracle {
    using AncestorContainer = std::remove_cvref_t<std::invoke_result_t<Ancestors, const NodeDesc&>>;
    static_assert(NodeContainerType<AncestorContainer>);
 
    [[ no_unique_address ]] Ancestors get_ancestors;

    INHERIT_ALL_CONSTRUCTORS(MetaNetworkLCAOracle, get_ancestors);

    void filter_uncommon(AncestorContainer& x_ancestors, const NodeDesc y) { mstd::intersect(x_ancestors, get_ancestors(y)); }
  
    template<StrictNodeContainerType Nodes>
    AncestorContainer get_common_ancestors(const NodeDesc x, const NodeDesc y) {
      AncestorContainer result = get_ancestors(x);
      filter_uncommon(result, y);
      return result;
    }

    template<NodeContainerType Nodes>
		auto get_common_ancestors(const Nodes& nodes) {
      assert(not nodes.empty());
      auto it = nodes.begin();
      NodeDesc first = *it;
      NodeSet seen{first};
      AncestorContainer common{first}; // defer computation of ancestors 'till we have seen a second node
      while(true) {
        if(++it != nodes.end()) {
          // filter only if we haven't seen the same node before
          if(append(seen, *it).second) {
            if(first != NoNode) { 
              common = get_ancestors(first);
              first = NoNode;
            }
            filter_uncommon(common, *it);
          }
        } else break;
      }
      return common;
    }

    Output filter_maxima(const AncestorContainer& nodes) const {
      Output result;
      for(const NodeDesc x: nodes)
        if(mstd::are_disjoint(Net::children(x), nodes))
          append(result, x);
      return result;
    }

    Output operator()(const NodeDesc x, const NodeDesc y) const {
      return (x == y) ? Output{x} : filter_maxima(get_common_ancestors(x, y));
		}
    template<NodeContainerType Nodes>
		Output operator()(const Nodes& nodes) const {
      return filter_maxima(get_common_ancestors(nodes));
    }

    static void add_leaf(const NodeDesc l, const NodeDesc parent) {}
    static void add_edge(const auto uv) {}
    static void subdivide_edge(const auto uv) {}
    static void remove_edge(const auto uv) {}    
  };

  //
  // --------------------- Network LCA ORACLE 1: no preprocessing, O(m) query ----------------------------
  // this uses naïve multi-path climbing
  // NOTE: we're inheriting from the TreeOracle in order to be castable to a tree oracle in case we know for sure that the network doesn't hybridize
  // requires PhylogenyType<Net> // NOTE: this will cause 'concept depends on itself'
	template<class Net, NodeContainerType Output = NodeVec>
	using NaiveNetworkLCAOracle = MetaNetworkLCAOracle<Net,
        decltype([](const NodeDesc x){ return Traversal<preorder | reverse_traversal, Net>{x}.template to_container<NodeSet>();}),
        Output>;


  // ------------------ convenience classes and functions --------------------------

  // for now, most of the default oracles are the naive ones, we'll change that once better ones are implemented
  template<class Tree> using DefaultStaticTreeAncestorOracle = IntervalTreeAncestorOracle<Tree>;
  template<class Tree> using DefaultDynamicTreeAncestorOracle = NaiveTreeAncestorOracle<Tree>;
  template<class Net> using DefaultStaticNetworkAncestorOracle = NaiveNetworkAncestorOracle<Net>;
  template<class Net> using DefaultDynamicNetworkAncestorOracle = NaiveNetworkAncestorOracle<Net>;

  template<class Tree> using DefaultStaticTreeLCAOracle = NaiveTreeLCAOracle<Tree>;
  template<class Tree> using DefaultDynamicTreeLCAOracle = NaiveTreeLCAOracle<Tree>;
  template<class Net> using DefaultStaticNetworkLCAOracle = NaiveNetworkLCAOracle<Net>;
  template<class Net> using DefaultDynamicNetworkLCAOracle = NaiveNetworkLCAOracle<Net>;


  template<class Net, bool is_tree = false>
  using DefaultStaticAncestorOracle =
    std::conditional_t<is_tree or Net::is_declared_tree, DefaultStaticTreeAncestorOracle<Net>, DefaultStaticNetworkAncestorOracle<Net>>;

  // for dynamic trees and networks, we don't know better than the naïve oracle for now
  template<class Net, bool is_tree = false>
  using DefaultDynamicAncestorOracle = 
    std::conditional_t<is_tree or Net::is_declared_tree, DefaultDynamicTreeAncestorOracle<Net>, DefaultDynamicNetworkAncestorOracle<Net>>;


  // for now, the default static LCA oracle is the naive one, we'll change that once a better one is implemented
  template<class Net, bool is_tree = false>
  using DefaultStaticLCAOracle =
    std::conditional_t<is_tree or Net::is_declared_tree, DefaultStaticTreeLCAOracle<Net>, DefaultStaticNetworkLCAOracle<Net>>;

  // for dynamic trees and networks, we don't know better than the naïve oracle for now
  template<class Net, bool is_tree = false>
  using DefaultDynamicLCAOracle = 
    std::conditional_t<is_tree or Net::is_declared_tree, DefaultDynamicTreeLCAOracle<Net>, DefaultDynamicNetworkLCAOracle<Net>>;

}
