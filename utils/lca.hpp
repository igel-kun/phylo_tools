
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

	template<class Net> //requires PhylogenyType<Net> // NOTE: this will cause 'concept depends on itself'
	class NaiveLCAOracle: public LCAOracle<Net> {
#warning write me
	public:
		NodeVec operator()(const NodeDesc x, const NodeDesc y) const {
			return {};
		}
	};

	template<TreeType Tree> //requires TreeType<Tree>
	class NaiveLCAOracle<Tree>: public LCAOracle<Tree> {
		using Parent = LCAOracle<Tree>;
		using Parent::N;
  
	protected:
    // helper function for the LCA
    bool update_for_LCA(std::unordered_bitset& seen, NodeDesc& z) const {
      if(z == N.root()) return false;
      if(!seen.set(z)) return true;
      z = N.parent(z);
      return false;
    }
  
	public:
		using Parent::Parent;

    //! the naive LCA just walks up from x and y one step at a time until we find a node that has been seen by both walks
    NodeDesc operator()(NodeDesc x, NodeDesc y) const {
      std::unordered_bitset seen;
      while(x != y){
        if(update_for_LCA(seen, x)) return x;
        if(update_for_LCA(seen, y)) return y;
      }
      return x;
    }

	};
}
