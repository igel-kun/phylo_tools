
#pragma once

#include "set_interface.hpp"
#include "extension.hpp"
#include "tree_extension.hpp"
#include "bridges.hpp"
#include "subsets_constraint.hpp"

namespace PT{

  template<class _Network, class _Extension>
  void compute_min_sw_extension_brute_force(const _Network& N, _Extension& ex)
  {
    using Node = typename _Network::Node;
    //using Edge = typename _Network::Edge;

    std::unordered_set<Node> seen;
    // step 0: separate network by its bridges and compute sw separately for each of them
    for(const auto& uv: N.get_bridges_postorder()){
      // by presenting the bridges in post-order, we can be sure that no bridge is below the current bridge, so we only split off leaf-components
      std::cout << "found bridge "<<uv<<std::endl;
      const Node v = uv.head(); 
      // if v is a leaf, just add it to the extension; otherwise recurse to add a cheapest sub-extension
      if(N.is_leaf(v)){
        append(static_cast<std::vector<Node>&>(ex), v);
        std::cout << "appended leaf "<<v<<" extension now: "<<ex<<"\n";
      } else split(N, v, ex, seen);
      append(seen, v);
    }
    split(N, N.root(), ex, seen);
  }

  template<class _Network, class _Extension, class _ExceptContainer>
  void split(const _Network& N, const typename _Network::Node v, _Extension& ex, const _ExceptContainer& except)
  {
    using Node = typename _Network::Node;
    using Edge = typename _Network::Edge;

    const std::vector<Edge> el(N.get_edges_below_except(except, v));
    std::cout << v <<" is not a leaf, so I'm building a subnet rooted at "<<v<<" avoiding "<<except<<": "<<el<<"\n";
    if(!el.empty()){
      const _Network v_net(el, N.get_names());
      compute_min_sw_extension_brute_force_no_bridges(v_net, ex);
    } else {
      append(static_cast<std::vector<Node>&>(ex), v);
      std::cout << "appended pseudo-leaf "<<v<<" extension now: "<<ex<<"\n";
    }
  }

  template<class _Network, class _Extension>
  void compute_min_sw_extension_brute_force_no_bridges(const _Network& N, _Extension& ex)
  {
    using Node = typename _Network::Node;
    //using Edge = typename _Network::Edge;
    std::cout << N << "\n";
    if(N.num_nodes() > 1){
      std::cout << "======= checking constraint node subsets ========\n";
      // check all node-subsets constraint by the arcs in N
      for(const auto& nodes: NetworkConstraintSubsets<_Network, std::unordered_set<Node>>(N)){
        std::cout << nodes << "\n";
      }
      exit(1);
    } else {
      append(static_cast<std::vector<Node>&>(ex), N.root());
      std::cout << "appended root "<<N.root()<<" extension now: "<<ex<<"\n";
    }
  }

}// namespace


