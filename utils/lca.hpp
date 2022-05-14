
#pragma once

#include "types.hpp"
#include "node.hpp"

namespace PT {

	// the LCA oracle is a class that answers LCA queries
	// NOTE: when the tree/network changes, the oracle becomes invalid, so don't query it!
	template<class Phylo> //requires PhylogenyType<Phylo> // NOTE: this will cause 'concept depends on itself'
	class LCAOracle {
	protected:
		const Phylo& N;
	public:
		LCAOracle(const Phylo& _N): N(_N) {}
	};

	template<class Tree> //requires PhylogenyType<Phylo> // NOTE: this will cause 'concept depends on itself'
	class NaiveTreeLCAOracle: public LCAOracle<Tree> {
		using Parent = LCAOracle<Tree>;
    using SeenSet = std::unordered_set<NodeDesc>;
    //using SeenSet = std::unordered_bitset;
		using Parent::N;
  
	protected:
    // helper function for the LCA
    bool update_for_LCA(SeenSet& seen, NodeDesc& z) const {
      if(z == N.root()) return false;
      if(!set_val(seen, z)) return true;
      z = N.parent(z);
      return false;
    }
  
	public:
		using Parent::Parent;

    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    NodeDesc operator()(NodeDesc x, NodeDesc y) const {
      SeenSet seen;
      while(x != y){
        if(update_for_LCA(seen, x)) return x;
        if(update_for_LCA(seen, y)) return y;
      }
      return x;
    }
	};

  // NOTE: we're inheriting from the TreeOracle in order to be castable to a tree oracle in case we know for sure that the network doesn't hybridize
	template<class Net> //requires PhylogenyType<Net> // NOTE: this will cause 'concept depends on itself'
	class NaiveNetworkLCAOracle: public NaiveTreeLCAOracle<Net> {
#warning "write me"
	public:
		NodeVec operator()(const NodeDesc x, const NodeDesc y) const {
      assert(false && "write me");
			return {};
		}
	};

}
