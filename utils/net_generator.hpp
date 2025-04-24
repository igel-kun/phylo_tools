
#pragma once

// compute the generaotr of a network,
// that is, the result of removing pendants and suppressing suppressible nodes
// NOTE: this might contain multi-edges!

#include "edge_emplacement.hpp"
#include "phylogeny.hpp"

namespace PT {

  // our generator may want to know the node correspondance to the original network
  struct GeneratorNodeInfo {
    NodeDesc original_node;
    bool has_leaf;

    friend std::ostream& operator<<(std::ostream& os, const GeneratorNodeInfo& info) {
      os << info.original_node;
      if(info.has_leaf) os << 'L';
      return os;
    }
  };

  template<class Adj>
  struct GeneratorEdgeInfo {
    Adj start_adj; // target of the first edge of the side
    Adj end_adj;   // source of the last edge of the side
    bool has_leaf; // does the side have any leaves?
  };

  template<StrictPhylogenyType Net>
  using GeneratorFor = Phylogeny<vecS, vecS, GeneratorNodeInfo, GeneratorEdgeInfo<typename Net::Adjacency>, void, Net::RootStorage>;


  template<StrictPhylogenyType Net>
  void treat_gen_side(const NodeDesc u, auto& gen_dp_table, auto& get_data, auto& emplacer) {
    using Adjacency = typename Net::Adjacency;
    using EdgeInfo = GeneratorEdgeInfo<Adjacency>;
    using Edge = EdgeOf<Net>;

    // track whether we have seen leaves below u
    NodeDesc u_copy = NoNode;
    bool u_has_leaves = false;
    auto u_it = std::end(gen_dp_table);

    // if u is a reti, then already register it in the generator
    if(Net::in_degree(u) != 1) {
      u_it = append(gen_dp_table, u).first; // register u as endpoint of a side
      u_copy = emplacer.create_copy_of(u); // store the original u in the node data of the copy
      static_cast<GeneratorNodeInfo&>(get_data(u_copy)) = GeneratorNodeInfo{u, false};
    }

    // then, go through the children of u, save the first generator side that u is one and, if u is on another side, u is also a generator-node
    std::optional<EdgeInfo> first_info; // store the first edge on the path towards the first seen generator side
    for(const Adjacency& v: Net::children(u)) {
      const auto v_it = gen_dp_table.find(v.get_desc());
      if(v_it != std::end(gen_dp_table)) {
        auto [side_below_v, side_has_leaves] = v_it->second;

        // if v is itself a generator-node, then...
        if(not side_below_v.has_value()) {
          side_below_v = Edge{u,v}; // ... its last edge needs to be fixed to u->v
          side_has_leaves = false; // ... its leaves are NOT on the generator-side
        }
        assert(emplacer.contains(side_below_v->head()));
        
        DEBUG4(std::cout << "found a side below "<<u<<" towards "<<v.get_desc()<<": "<<*side_below_v<<" & leaves: "<<side_has_leaves<<"\n");

        // we will need this EdgeInfo
        EdgeInfo tmp{v, side_below_v->tail_with_data(), side_has_leaves};
        // if v has a generator node w below it, then either save it for the next time (if it's the first), or make u a generator node (otherwise)
        if(u_it == std::end(gen_dp_table)) {
          DEBUG4(std::cout << "it's the first side\n");
          // if we haven't seen any generator-node below u, then register that the generator-node below v is also below u
          u_it = append(gen_dp_table, u, side_below_v, side_has_leaves).first;
          first_info = std::move(tmp);
        } else {
          // if we have already seen a generator-node below u, then make u a generator node and make generator_below[u] = u
          if(u_copy == NoNode) {
            assert(first_info.has_value());
            assert(not emplacer.contains(u));
            u_copy = emplacer.create_copy_of(u);
            static_cast<GeneratorNodeInfo&>(get_data(u_copy)) = GeneratorNodeInfo{u, u_has_leaves};
            // store the 2 adjacencies in the generator
            const auto uv_adj_iter = emplacer.emplace_edge(u, u_it->second.first->head()).first;
            static_cast<EdgeInfo&>(get_data(u_copy, *uv_adj_iter)) = std::move(*first_info);
            first_info.reset();
            u_it->second.first.reset(); // mark u as being a generator node in the DP-table
          }
          // store in the data: the adjacency (in N) leading to v, the adjacency coming from v, and whether the side has leaves
          const auto uw_adj_iter = emplacer.emplace_edge(u, side_below_v->head()).first;
          static_cast<EdgeInfo&>(get_data(u_copy, *uw_adj_iter)) = std::move(tmp);
        }
      } else { 
        // if v is no generator node and has no generator node below it, then it has leaves below it
        u_has_leaves = true;
        // tell the node-data in the generator that u has leaves
        if(u_copy != NoNode)
          static_cast<GeneratorNodeInfo&>(get_data(u_copy)).has_leaf = true;
      } // if v has a generator node below it
    } // foreach children(u)
    // if u is on a generator side, but not on the generator, and u has leaves, then register that the side has leaves (those of u)
    if(first_info.has_value() && u_has_leaves) {
      DEBUG4(std::cout << "found leaves below "<<u<<" so updating the DP entry\n");
      assert(u_copy == NoNode);
      assert(u_it != std::end(gen_dp_table));
      u_it->second.second = true;
    }
  }

  // compute the generator for the given network N
  // NOTE: provide get_data to access data
  //    if data is stored within the generator nodes & edges, then get_data(x) = Gen.data(x)
  //    if data is stored externally in 2 maps, then get_data(x) = if constexpr (NodeDesc<x>) node_map[x] else edge_map[x]
  template<StrictPhylogenyType Network, class GetData, EdgeEmplacerType Emplacer>
    requires (std::invocable<std::remove_reference_t<GetData>, NodeDesc> &&
              std::invocable<std::remove_reference_t<GetData>, NodeDesc, typename Network::Adjacency>)
  void compute_generator(Network& N, GetData&& get_data, Emplacer&& emplacer) {
    // when first we see a generator node below v, store its information
    // when we see a second generator node below v, then make v a generator node as well
    using Edge = EdgeOf<Network>;
    using DPInfo = std::pair<std::optional<Edge>, bool>; // in the bottom-up DP, store the last edge of the side, and whether the side has leaves
    using SideMap = NodeMap<DPInfo>;
    
    SideMap gen_dp_table;
    for(const NodeDesc u: N.nodes_postorder()) {
      treat_gen_side<Network>(u, gen_dp_table, get_data, emplacer);
    } // forall nodes in post-order
  }

  template<PhylogenyType _Generator = void, class _GetData = void>
  auto compute_generator(auto&& N) requires (PhylogenyType<decltype(N)>) {
    using Network = std::remove_reference_t<decltype(N)>;
    using Generator = mstd::FirstNonVoid<_Generator, GeneratorFor<Network>>;
    using EdgeInfo = GeneratorEdgeInfo<typename Network::Adjacency>;
    static constexpr bool internal_possible = std::is_convertible_v<typename Generator::NodeData, GeneratorNodeInfo> &&
                                              std::is_convertible_v<typename Generator::EdgeData, EdgeInfo>;
    using MyGetData = std::conditional_t<internal_possible, InternalDataAccess<Generator>, ExternalDataAccess<GeneratorNodeInfo, EdgeInfo>>;
    using GetData = mstd::FirstNonVoid<_GetData, MyGetData>;
    static_assert((not std::is_void_v<_Generator>) || internal_possible);

    Generator G;
    compute_generator(N, GetData{}, DefaultEdgeEmplacer<Generator>{G, EO_forbid_junctions + EO_forbid_non_binary});
    DEBUG1(std::cout << "computed generator:\n" << ExtendedDisplay(G) << '\n');
    return G;
  }
}
