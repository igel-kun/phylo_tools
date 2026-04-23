
#pragma once

// compute the generator of a network,
// that is, the result of removing pendants and suppressing suppressible nodes
// NOTE: this might contain multi-edges!

#include "edge_emplacement.hpp"
#include "phylogeny.hpp"

namespace PT {


  // an informed generator knows the following:
  // (1) for each generator node gu:
  //  (a) which node in the network is represented by gu
  //  (b) does the represented node have paths to leaves avoiding generator sides? ("private leaves")
  // (2) for each generator edge guv:
  //  (a) the source-adjacency of the last edge on the side
  //  (b) the target-adjacency of the first edge on the side
  //  (c) whether any node on the side can reach a leaf with a path avoiding generator sides
  namespace InformedGenerator {
    // our generator may want to know the node correspondance to the original network
    struct NodeInfo {
      NodeDesc original_node = NoNode;
      bool has_leaf = false;

      friend std::ostream& operator<<(std::ostream& os, const NodeInfo& info) {
        os << info.original_node;
        if(info.has_leaf) os << 'L';
        return os;
      }
    };

    template<AdjacencyType Adj>
    struct EdgeInfo {
      Adj start_adj; // target of the first edge of the side
      Adj end_adj;   // source of the last edge of the side
      bool side_has_leaf = false; // does the side have any leaves?
    };

    template<AdjacencyType Adj>
    struct Accu:
      public NodeInfo, EdgeInfo<Adj>
    {
      // NOTE: this has side-effects on 'other': if u is a generator-node, then we update other's start_adjacency
      void operator()(const auto& uv, Accu& other, const NodeDesc u_nearest_gen, const NodeDesc v_nearest_gen) {
        const bool u_is_on_gen_side = (u_nearest_gen != NoNode);
        const bool u_is_gen_node = (u_nearest_gen == uv.tail());
        const bool v_is_on_gen_side = (v_nearest_gen != NoNode);
        const bool v_is_gen_node = (v_nearest_gen == uv.head());

        // bonus: update other's start-adjacency
        if(u_is_gen_node and v_is_on_gen_side)
          other.start_adj = uv.head();
        if(v_is_gen_node)
          other.end_adj = uv.get_reversed().head();

        // update NodeInfos and EdgeInfos
        this->original_node = uv.tail();
        if(u_is_on_gen_side) {
          if(v_is_on_gen_side) {
            if(not v_is_gen_node)
              this->side_has_leaf |= other->side_has_leaf or other->has_leaf;
          } else this->has_leaf = true;
        }
      }
    }; // struct Accu
  } // namespace InformedGenerator

  using DefaultGenerator = Network<vecS, vecS>;

  template<StrictPhylogenyType Net>
  using InformedGeneratorFor = Phylogeny<vecS, vecS, InformedGenerator::NodeInfo, InformedGenerator::EdgeInfo<typename Net::Adjacency>, void, Net::RootStorage>;

  // We're using 'DataAccu' to accumulate data from nodes and edges below each generator node:
  // We default-initialize DataAccu at the leaves of the network and repeatedly call
  //    operator()(Net::Edge, DataAccu, u's nearest gen node, v's nearest gen node)
  // to accumulate, until we find a generator node u. At this point, u's NodeData is the result of casting the DataAccu to NodeData.
  // The EdgeData of each edge outgoing from u is the result of casting the DataAccu to EdgeData when we encounter uv.
  template<StrictPhylogenyType _Generator, class _DataAccu>
  struct GeneratorMaker {
    using Generator = _Generator;
    using GeneratorEdge = typename Generator::Edge;
    using GenNodeData = typename Generator::NodeData;
    using GenEdgeData = typename Generator::EdgeData;
    static constexpr bool has_node_data = Generator::has_node_data;
    static constexpr bool has_edge_data = Generator::has_edge_data;

    static constexpr bool has_data_accu = (has_node_data or has_edge_data) and not std::is_void_v<_DataAccu>;
    using DataAccu = std::conditional_t<has_data_accu, _DataAccu, mstd::monostate>;
    using DataAndNode = std::pair<DataAccu, NodeDesc>;

    // construct the generator for the network N
    // NOTE: generator node-data = result of accumulating data of all nodes between itself and other generator nodes
    //       generator edge-data(uv) = result of accumulating all edge-data (in postorder) on paths starting with uv and avoiding generator nodes != u
    // NOTE: we'll do a bottom-up traversal, accumulating node- and edge-data, sticking the results as node- and edge-data into the generator elements
    template<StrictPhylogenyType Net, class... EmplacerArgs>
    static Generator make_generator(const Net& N, const DataAccu& init_accu, EmplacerArgs&&... args) {
      Generator G;
      auto emplacer = EdgeEmplacers<true, Net>::make_emplacer(G, std::forward<EmplacerArgs>(args)...);
      
      assert(N.num_roots() == 1);
      NodeSet seen;
      construct_generator_below<Net>(N.root(), emplacer, seen, init_accu);
      return G;
    }
    template<StrictPhylogenyType Net, class First, class... EmplacerArgs> requires (not mstd::is_same_v<First, DataAccu>)
    static Generator make_generator(const Net& N, First&& first, EmplacerArgs&&... args) {
      return make_generator(N, DataAccu{}, std::forward<First>(first), std::forward<EmplacerArgs>(args)...);
    }
    template<StrictPhylogenyType Net>
    static Generator make_generator(const Net& N) { return make_generator(N, DataAccu{}); }


    static GenNodeData& to_node_data(DataAccu& data) { return static_cast<GenNodeData&>(data); }
    static GenEdgeData& to_edge_data(DataAccu& data) { return static_cast<GenEdgeData&>(data); }

    // treat the network below u
    // return the accumulated NodeInfo and EdgeInfo below u
    template<StrictPhylogenyType Net, EdgeEmplacerType Emplacer>
    static DataAndNode construct_generator_below(const NodeDesc u, Emplacer& emplacer, NodeSet& seen, const DataAccu& init_accu) {
      using NetworkEdge = typename Net::Edge;
      
      // NOTE: When we see the first child v of u that's on a generator side, we cannot immediately tell if u is a generator node.
      //       Thus, we defer adding the state and data of v until we see a second generator side, or we treated all children
      // NOTE: the "state" is simply the nearest generator node below, which might be NoNode
      DataAndNode u_state{init_accu, NoNode};
      std::vector<std::pair<NetworkEdge, DataAndNode>> child_states;
      child_states.reserve(Net::out_degree(u));

      // step 1: recurse for all children and find out whether u is a generator node
      for(const auto uv: Net::out_edges(u)) {
        const NodeDesc v = uv.head();
        const bool v_unseen = append(seen, v).second;
        if(v_unseen) {
          // recurse for v and use v's state to update u's state
          auto v_state = construct_generator_below<Net>(v, emplacer, seen, init_accu);

          // update u's nearest generator node
          if(v_state.second != NoNode) {
            if(u_state.second != NoNode) {
              u_state.second = u;
            } else u_state.second = v_state.second;
          }

          // append the data to the child_states
          append(child_states, std::move(uv), std::move(v_state));
        } else append(child_states, std::move(uv), DataAndNode{DataAccu{}, v});
      }

      // step 3: accumulate the child_state data into u's data
      if constexpr (has_data_accu)
        for(auto& [uv, v_state]: child_states)
          u_state.first(uv, v_state.first, u_state.second, v_state.second);

      // step 4: if u is a generator node, then install the actual edges in the generator, using the DataAccus as EdgeData
      if(u_state.second == u) {
        // first, construct u in the generator
        NodeDesc u_copy;
        if constexpr (has_node_data and has_data_accu)
          u_copy = emplacer.create_copy_of(u, static_cast<const GenNodeData&>(to_node_data(u_state.first)));
        else u_copy = emplacer.create_copy_of(u);

        for(auto& [uv, v_state]: child_states) 
          if(v_state.second != NoNode) {
            const NodeDesc v = uv.head();
            // if uv is on a generator-side, then construct this side in the generator
            // NOTE: the edge-data is constructed by casting the DataAccumulator to EdgeData
            if constexpr (has_edge_data and has_data_accu) {
              emplacer.emplace_edge_raw(u_copy, emplacer.create_copy_of(v), static_cast<GenEdgeData&&>(to_edge_data(v_state.first)));
            } else emplacer.emplace_edge_raw(u_copy, emplacer.create_copy_of(v));
          }
      }
      return u_state;
    }

  };


}
