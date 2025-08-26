
#pragma once

#include <unordered_map>
#include "set_interface.hpp"
#include "dynamic_sw.hpp"

namespace PT { class Extension; }
namespace mstd { // extensions are basically vectors...
  template<> struct is_vector<PT::Extension> { static constexpr bool value = true; };
}
namespace PT {
  struct partial_extension_tag {};

  struct Extension:
    public NodeVec
  {
    using Parent = NodeVec;

    INHERIT_ALL_CONSTRUCTORS(Extension, Parent);
    INHERIT_ASSIGNMENT(Extension, Parent);

    template<mstd::IterableType C> requires (not std::is_convertible_v<C, Parent> and std::is_convertible_v<mstd::value_type_of_t<C>, NodeDesc>)
    Extension& operator=(C&& v) noexcept { Parent tmp(v.begin(), v.end()); Parent::operator=(std::move(tmp)); return *this; }


    void compute_inverse(std::unordered_map<NodeDesc, sw_t>& inverse) const {
       for(size_t i = 0; i < this->size(); ++i) inverse.emplace(this->at(i), i);
    }
    auto get_inverse() const {
      std::unordered_map<NodeDesc, sw_t> result;
      compute_inverse(result);
      return result;
    }

    // return if the extension is valid for a given network
    template<StrictPhylogenyType Net>
    bool is_valid_for(const Net& N) const {
      // construct inverse of the extension, mapping each node to its position
      const std::unordered_map<NodeDesc, sw_t> inverse = get_inverse();

      // check if all arcs in the network go backwards in the extension
      for(const auto& uv: N.get_edges())
        if(inverse.at(uv.head()) > inverse.at(uv.tail())) return false;

      return true;
    }

    // return the scanwidth of the extension in the network N
    template<StrictPhylogenyType Net, class NetDeg = DefaultDegrees<Net>>
    sw_t scanwidth(NetDeg&& degrees = NetDeg()) const {
      if(!this->empty()) {
        return std::ranges::max(mstd::seconds(get_sw_map<Net>(std::forward<NetDeg>(degrees))));
      } else return 0;
    }

    // compute the scanwidth of all nodes in a given extension
    // NOTE: NetworkDegree is used to return the degrees (pair of indegree and outdegree) of a node in the network
    // NOTE: this can be used to construct the actual edge- or node- set corresponding to the scanwidth entry by passing a suitable function network_degrees
    template<StrictPhylogenyType Net, class NetDeg, class Output, class... Args>
      requires mstd::IsPair<std::invoke_result_t<std::remove_reference_t<NetDeg>, const NodeDesc>>
    void sw_map_meta(NetDeg&& network_degrees, Output& out, Args&&... args) const {
      DEBUG3(std::cout << "computing sw-map of extension "<<*this<<std::endl);
      DEBUG3(std::cout << "degree-extracter is "<<mstd::type_name<NetDeg>()<<"\n");
      DynamicScanwidth<Net, Output&, NetDeg>{out, std::forward<NetDeg>(network_degrees)}.update_all(*this, std::forward<Args>(args)...);
    }

    template<StrictPhylogenyType Network, class NetDeg, class Callback, class Output>
    void sw_map(Output&& out, NetDeg&& degrees, Callback&& save_highest_child) const {
      return sw_map_meta<Network>(std::forward<NetDeg>(degrees), std::forward<Output>(out), std::forward<Callback>(save_highest_child));
    }
    
    template<StrictPhylogenyType Network, class Output, class NetDeg = DefaultDegrees<Network>>
      requires (std::invocable<NetDeg, NodeDesc> && !std::is_void_v<std::invoke_result<NetDeg, NodeDesc>>)
    void sw_map(Output&& out, NetDeg&& degrees = NetDeg()) const {
      return sw_map_meta<Network>(std::forward<NetDeg>(degrees), std::forward<Output>(out));
    }

    template<StrictPhylogenyType Network, class Output, class Callback = DefaultDegrees<Network>>
      requires (!std::invocable<Callback, NodeDesc> || std::is_void_v<std::invoke_result<Callback, NodeDesc>>)
    void sw_map(Output&& out, Callback&& save_highest_child) const {
      return sw_map_meta<Network>(DefaultDegrees<Network>(), std::forward<Output>(out), std::forward<Callback>(save_highest_child));
    }


    template<StrictPhylogenyType Network, class Output = NodeMap<sw_t>, class... Args>
    Output get_sw_map(Args&&... args) const {
      Output result;
      sw_map<Network>(result, std::forward<Args>(args)...);
      return result;
    }


    // sw_nodes_map is for retrieving the SW-sets for each node in the scanwidth-layout corresponding to the extension
    template<StrictPhylogenyType Network, NodeMapType Output, class... Args>
    void sw_nodes_map(Output&& out, Args&&... args) const {
      using Nodes = mstd::mapped_type_of_t<Output>;
      using NodesAndNode = std::pair<Nodes, NodeDesc>;
      static_assert(std::is_same_v<std::remove_cvref_t<mstd::value_type_of_t<Nodes>>, NodeDesc>);

      return sw_map_meta<Network>(
        [&](const NodeDesc u){
          const auto& u_parents = Network::parents(u);
          return NodesAndNode(std::piecewise_construct, std::tuple{u_parents.begin(), u_parents.end()}, std::tuple{u});
        },
        std::forward<Output>(out),
        std::forward<Args>(args)...
      );
    }
    template<StrictPhylogenyType Network, NodeMapType Output = NodeMap<NodeSet>, class... Args>
    Output get_sw_nodes_map(Args&&... args) const {
      Output result;
      sw_nodes_map<Network>(result, std::forward<Args>(args)...);
      return result;
    }

    template<StrictPhylogenyType Network, NodeMapType Output, class... Args>
    void sw_edges_map(Output&& out, Args&&... args) const {
      using Edges = mstd::mapped_type_of_t<Output>;
      static_assert(EdgeType<mstd::value_type_of_t<Edges>>);
      using EdgesPair = std::pair<Edges, Edges>; 
      return sw_map_meta<Network>([&](const NodeDesc u) {
          const auto& u_in_edges = Network::in_edges(u);
          const auto& u_out_edges = Network::out_edges(u);
          return EdgesPair(u_in_edges.template to_container<Edges>(), u_out_edges.template to_container<Edges>());
        },
        std::forward<Output>(out),
        std::forward<Args>(args)...
      );
    }
    template<StrictPhylogenyType Network, NodeMapType Output = NodeMap<typename Network::EdgeSet>, class... Args>
    Output get_sw_edges_map(Args&&... args) const {
      Output result;
      sw_edges_map<Network>(result, std::forward<Args>(args)...);
      return result;
    }

  };

  // given an extension E and a network N, compute an arbitrary extension respecting both N and E
  // NOTE: this can be used to extend partial extensions to the node-set of a network and/or remove nodes from E that do not occur in N
  // NOTE: set extend_only to leave nodes in the extension that do not occur in N
  template<bool extend_only = false, PhylogenyType Network, NodeSetType Nodes>
  Extension apply_to_network(const Extension& ext, const Network& N, const Nodes& nodes_of_N) {
    Extension result;
    DEBUG3(std::cout << "extending "<<ext<<" to nodes "<<nodes_of_N<<" of network\n"<<ExtendedDisplay(N)<<"\n");
    if(!ext.empty()) {
      // we will continue to use a single NodeTraversal, and keep adding to its seen_set
      NodeTraversal<postorder, Network> traversal(N);
      for(const NodeDesc u: ext) {
        if(test(nodes_of_N, u)) {
          // add to the extension all nodes between u and the seen nodes 
          traversal.set_roots(u);
          append(result, traversal);
          DEBUG3(std::cout << "appended traversal from "<<u<<" - result now: "<<result<<"\n");
        } else // if u is not a node of N
          if constexpr (extend_only)
            append(result, u);
      }
    } else append(result, N.nodes_postorder());
    return result;
  }
  template<bool extend_only = false, PhylogenyType Network>
  Extension apply_to_network(const Extension& ext, const Network& N) {
    return apply_to_network(ext, N, N.nodes().template to_container<NodeSet>());
  }

}// namespace

