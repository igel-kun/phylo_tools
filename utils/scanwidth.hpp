
#pragma once

#include "cuts.hpp"
#include "biconnected_comps.hpp"
#include "scanwidthPP.hpp"
#include "scanwidthDP.hpp"

namespace PT{

  template<bool low_memory_version, bool preprocessing, StrictPhylogenyType Network, class RegisterNode, class... ExtracterArgs>
  void _compute_min_sw_extension(const Network& N, RegisterNode&& _register_node, ExtracterArgs&&... args) {
    using EdgeWeight = std::conditional_t<preprocessing, uint32_t, void>;
    using Component = CompatibleNetwork<Network, NodeDesc, EdgeWeight, void>;
    using Adjacency = typename Component::Adjacency;
    
    struct EdgeWeightExtracter { decltype(auto) operator()(const Adjacency& adj) const { if constexpr (preprocessing) return adj.data(); else exit(1); } };
    // biconnected components only have node-data (linking to the original node), but no edge data and no labels
    // NOTE: if we're doing preprocessing, we will need to store edge-weights
    using EdgeWeightExtract = std::conditional_t<preprocessing, EdgeWeightExtracter, void>;
    // if we're preprocessing, we must not ignore deg-2 nodes in the dynamic programming
    using DPType = ScanwidthDP<low_memory_version, const Component, EdgeWeightExtract, !preprocessing>;
    
    DEBUG4(std::cout << "getting biconnected component factory\n");
    const auto bc_components = get_biconnected_components<Component>(N, std::forward<ExtracterArgs>(args)...);
    for(auto& bcc: bc_components){
      DEBUG4(std::cout << "found biconnected comp ("<<bcc.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(bcc) <<"\n");

      if constexpr (preprocessing) {
        apply_sw_preprocessing(bcc);
        DEBUG4(std::cout << "after preprocessing ("<<bcc.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(bcc) <<"\n");
      }
      switch(bcc.num_edges()){
      case 0: break;
      case 1: {
                DEBUG5(std::cout << "only 1 edge, so adding its head to ex\n");
                const auto uv = mstd::front(bcc.edges());
                //const auto& uv = std::front(bcc.edges());
                DEBUG5(std::cout << "edge is "<<uv<<"\n");
                mstd::append(_register_node, node_of<Component>(uv.head()).data());
                break;
              }
      default:{
                DPType dp(bcc);
                dp.compute_min_sw_extension_no_bridges([&](const NodeDesc u){ mstd::append(_register_node, node_of<Component>(u).data()); });
              }
      }
      DEBUG5(std::cout << "done working with\n"; std::cout <<bcc<<"\n");
    }
    mstd::append(_register_node, bc_components.get_begin_end_transformation().extracter(Ex_node_data{}, N.root()));
  }

  template<bool low_memory_version, bool preprocessing, StrictPhylogenyType Network, class RegisterNode>
  void compute_min_sw_extension(const Network& N, RegisterNode&& _register_node) {
    using NodeDataExtract = mstd::IdentityFunction<NodeDesc>;
    if constexpr (preprocessing) {
      using RWNetwork = CompatibleNetwork<Network, NodeDesc, uint32_t, void>;
      // make a copy of N in which all nodes are annotated with their corresponding node in N and all edges have weight 1
      RWNetwork N_copy(N, NodeDataExtract(), [](const NodeDesc, const NodeDesc){return 1;});
      DEBUG3(std::cout << "after copy:\n"<<ExtendedDisplay(N_copy)<<"\n");
      apply_sw_preprocessing(N_copy);
      _compute_min_sw_extension<low_memory_version, preprocessing>(N_copy, std::forward<RegisterNode>(_register_node));
      if constexpr (std::is_same_v<std::remove_reference_t<RegisterNode>, Extension>) {
        _register_node = apply_to_network(_register_node, N);
      }
    } else
      _compute_min_sw_extension<low_memory_version, preprocessing>(N, std::forward<RegisterNode>(_register_node), NodeDataExtract());
  }


}// namespace


