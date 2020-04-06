

#pragma once

#include "types.hpp"
#include "iter_bitset.hpp"

// this contains common functions for use in edge_- and adjacency_storages

namespace PT{
  struct immutable_tag {};
  struct mutable_tag {};

  // return whether the nodes in the given edgelist occur consecutively
  template<class EdgeList>
  bool is_consecutive(const EdgeList& el)
  {
    std::ordered_bitset _seen;
    for(const auto& xy: el) {
      _seen.set(head(xy));
      _seen.set(tail(xy));
    }
    return _seen.full();
  }


  template<class GivenEdgeContainer>
  inline void compute_degrees(const GivenEdgeContainer& given_edges, InOutDegreeMap& degrees)
  {
    // compute out-degrees
    for(const auto &uv: given_edges){
      ++degrees[uv.head()].first;
      ++degrees[uv.tail()].second;
    }
  }

  template<class GivenEdgeContainer>
  inline void compute_degrees(const GivenEdgeContainer& given_edges, InOutDegreeMap& degrees, NodeTranslation* old_to_new)
  {
    if(old_to_new){
      // compute out-degrees and translate
      size_t num_nodes = 0;
      for(const auto &uv: given_edges){
        const auto emp_res = append(*old_to_new, uv.head(), (Node)num_nodes);
        if(emp_res.second) ++num_nodes;
        ++degrees[emp_res.first->second].first;

        const auto emp_res2 = append(*old_to_new, uv.tail(), (Node)num_nodes);
        if(emp_res2.second) ++num_nodes;
        ++degrees[emp_res2.first->second].second;
      }
    } else compute_degrees(given_edges, degrees);
  }

  // compute the root and leaves from a mapping of nodes to (indeg,outdeg)-pairs
  template<class LeafContainer>
  Node compute_root_and_leaves(const InOutDegreeMap& deg, LeafContainer* leaves = nullptr)
  {
    char roots = 0;
    Node _root = 0;
    for(const auto& ud: deg){
      const Node u = ud.first;
      const auto& u_deg = ud.second;
      if(u_deg.first == 0){
        if(roots++ == 0)
          _root = u;
        else throw std::logic_error("cannot create tree/network with multiple roots ("+std::to_string(_root)+" & "+std::to_string(u)+")");
      } else if(leaves && (u_deg.second == 0)) append(*leaves, u);
    }
    return _root;
  }

  template<class EdgeContainer, class LeafContainer>
  void compute_root_and_leaves(const EdgeContainer& edges, NodeTranslation* old_to_new, LeafContainer* leaves = nullptr)
  {
    edges.compute_root();
    if(leaves)
      for(const auto& uV: edges.successors())
        if(uV.second.empty())
          append(leaves, uV.first);
    // translate maps are kind of silly for mutable storages, but if the user insists to want one, we'll give her/him one
    if(old_to_new)
      for(const auto& uV: edges.successors())
        append(*old_to_new, uV.first, uV.first);
  }

}// namespace


