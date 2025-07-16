
#pragma once

#include "types.hpp"
#include "node.hpp"

namespace PT {

	// LCA & ancestor oracles are classes that answer LCA/ancestor queries in trees/networks
	// NOTE: when the tree/network changes, some of the oracles become invalid, so don't query them!

  // --------------------- Tree ANCESTOR ORACLE 1: no preprocessing, O(n) query  -----------------------
  // this uses naïve tree-climbing

  // --------------------- Tree ANCESTOR ORACLE 2: O(n) preprocessing, O(1) query -----------------------
  // this compares preorder-intervals
  // this works since u is ancestor of v <=> v's preorder interval is entirely contained in v's preorder interval

  // --------------------- Tree LCA ORACLE 1: Naïve oracle ----------------------------
  // the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
  // NOTE: this one does not become invalid when the tree changes
	template<class Tree, class SeenSet = NodeSet> //requires PhylogenyType<Phylo> // NOTE: this will cause 'concept depends on itself'
	struct NaiveTreeLCAOracle {
    const Tree* N;
  
    // helper function for the LCA
    // NOTE: this assumes that x is not the root!
    static bool update_for_LCA(SeenSet& seen, NodeDesc& z) {
      if(not mstd::set_val(seen, z)) return true;
      z = Tree::parent(z);
      return false;
    }
  
    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    NodeDesc operator()(NodeDesc x, NodeDesc y) const {
      SeenSet seen;
      const NodeDesc rt = N->root();
      while(x != y){
        if(x == rt) { // if x has become the root, then only y needs to be updated
          while(y != rt)
            if(update_for_LCA(seen, y)) return y;
          return rt;
        } else if(update_for_LCA(seen, x)) return x;
        std::swap(x,y);
      }
      return x;
    }
	};

  // --------------------- Tree LCA ORACLE 2: O(n) preprocessing, O(1) query ---------------------------
  // this is the "standard" Euler-Tour and RMQ-based oracle
  
  // --------------------- Tree LCA ORACLE 3: O(n) preprocessing, O(1) query ---------------------------
  // this uses the heavy-path decomposition, according to [Gabow'18, Gabow'90]
  // NOTE: this can actually return the "characteristic ancestors" of (x,y), that is LCA(x,y) and the children of the LCA leading to x and y
  template<class Tree>
  struct HeavyPathsLCAOracle {

    // the nodes of the compressed tree are actually microsets of log n nodes of the input tree and
    // each input node is a node x in a microset, corresponding to a bitstring representation of the
    // smallest DFS of the microset reaching x
    // The LCA of (x,y) within the microset is the longest common prefix of their bit-representations
    //NodeMap<MicroSetBitString> ms_bitstring;
  };
  
  // --------------------- Tree LCA ORACLE 4: O(n) preprocessing, O(alpha(n)) query  ---------------------------
  // this uses Gabow's dynamic-tree oracle
  // NOTE: this oracle allows adding leaves, but needs to be informed about the addition (duh)

  
  // --------------------- Network ANCESTOR ORACLE 1: no preprocessing, O(n) query -----------------------
  // this just does naïve multipath climbing
  
  // --------------------- Network ANCESTOR ORACLE 2: O(n) preprocessing, O(1) query -----------------------
  // this uses multidimensional dominance drawings
  // https://link.springer.com/article/10.1007/s42979-021-00713-6

  // --------------------- Network LCA ORACLE 1: no preprocessing, O(n) query ----------------------------
  // this uses naïve multi-path climbing
  // NOTE: we're inheriting from the TreeOracle in order to be castable to a tree oracle in case we know for sure that the network doesn't hybridize
	template<class Net, class SeenSet = NodeSet> //requires PhylogenyType<Net> // NOTE: this will cause 'concept depends on itself'
	struct NaiveNetworkLCAOracle:
    public NaiveTreeLCAOracle<Net, SeenSet>
  {
#warning "write me"
	public:
		NodeVec operator()(const NodeDesc x, const NodeDesc y) const {
      assert(false && "write me");
			return {};
		}
	};


  // ------------------ convenience classes and functions --------------------------
/*
  // for now, the default static ancestor oracle is the naive one, we'll change that once a better one is implemented
  template<class Net, class Seen = NodeSet>
  using DefaultStaticTreeAncestorOracle = NaiveTreeAncestorOracle<Net, Seen>;
  template<class Net, class Seen = NodeSet>
  using DefaultStaticNetworkAncestorOracle = NaiveNetworkAncestorOracle<Net, Seen>;

  // for dynamic trees and networks, we don't know better than the naïve oracle for now
  template<class Net, class Seen = NodeSet>
  using DefaultDynamicTreeAncestorOracle = NaiveTreeAncestorOracle<Net, Seen>;
  template<class Net, class Seen = NodeSet>
  using DefaultDynamicNetworkAncestorOracle = NaiveNetworkAncestorOracle<Net, Seen>;
*/

  // for now, the default static LCA oracle is the naive one, we'll change that once a better one is implemented
  template<class Net, class Seen = NodeSet>
  using DefaultStaticTreeLCAOracle = NaiveTreeLCAOracle<Net, Seen>;
  template<class Net, class Seen = NodeSet>
  using DefaultStaticNetworkLCAOracle = NaiveNetworkLCAOracle<Net, Seen>;

  // for dynamic trees and networks, we don't know better than the naïve oracle for now
  template<class Net, class Seen = NodeSet>
  using DefaultDynamicTreeLCAOracle = NaiveTreeLCAOracle<Net, Seen>;
  template<class Net, class Seen = NodeSet>
  using DefaultDynamicNetworkLCAOracle = NaiveNetworkLCAOracle<Net, Seen>;


}
