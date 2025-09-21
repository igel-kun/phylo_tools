
#pragma once

#include "cuts.hpp"
#include "biconnected_comps.hpp"
#include "scanwidthPP.hpp"
#include "scanwidthDP2.hpp"

namespace PT{

  enum SWconfig {
    sw_default = 0x00,

    // recompute the "weakly-connected" relation of the partial extensions in the DP-table entries
    // instead of storing them with their extensions
    sw_low_mem_footprint = 0x01,

    // do not use preprocessing
    sw_no_preprocess = 0x02,

    // ue bottom-up DP instead of top-down DP
    sw_bottom_up = 0x04
  };
  // we want to write "sw_no_preprocess + sw_bottom_up"
  constexpr SWconfig operator+(const SWconfig x, const SWconfig y) {
    return static_cast<SWconfig>(static_cast<int>(x) + static_cast<int>(y));
  }

  template<bool low_mem, bool no_prep, bool bottom_up>
  constexpr SWconfig make_sw_config = low_mem * int{sw_low_mem_footprint} + sw_no_preprocess * int{no_prep} + sw_bottom_up * int{bottom_up};

  template<class Adjacency>
  struct EdgeWeightExtracter {
    decltype(auto) operator()(const Adjacency& adj) const { return adj.data();}
  };


  template<SWconfig config, StrictPhylogenyType Network, class RegisterNode>
  void _compute_min_sw_extension(const Network& N, RegisterNode&& _register_node) {
    static constexpr bool preprocess = (config & sw_no_preprocess) == 0;
    static constexpr bool low_mem    = (config & sw_low_mem_footprint) != 0;
    static constexpr bool bottom_up  = (config & sw_bottom_up) != 0;
    
    using EdgeWeight = std::conditional_t<preprocess, uint32_t, void>;
    using Component = CompatibleNetwork<Network, NodeDesc, EdgeWeight, void>;
    using Adjacency = typename Component::Adjacency;

    // biconnected components only have node-data (linking to the original node), but no edge data and no labels
    // NOTE: if we're doing preprocessing, we will need to store edge-weights
    using EdgeWeightExtract = std::conditional_t<preprocess, EdgeWeightExtracter<Adjacency>, void>;
    // if we're preprocessing, we must not ignore deg-2 nodes in the dynamic programming
    using DPType = std::conditional_t<bottom_up,
          ScanwidthDP<low_mem, const Component, EdgeWeightExtract, !preprocess>,
          ScanwidthDP2<low_mem, const Component, EdgeWeightExtract>>;
    
    DEBUG4(std::cout << "getting biconnected component factory\n");
    // NOTE: we're extracting u's NodeDesc in order to store it in the corresponding node in the BCC as data
    const auto bc_components = get_biconnected_components<Component>(N, mstd::IdentityFunction<NodeDesc>());
    for(auto& bcc: std::move(bc_components)){
      DEBUG4(std::cout << "found biconnected comp ("<<bcc.num_nodes()<<" nodes):\n"; std::cout << ExtendedDisplay(bcc) <<"\n");

      if constexpr (preprocess) {
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
                mstd::append(_register_node, Component::node_of(uv.head()).data());
                break;
              }
      default:{
                DPType dp(bcc);
                dp.compute_min_sw_extension_no_bridges([&](const NodeDesc u){ mstd::append(_register_node, Component::node_of(u).data()); });
              }
      }
      DEBUG5(std::cout << "done working with\n"; std::cout <<bcc<<"\n");
    }
    // finally register the root manually
    mstd::append(_register_node, N.root());
  }

#warning "TODO: move the initial preprocessing into the construction of the DP?"

  template<SWconfig sw_config, StrictPhylogenyType Network, class RegisterNode>
  void compute_min_sw_extension(const Network& N, RegisterNode&& _register_node) {
    if constexpr ((sw_config & sw_no_preprocess) == 0) {
      using RWNetwork = CompatibleNetwork<Network, NodeDesc, uint32_t, void>;
      // make a copy of N in which all nodes are annotated with their corresponding node in N and all edges have weight 1
      RWNetwork N_copy(N, mstd::IdentityFunction<NodeDesc>(), [](const NodeDesc, const NodeDesc){return 1;});
      DEBUG3(std::cout << "after copy:\n"<<ExtendedDisplay(N_copy)<<"\n");
      apply_sw_preprocessing(N_copy);
      _compute_min_sw_extension<sw_config>(N_copy, std::forward<RegisterNode>(_register_node));
      if constexpr (std::is_same_v<std::remove_reference_t<RegisterNode>, Extension>) {
        _register_node = apply_to_network(_register_node, N);
      }
    } else
      _compute_min_sw_extension<sw_config>(N, std::forward<RegisterNode>(_register_node));
  }


}// namespace


