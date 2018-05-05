
#pragma once

#include "utils.hpp"
#include "nh_lists.hpp"

namespace PT{

  template<class _NList = SortedFixNeighborList>
  struct AnyNode
  {
    _NList succ;

    bool is_leaf() const
    {
      return succ.empty();
    }
    unsigned char get_type() const
    {
      return is_leaf() ? NODE_TYPE_LEAF : NODE_TYPE_TREE;
    }
    bool is_bifurcating() const
    {
      return succ.size() == 2;
    }
  };

  template<class _NList = SortedFixNeighborList>
  struct TreeNode: public AnyNode<_NList>
  {
    uint32_t pred;
  };

  template<class _PredList = SortedFixNeighborList, class _SuccList = SortedFixNeighborList>
  struct NetworkNode: public AnyNode<_SuccList>
  {
    using Parent = AnyNode<_SuccList>;
    using Parent::succ;

    _PredList pred;

    bool is_reti() const
    {
      return pred.size() > 1;
    }
    bool is_root() const
    {
      return pred.empty() && !succ.empty();
    }
    bool is_inner_tree() const
    {
      return (pred.size() == 1) && !succ.empty();
    }
    bool is_isolated() const
    {
      return succ.empty() && pred.empty();
    }
    bool is_leaf() const
    {
      return succ.empty() && !pred.empty();
    }
    unsigned char get_type() const
    {
      if(succ.empty())
        return pred.empty() ? NODE_TYPE_ISOL : NODE_TYPE_LEAF;
      else
        return (pred.size() <= 1) ? NODE_TYPE_TREE : NODE_TYPE_RETI;
    }
  };

  template<class _Data, class _NList = SortedFixNeighborList>
  struct TreeNodeWithData: public TreeNode<_NList>
  {
    _Data data;
  };

  template<class _Data, class _PredList = SortedFixNeighborList, class _SuccList = SortedFixNeighborList>
  struct NetworkNodeWithData: public NetworkNode<_PredList, _SuccList>
  {
    _Data data;
  };

}

