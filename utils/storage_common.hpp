

#pragma once

#include "types.hpp"

// this contains common functions for use in edge_- and adjacency_storages

namespace PT{
  struct immutable_tag {};
  struct mutable_tag {};


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
  void compute_degrees(const GivenEdgeContainer& given_edges, InOutDegreeMap& degrees, NodeTranslation* old_to_new)
  {
    if(old_to_new){
      // compute out-degrees and translate
      size_t num_nodes = 0;
      for(const auto &uv: given_edges){
        const auto emp_res = old_to_new->emplace(uv.head(), num_nodes);
        if(emp_res.second) ++num_nodes;
        ++degrees[emp_res.first->second].first;

        const auto emp_res2 = old_to_new->emplace(uv.tail(), num_nodes);
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

  // compute the root and leaves from an edge-container
  template<class EdgeContainer, class LeafContainer>
  Node compute_root_and_leaves(const EdgeContainer& edges, LeafContainer* leaves = nullptr)
  {
    char roots = 0;
    Node _root = 0;
    for(const Node u: edges.get_nodes()){
      if(edges.in_degree(u) == 0){
        if(roots++ == 0)
          _root = u;
        else throw std::logic_error("cannot create tree/network with multiple roots ("+std::string(_root)+" & "+std::string(u)+")");
      } else if(leaves && (edges.out_degree(u) == 0)) append(*leaves, u);
    }
    return _root;
  }


}// namespace


