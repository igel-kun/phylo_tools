
#pragma once

#include "types.hpp"
#include "node.hpp"

namespace PT {
  enum ExDataType { Ex_node_label = 0, Ex_node_data = 1, Ex_edge_data = 2};

  template<PhylogenyType PhyloRef, ExDataType data_type>
  struct ExtractData {
    using Phylo = std::remove_reference_t<PhyloRef>;
    using Node = typename Phylo::Node;
    using Edge = typename Phylo::Edge;

    using NodeLabelReturnType = NodeLabelOr<PhyloRef>;
    using NodeDataReturnType  = NodeDataOr<PhyloRef>;
    using EdgeDataReturnType  = EdgeDataOr<PhyloRef>;
    using ReturnType = std::conditional_t<data_type == Ex_node_label, NodeLabelReturnType,
          std::conditional_t<data_type == Ex_node_data, NodeDataReturnType, EdgeDataReturnType>>;

    static NodeLabelReturnType extract_node_label(const NodeDesc u) { if constexpr (Node::has_label) return node_of<Phylo>(u).label(); else return NoData(); }
    static NodeDataReturnType  extract_node_data(const NodeDesc u) { if constexpr (Node::has_data) return node_of<Phylo>(u).data(); else return NoData(); }
    static EdgeDataReturnType  extract_edge_data(const Edge& uv) { if constexpr (Edge::has_data) return uv.data(); else return NoData(); }

    ReturnType operator()(const auto& x) const {
      if constexpr (data_type == Ex_node_label) {
        return extract_node_label(x);
      } else if constexpr (data_type == Ex_node_data) {
        return extract_node_data(x);
      } else return extract_edge_data(x);
    }

  };


/*
 * this is a container class storing functions to extract node-data, edge-data, and node-labels
 * depending on the number and type of constructor parameters and depending on a Network type, the 3 functions are initialized smartly.
 */

  template<StrictPhylogenyType Network, NodeFunctionType ExtractNodeLabel  = ExtractData<Network, Ex_node_label>>
  struct Extracter_nl {
    ExtractNodeLabel nle;
    static constexpr bool custom_node_label_maker = !std::is_same_v<ExtractNodeLabel, ExtractData<Network, Ex_node_label>>;
  };

  template<StrictPhylogenyType Network,
    class ExtractEdgeData             = ExtractData<Network, Ex_edge_data>,
    NodeFunctionType ExtractNodeLabel = ExtractData<Network, Ex_node_label>>
  struct Extracter_ed_nl: public Extracter_nl<Network, ExtractNodeLabel> {
    using Parent = Extracter_nl<Network, ExtractNodeLabel>;
    using Parent::custom_node_label_maker;
    static constexpr bool custom_edge_data_maker  = !std::is_same_v<ExtractEdgeData,  ExtractData<Network, Ex_edge_data>>;
    
    ExtractEdgeData ede;

    Extracter_ed_nl() = default;
    Extracter_ed_nl(const Extracter_ed_nl&) = default;
    Extracter_ed_nl(Extracter_ed_nl&&) = default;

    template<class... Args> requires (!custom_node_label_maker)
    Extracter_ed_nl(Args&&... args):
      Parent(),
      ede(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_edge_data_maker)
    Extracter_ed_nl(Args&&... args):
      Parent(std::forward<Args>(args)...),
      ede()
    {}

    template<class First, class... Args> requires (!NodeFunctionType<First> && custom_node_label_maker && custom_edge_data_maker)
    Extracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      ede(std::forward<First>(first))
    {}

    // NOTE: we wont try to initialize our ExtractEdgeData with a NodeFunctionType
    //       instead, we pass that to the ExtractNodeLabel function and use the rest to initialize our ExtractEdgeData
    template<NodeFunctionType First, class... Args>
    Extracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<First>(first)),
      ede(std::forward<Args>(args)...)
    {}

    template<class First, NodeFunctionType Second>
    Extracter_ed_nl(First&& first, Second&& second):
      Parent(std::forward<Second>(second)),
      ede(std::forward<First>(first))
    {}

  };


  template<StrictPhylogenyType Network,
           NodeFunctionType ExtractNodeData  = ExtractData<Network, Ex_node_data>,
           class ExtractEdgeData             = ExtractData<Network, Ex_edge_data>,
           NodeFunctionType ExtractNodeLabel = ExtractData<Network, Ex_node_label>>
  struct Extracter: public Extracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = Extracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>;
    using Parent::custom_node_label_maker;
    using Parent::custom_edge_data_maker;
    static constexpr bool custom_node_data_maker = !std::is_same_v<ExtractNodeData, ExtractData<Network, Ex_node_data>>;

    ExtractNodeData nde;


    Extracter() = default;
    Extracter(const Extracter&) = default;
    Extracter(Extracter&&) = default;

    template<class... Args> requires (custom_node_data_maker && !custom_node_label_maker && !custom_edge_data_maker)
    Extracter(Args&&... args):
      Parent(),
      nde(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_node_data_maker)
    Extracter(Args&&... args):
      Parent(std::forward<Args>(args)...),
      nde()
    {}

    template<class First, class... Args> requires (custom_node_data_maker && (custom_edge_data_maker || custom_node_label_maker))
    Extracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      nde(std::forward<First>(first))
    {}

    template<NodeFunctionType First, class... Args> requires (sizeof...(Args) >= 2)
    Extracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      nde(std::forward<First>(first))
    {}

  };

}
