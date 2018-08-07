
#pragma once

#include "utils.hpp"
#include "nh_lists.hpp"

namespace PT{

#define NODE_TYPE_LEAF 0x00
#define NODE_TYPE_TREE 0x01
#define NODE_TYPE_RETI 0x02
#define NODE_TYPE_ISOL 0x03


  template<class _SuccList = SortedConsecutiveEdges<Edge>>
  struct AnyNodeT
  {
    _SuccList out;

    bool is_leaf() const
    {
      return out.empty();
    }
    unsigned char get_type() const
    {
      return is_leaf() ? NODE_TYPE_LEAF : NODE_TYPE_TREE;
    }
    bool is_bifurcating() const
    {
      return out.size() == 2;
    }
  
    const uint32_t& child(const uint32_t i) const
    {
      return head(out[i]);
    }
    uint32_t& child(const uint32_t i)
    {
      return head(out[i]);
    }

    HeadConstFactory<_SuccList> children() const
    {
      return HeadConstFactory<_SuccList>(out);
    }
    HeadFactory<_SuccList> children()
    {
      return HeadFactory<_SuccList>(out);
    }

  };

  template<class _Edge = Edge, class _SuccList = SortedConsecutiveEdges<_Edge>, typename _RevEdge = _Edge*>
  struct TreeNodeT: public AnyNodeT<_SuccList>
  {
    using Parent = AnyNodeT<_SuccList>;
    using Parent::AnyNodeT;
    typedef _SuccList SuccList;
    typedef _RevEdge RevEdge;

    _RevEdge in;

    // all parents of this node are the same
    uint32_t parent(const uint32_t i) const
    {
      return in->tail();
    }
  };

  template<class _Edge = Edge, class _SuccList = SortedConsecutiveEdges<_Edge>, class _PredList = SortedNonConsecutiveEdges<_Edge>>
  struct NetworkNodeT: public AnyNodeT<_SuccList>
  {
    using Parent = AnyNodeT<_SuccList>;
    using Parent::out;
    typedef _SuccList SuccList;
    typedef _PredList PredList;

    _PredList in;

    bool is_reti() const
    {
      return in.size() > 1;
    }
    bool is_root() const
    {
      return in.empty() && !out.empty();
    }
    bool is_inner_tree() const
    {
      return (in.size() == 1) && !out.empty();
    }
    bool is_non_leaf_tree() const
    {
      return get_type() == NODE_TYPE_TREE;
    }
    bool is_isolated() const
    {
      return get_type() == NODE_TYPE_ISOL;
    }
    bool is_leaf() const
    {
      return get_type() == NODE_TYPE_LEAF;
    }
    unsigned char get_type() const
    {
      if(out.empty())
        return in.empty() ? NODE_TYPE_ISOL : NODE_TYPE_LEAF;
      else
        return (in.size() <= 1) ? NODE_TYPE_TREE : NODE_TYPE_RETI;
    }

    const uint32_t& parent(const uint32_t i) const
    {
      return tail(in[i]);
    }
    uint32_t& parent(const uint32_t i)
    {
      return tail(in[i]);
    }
    TailConstFactory<_PredList> parents() const
    {
      return TailConstFactory<_PredList>(in);
    }
    TailFactory<_PredList> parents()
    {
      return TailFactory<_PredList>(in);
    }

  };

  template<typename _Node, class _Data>
  struct NodeWithDataT: public _Node
  {
    _Data data;
  };


  typedef TreeNodeT<> TreeNode;
  typedef NetworkNodeT<> NetworkNode;

  template<typename _Data>
  using TreeNodeWithData = NodeWithDataT<TreeNode, _Data>;

  template<typename _Data>
  using NetworkNodeWithData = NodeWithDataT<NetworkNode, _Data>;

}

