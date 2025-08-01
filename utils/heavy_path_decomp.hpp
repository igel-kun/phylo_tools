
#pragma once

/*
 * This is an implementation of heavy-path decompositions for trees
 */
#include "types.hpp"

#ifdef DFSCORO
#include "dfs_coro.hpp"
#else
#include "dfs.hpp"
#endif

namespace PT {

  // the heavy-path decomposition class
  // accepts a NodeOracleType (also by pointer) or void, indicating that the subtree count should be done and stored by the decomposition itself
  template<class Tree, NodeOracleType<mstd::TR_PtrVoidOK> _SubtreeSizeOracle = void>
  struct HeavyPathDecomposition {
    // ------- description --------
    // definition: deleting all edges uv s.t. #descendants(u) >= 2*#descendants(v) + 1  yields a decomposition of T into "heavy paths"
    // definition: the top node of every heavy path is called an "apex"
    // now, every apex knows its parent in the parent heavy path, and each non-apex knows its apex; also, everyone knows their height in their heavy path

    // ------- static stuff --------
    static constexpr bool has_size_oracle = not std::is_void_v<_SubtreeSizeOracle>;
    using SubtreeSizeOracle = std::conditional_t<has_size_oracle, _SubtreeSizeOracle, mstd::monostate>;

    // if the user doesn't provide a size oracle, we'll store subtree sizes in the path_infos
    struct PathInfo {
      using SubtreeSize = std::conditional_t<has_size_oracle, mstd::monostate, size_t>;

      NodeDesc parent;
      [[ no_unique_address ]] SubtreeSize subtree_size;
    };
    
    // ------- members --------
  protected:
    NodeMap<PathInfo> path_info;
    NodeDesc root;
    [[ no_unique_address ]] SubtreeSizeOracle size_oracle;
  public:

    // ------- operators --------

    // ------- methods: initialization --------
  protected:
    // if no oracle was provided as a template argument, we'll make our own oracle
    void build_size_oracle() {
      for(const NodeDesc x: NodeTraversal<postorder, Tree>(root)) {
        auto& child_size = subtree_size(x);
        child_size = 1;
        for(const NodeDesc y: Tree::children(x))
          child_size += subtree_size(y);
      }
    }

    // build the actual decomposition
    void build_decomposition() {
      path_info[root].parent = NoNode;
      for(const NodeDesc x: NodeTraversal<preorder, Tree>(root)) {
        for(const NodeDesc y: Tree::children(x)) {
          // if y is an apex, then link y to x, otherwise to the apex of x (which might be x itself)
          if(is_apex_with_parent(y, x)) {
            path_info[y].parent = x;
          } else path_info[y].parent = is_apex(x) ? x : path_info[x].parent;
        }
      }
    }

    // ------- methods: modification --------

    // ------- methods: query --------
  public:
    NodeDesc parent(const NodeDesc x) const { return path_info.at(x).parent; }

    // if the subtree size of y is large, then it's on the heavy path (so no apex), otherwise it's an apex
    bool is_apex_with_parent(const NodeDesc x, const NodeDesc parent) const { return (subtree_size(parent) >= 2 * subtree_size(x) + 1); }

    // return if x is an apex
    bool is_apex(const NodeDesc x) const {
      if(x != root) {
        return is_apex_with_parent(x, Tree::parent(x));
      } else return true;
    }
    NodeDesc get_apex(const NodeDesc x) const {
      const NodeDesc p = parent(x);
      return is_apex_with_parent(x, p) ? x : p;
    }
    // return the subtree-size of the apex of x
    size_t subtree_size_of_apex(const NodeDesc x) const { return subtree_size(get_apex(x)); }

    // if x and y are on the same heavy path, return the higher one among them, otherwise return NoNode
    NodeDesc choose_higher_on_same_path(const NodeDesc x, const NodeDesc y) const { return choose_higher_on_same_path(x, y, subtree_size(x), subtree_size(y));}
    NodeDesc choose_higher_on_same_path(const NodeDesc x, const NodeDesc y, const size_t x_size, const size_t y_size) const {
      if(x != y) {
        if(x_size != y_size) {
          const auto [upper, lower] = (x_size > y_size) ? std::pair{x, y} : std::pair{y, x};
          if(is_apex(upper)) {
            if(is_apex(lower)) {
              return NoNode; // if both are apexes, they cannot be on the same heavy path
            } else return (upper == parent(lower)) ? upper : NoNode; // if upper is an apex, but lower is not, return whether upper IS the apex of lower
          } else return (parent(upper) == parent(lower)) ? upper : NoNode; // if neither x nor y is an apex, return whether they point to the same apex
        } else return NoNode; // if x != y but the subtree sizes are the same, then they are not on the same heavy path
      } else return x; // if x == y, then they are surely on the same heavy path
    }
    // return whether x and y are on the same heavy path
    bool on_same_heavy_path(const NodeDesc x, const NodeDesc y) const { return choose_higher_on_same_path(x, y) != NoNode; }

    const PathInfo& get_path_info() const { return path_info; }

    const auto& subtree_size(const NodeDesc x) const {
      if constexpr (NodeMapType<SubtreeSizeOracle, mstd::TR_PtrOK>) {
        return mstd::access(size_oracle)[x];
      } else if constexpr (mstd::is_invocable_v<SubtreeSizeOracle, mstd::TR_PtrOK, NodeDesc>) {
        return mstd::access(size_oracle)(x);
      } else { // if the oracle is not a map and not invocable with a NodeDesc, then it's void, so we're just storing it in the path_info
        return path_info[x].subtree_size;
      }
    }
  protected:
    auto& subtree_size(const NodeDesc x) {
      if constexpr (NodeMapType<SubtreeSizeOracle, mstd::TR_PtrOK>) {
        return mstd::access(size_oracle)[x];
      } else if constexpr (mstd::is_invocable_v<SubtreeSizeOracle, mstd::TR_PtrOK, NodeDesc>) {
        return mstd::access(size_oracle)(x);
      } else { // if the oracle is not a map and not invocable with a NodeDesc, then it's void, so we're just storing it in the path_info
        return path_info[x].subtree_size;
      }
    }

    // ------- construction & desctruction ---------
  public:
    HeavyPathDecomposition() = default;

    // pass all arguments into the size oracle construction
    template<class... Args>
    HeavyPathDecomposition(const NodeDesc _root, Args&&... args):
      root{_root},
      size_oracle(std::forward<Args>(args)...)
    {
      if constexpr ((not has_size_oracle) and (sizeof...(Args) == 0))
        build_size_oracle();
      build_decomposition();
    };

    template<class... Args>
    HeavyPathDecomposition(const Tree& T, Args&&... args):
      HeavyPathDecomposition(T.root(), std::forward<Args>(args)...)
    {}

  };
}
