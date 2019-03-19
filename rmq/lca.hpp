/**
 * Solves the LCA problem by running the ±1 RMQ solution (or any RMQ
 * implementation, actually) on the Euler tour of the tree.
 */

#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "pm_rmq.hpp"
#include "tree.hpp"

template<typename node_type, typename id_type = unsigned>
struct NodeToIdx
{
  const id_type operator[](const node_type& node) const { return node.get_id(); }
};

template<typename node_type>
struct NodeToChildren
{
  const std::vector<const node_type*> operator[](const node_type& node) const { return node.children(); }
};


template<
  typename node_type,
  typename node_id_type = unsigned, // NOTE: this has to be castable to unsigned as we will use it as array index!
  class node_to_id_type = const NodeToIdx<node_type, node_id_type>,
  class node_to_children_type = const NodeToChildren<node_type>,
  typename rmq_impl = pm_rmq<std::vector<ssize_t>::const_iterator>
  >
class lca 
{
  const node_type &_root_node; //< Input tree structure.
  const node_to_id_type _node_to_id; //< get the id of nodes
  const node_to_children_type _get_children; //< get the children of nodes
  std::vector<const node_type*> _euler_tour; //< The Euler tour of the input.
  std::vector<node_id_type> _node_id_to_euler_index; //< translate a node_id to an index in the Euler tour

  // A vector of the same length as the Euler tour, stores the depth of
  // the node from which the ith element of the Euler tour came.
  std::vector<ssize_t> _depth;

  // The ±1 RMQ data structure for the _depth array.
  std::unique_ptr<rmq_impl> _rmq;

  void register_vertex_in_euler_tour(const node_type& node,
                                     const unsigned depth)
  {
    const auto& node_id = _node_to_id[node];
    if(node_id >= _node_id_to_euler_index.size())
      _node_id_to_euler_index.resize(node_id + 1);
    _node_id_to_euler_index[node_id] = _euler_tour.size();
    _euler_tour.push_back(&node);
    _depth.push_back(depth);
  }

  void preprocess(const node_type &node, unsigned depth){
    // Constructs an Euler tour by running a DFS on the tree, emitting the
    // root of each subtree upon arrival and also upon completion of the
    // search of each of its children.
    register_vertex_in_euler_tour(node, depth);

    for(const auto& c: _get_children[node]){
      preprocess(*c, depth + 1);
      register_vertex_in_euler_tour(node, depth);
    }
  }

  void preprocess() {
    preprocess(_root_node, 0);
    _rmq.reset(new rmq_impl(_depth.begin(), _depth.end()));
  }

public:
  lca(const node_type &t,
      const node_id_type& max_id = 0,
      const node_to_id_type& node_to_id = node_to_id_type(),
      const node_to_children_type& node_to_children = node_to_children_type() )
    : _root_node(t),
      _node_to_id(node_to_id),
      _get_children(node_to_children)
  {
    _node_id_to_euler_index.reserve(max_id);
    preprocess();
  }

  const node_type* query(const node_type &u, const node_type &v) const {
    // After preprocessing, all nodes in the tree should have their repr()
    // initialized.  We use that to find the indexes on which to run the
    // RMQ algorithm.
    auto uei = _node_id_to_euler_index.at(_node_to_id[u]);
    auto vei = _node_id_to_euler_index.at(_node_to_id[v]);

    // The RMQ interface uses an exclusive upper bound so we need to go
    // one past that to include the node represented by the upper bound
    // here.
    if(vei < uei) std::swap(uei, vei);
    auto idx = _rmq->query(_depth.begin() + uei, _depth.begin() + vei + 1);

    // Future work: return the node itself, rather than the node's id?
    return _euler_tour[idx];
  }
};

