

#pragma once

// this contains common functions for use in edge_- and adjacency_storages

namespace PT{


  template<class GivenEdgeContainer, class DegMap>
  void compute_degrees(const GivenEdgeContainer& given_edges, DegMap& degrees, const uint32_t num_nodes)
  {
    // compute out-degrees and the root
    for(const auto &uv: given_edges){
      assert(uv.tail() < num_nodes);
      assert(uv.head() < num_nodes);
      ++degrees[uv.head()].first;
      ++degrees[uv.tail()].second;
    }
  }

   // compute degrees of nodes, and a list of nodes (each node except the root occuring exactly once) given a list of edges
  template<class GivenEdgeContainer, class NodeContainer, class DegMap>
  void compute_degrees_and_nodes(const GivenEdgeContainer& given_edges, NodeContainer& nodes, DegMap& degrees)
  {
    for(const auto& uv: given_edges){
      const auto u = uv.tail();
      const auto v = uv.head();
      
      // if v does not have in-degree yet, add it to the nodes
      auto& v_deg = degrees[v];
      if(!v_deg.first)
        append(nodes, v);
      
      // increase degrees of u and v
      ++(v_deg.first);
      ++(degrees[u].second);
      
      std::cout << "treated edge "<<uv<<" - nodes now "<<nodes<<" and degrees: "<<degrees<<"\n";
    }
  }

  // compute the root and leaves from a mapping of nodes to (indeg,outdeg)-pairs
  template<class DegMap, class LeafContainer, class Node = typename LeafContainer::value_type>
  Node compute_root_and_leaves(const DegMap& deg, LeafContainer* leaves = nullptr)
  {
    char roots = 0;
    Node _root = 0;
    for(const auto& ud: deg){
      const Node u = ud.first;
      const UIntPair& u_deg = ud.second;
      if(u_deg.first == 0){
        if(roots++ == 0)
          _root = u;
        else throw std::logic_error("cannot create tree/network with multiple roots ("+std::to_string(_root)+" & "+std::to_string(u)+")");
      }
      if(leaves && (u_deg.second == 0)) append(*leaves, u);
    }
    return _root;
  }


}// namespace


