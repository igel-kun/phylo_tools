
#pragma once

#include "types.hpp"
#include "node.hpp"

namespace PT {

  struct Ex_node_label {};
  struct Ex_node_data {};
  struct Ex_edge_data {};

  template<class T> constexpr bool is_node_label_tag = std::is_same_v<T, Ex_node_label>;
  template<class T> constexpr bool is_node_data_tag = std::is_same_v<T, Ex_node_data>;
  template<class T> constexpr bool is_edge_data_tag = std::is_same_v<T, Ex_edge_data>;
  template<class T> concept DataTag = (is_node_label_tag<T> || is_node_data_tag<T> || is_edge_data_tag<T>);

  template<OptionalPhylogenyType Phylo> struct _NodeLabelOf { using type = std::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::LabelType>; };
  template<OptionalPhylogenyType Phylo> struct _NodeDataOf { using type = std::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::NodeData>; };
  template<OptionalPhylogenyType Phylo> struct _EdgeDataOf { using type = std::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::EdgeData>; };
  template<> struct _NodeLabelOf<void> { using type = void; };
  template<> struct _NodeDataOf<void> { using type = void; };
  template<> struct _EdgeDataOf<void> { using type = void; };

  template<OptionalPhylogenyType Phylo> using NodeLabelOf = typename _NodeLabelOf<Phylo>::type;
  template<OptionalPhylogenyType Phylo> using NodeDataOf = typename _NodeDataOf<Phylo>::type;
  template<OptionalPhylogenyType Phylo> using EdgeDataOf = typename _EdgeDataOf<Phylo>::type;

  template<OptionalPhylogenyType Phylo> constexpr bool HasNodeLabel = !std::is_void_v<NodeLabelOf<Phylo>>;
  template<OptionalPhylogenyType Phylo> constexpr bool HasNodeData  = !std::is_void_v<NodeDataOf<Phylo>>;
  template<OptionalPhylogenyType Phylo> constexpr bool HasEdgeData  = !std::is_void_v<EdgeDataOf<Phylo>>;


  template<DataTag Tag, OptionalPhylogenyType Phylo>
  using DataTypeOf = std::conditional_t<is_node_label_tag<Tag>,
                            NodeLabelOf<Phylo>,
                            std::conditional_t<is_node_data_tag<Tag>,
                                    NodeDataOf<Phylo>,
                                    EdgeDataOf<Phylo>>>;
 
  template<DataTag Tag, OptionalPhylogenyType Phylo>
  using ReturnableDataTypeOf = std::ReturnableType<DataTypeOf<Tag, Phylo>>;
 
  template<DataTag Tag, OptionalPhylogenyType Phylo> constexpr bool HasDataType = !std::is_void_v<DataTypeOf<Tag, Phylo>>;


  template<DataTag Tag, OptionalPhylogenyType Phylo>
  struct _DefaultExtractData {};

  // NOTE: when passing an rvalue-reference as Phylo, we will try to std::move the data out of that Phylogeny
  template<OptionalPhylogenyType Phylo>
  struct _DefaultExtractData<Ex_node_label, Phylo> { auto&& operator()(const NodeDesc u) { return node_of<Phylo>(u).label(); } };

  template<OptionalPhylogenyType Phylo>
  struct _DefaultExtractData<Ex_node_data, Phylo> { auto&& operator()(const NodeDesc u) { return node_of<Phylo>(u).data(); } };

  template<OptionalPhylogenyType Phylo>
  struct _DefaultExtractData<Ex_edge_data, Phylo> {
    // NOTE: we have to be able to tell EdgeDataExtractors from NodeDataExtractors, so we use the following requirement:
    template<class Edge> requires (!std::is_same_v<std::remove_cvref_t<Edge>, NodeDesc>)
    auto& operator()(Edge&& uv) { return uv.data(); }

    auto& operator()(const NodeDesc u, const NodeDesc v) {
      auto& u_children = node_of<Phylo>(u).children();
      const auto iter = std::find(u_children, v);
      if(iter == u_children.end()) throw std::logic_error("trying to get data from a non-edge");
      return iter->data();
    }
  };

  template<DataTag Tag, OptionalPhylogenyType Phylo>
  struct IgnoreExtractionFunction {
    auto operator()(const NodeDesc&) const { return ReturnableDataTypeOf<Tag, Phylo>(); }
  };
  template<OptionalPhylogenyType Phylo> struct IgnoreExtractionFunction<Ex_edge_data, Phylo> {
    // NOTE: we have to be able to tell EdgeDataExtractors from NodeDataExtractors, so we use the following requirement:
    template<class Edge> requires (!std::is_same_v<std::remove_cvref_t<Edge>, NodeDesc>)
    auto operator()(Edge&& uv) { return ReturnableDataTypeOf<Ex_edge_data, Phylo>(); }
    auto operator()(const NodeDesc u, const NodeDesc v) { return ReturnableDataTypeOf<Ex_edge_data, Phylo>(); }
  };


  // NOTE: if the source phylo has data but the target phylo does not, it's no use extracting said data
  // NOTE: if the target phylo has data but the source phylo does not, then the data is default constructed by the std::IgnoreFunction
  template<DataTag Tag, OptionalPhylogenyType SourcePhylo, OptionalPhylogenyType TargetPhylo = SourcePhylo>
  using DefaultExtractData = std::conditional_t<HasDataType<Tag, SourcePhylo> && HasDataType<Tag, TargetPhylo>,
                                  _DefaultExtractData<Tag, std::remove_reference_t<SourcePhylo>>,
                                  IgnoreExtractionFunction<Tag, TargetPhylo>>;


  

/*
 * this is a container class storing functions to extract node-data, edge-data, and node-labels
 * depending on the number and type of constructor parameters and depending on a Network type, the 3 functions are initialized smartly.
 */
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeLabel  = void>
  struct DataExtracter_nl {
    using ExtractNodeLabel = std::conditional_t<std::is_void_v<_ExtractNodeLabel>, DefaultExtractData<Ex_node_label, Network>, _ExtractNodeLabel>;
    static constexpr bool custom_node_label_maker = !std::is_same_v<ExtractNodeLabel, DefaultExtractData<Ex_node_label, Network>>;
    ExtractNodeLabel get_node_label;
  };

  template<OptionalPhylogenyType Network,
    class _ExtractEdgeData                    = void,
    OptionalNodeFunctionType ExtractNodeLabel = void>
  struct DataExtracter_ed_nl: public DataExtracter_nl<Network, ExtractNodeLabel> {
    using Parent = DataExtracter_nl<Network, ExtractNodeLabel>;
    using ExtractEdgeData = std::conditional_t<std::is_void_v<_ExtractEdgeData>, DefaultExtractData<Ex_edge_data, Network>, _ExtractEdgeData>;
    
    static constexpr bool custom_edge_data_maker  = !std::is_same_v<ExtractEdgeData,  DefaultExtractData<Ex_edge_data, Network>>;
    using Parent::custom_node_label_maker;
    
    ExtractEdgeData get_edge_data;

    DataExtracter_ed_nl() = default;
    DataExtracter_ed_nl(const DataExtracter_ed_nl&) = default;
    DataExtracter_ed_nl(DataExtracter_ed_nl&&) = default;

    template<class... Args> requires (!custom_node_label_maker) && (!custom_node_label_maker)
    DataExtracter_ed_nl(Args&&... args):
      Parent(),
      get_edge_data(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_edge_data_maker) && custom_node_label_maker
    DataExtracter_ed_nl(Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_edge_data()
    {}

    template<class First, class... Args> requires (!NodeFunctionType<First> && custom_node_label_maker && custom_edge_data_maker)
    DataExtracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_edge_data(std::forward<First>(first))
    {}

    // NOTE: we wont try to initialize our ExtractEdgeData with a NodeFunctionType
    //       instead, we pass that to the ExtractNodeLabel function and use the rest to initialize our ExtractEdgeData
    template<NodeFunctionType First, class... Args>
    DataExtracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<First>(first)),
      get_edge_data(std::forward<Args>(args)...)
    {}

    template<class First, NodeFunctionType Second>
    DataExtracter_ed_nl(First&& first, Second&& second):
      Parent(std::forward<Second>(second)),
      get_edge_data(std::forward<First>(first))
    {}

  };


  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeData = void,
           class ExtractEdgeData                     = void,
           OptionalNodeFunctionType ExtractNodeLabel = void>
  struct DataExtracter: public DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>;
    using ExtractNodeData = std::conditional_t<std::is_void_v<_ExtractNodeData>, DefaultExtractData<Ex_node_data, Network>, _ExtractNodeData>;

    using Parent::custom_node_label_maker;
    using Parent::custom_edge_data_maker;
    static constexpr bool custom_node_data_maker = !std::is_same_v<ExtractNodeData, DefaultExtractData<Ex_node_data, Network>>;

    ExtractNodeData get_node_data;


    DataExtracter() = default;
    DataExtracter(const DataExtracter&) = default;
    DataExtracter(DataExtracter&&) = default;

    template<class... Args> requires (custom_node_data_maker && !custom_node_label_maker && !custom_edge_data_maker)
    DataExtracter(Args&&... args):
      Parent(),
      get_node_data(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_node_data_maker)
    DataExtracter(Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data()
    {}

    template<class First, class... Args> requires (custom_node_data_maker && (custom_edge_data_maker || custom_node_label_maker))
    DataExtracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data(std::forward<First>(first))
    {}

    template<NodeFunctionType First, class... Args> requires (sizeof...(Args) >= 2)
    DataExtracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data(std::forward<First>(first))
    {}

  };

  // NOTE: use make_data_extracter to smartly construct a DataExtracter:
  // make_data_extracter(Ex_node_data, X) and make_data_extracter(Ex_node_label, X)
  //    X is used to extract NODE DATA and NODE LABELS, respectively
  // make_data_extracter(X):
  //    if X can be called with a NodeDesc and Network has node data, then X is used to extract NODE DATA from Networks
  //    if X can be called with a NodeDesc and Network does not have node data, then X is used to extract NODE LABELS
  //    if X cannot be called with a NodeDesc, then X is used to extract EDGE DATA
  // make_data_extracter(X, Y)
  //    if both X and Y can be called with a NodeDesc, then X extracts NODE DATA and Y extracts NODE LABELS
  //    if X can be called  with a NodeDesc but Y can not, then the rules for make_data_extracter(X) apply to X and Y extracts EDGE DATA
  // make_data_extracter(X, Y, Z)
  //    X extracts NODE DATA, Y extracts EDGE DATA, and Z extracts NODE LABELS
  // NOTE: all extractions that are not passed to the functions are set to defaults, you can even call make_data_extractor() to set all to defaults
  template<OptionalPhylogenyType Network,
           NodeFunctionType ExtractNodeData,
           class ExtractEdgeData,
           NodeFunctionType ExtractNodeLabel>
  auto make_data_extracter(ExtractNodeData&& get_node_data, ExtractEdgeData&& get_edge_data, ExtractNodeLabel&& get_node_label) {
    using ReturnExtracter = DataExtracter<Network, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>;
    return ReturnExtracter(std::forward<ExtractNodeData>(get_node_data),
                           std::forward<ExtractEdgeData>(get_edge_data),
                           std::forward<ExtractNodeLabel>(get_node_label));
  }

  // if 2 NodeFunctionTypes are provided, the first is interpreted as DataExtract and the second as LabelExtract
  template<OptionalPhylogenyType Network,
           NodeFunctionType ExtractNodeData,
           NodeFunctionType ExtractNodeLabel>
  auto make_data_extracter(ExtractNodeData&& get_node_data, ExtractNodeLabel&& get_node_label) {
    using ReturnExtracter = DataExtracter<Network, ExtractNodeData, void, ExtractNodeLabel>;
    return ReturnExtracter(std::forward<ExtractNodeData>(get_node_data), std::forward<ExtractNodeLabel>(get_node_label));
  }

  // if only 1 NodeFunctionType is given, the user can specify how to interpret it by passing either the Ex_node_label or the Ex_node_data tag
  template<OptionalPhylogenyType Network,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData = DefaultExtractData<Ex_edge_data, Network>> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(Ex_node_label, ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data = ExtractEdgeData()) {
    using ReturnExtracter = DataExtracter<Network, void, ExtractEdgeData, ExtractNodeSomething>;
    if constexpr (ReturnExtracter::custom_edge_data_maker)
      return ReturnExtracter(std::forward<ExtractEdgeData>(get_edge_data), std::forward<ExtractNodeSomething>(nds));
    else
      return ReturnExtracter(std::forward<ExtractNodeSomething>(nds));
  }

  template<OptionalPhylogenyType Network,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData = DefaultExtractData<Ex_edge_data, Network>> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(Ex_node_data, ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data = ExtractEdgeData()) {
    using ReturnExtracter = DataExtracter<Network, ExtractNodeSomething, ExtractEdgeData, void>;
    if constexpr (ReturnExtracter::custom_edge_data_maker)
      return ReturnExtracter(std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
    else
      return ReturnExtracter(std::forward<ExtractNodeSomething>(nds));
  }

  // if only 1 NodeFunctionType is given, it is interpreted as node-data-extract, unless Network has no node data
  template<OptionalPhylogenyType Network,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData = DefaultExtractData<Ex_edge_data, Network>> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data = ExtractEdgeData()) {
    if constexpr (std::is_void_v<Network>)
      return make_data_extracter<Network, ExtractNodeSomething, ExtractEdgeData>
        (Ex_node_data{}, std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
    else if constexpr (Network::has_node_data)
      return make_data_extracter<Network, ExtractNodeSomething, ExtractEdgeData>
        (Ex_node_data{}, std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
    else
      return make_data_extracter<Network, ExtractNodeSomething, ExtractEdgeData>
        (Ex_node_label{}, std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
  }

  template<OptionalPhylogenyType Network, class ExtractEdgeData = DefaultExtractData<Ex_edge_data, Network>> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(ExtractEdgeData&& get_edge_data = ExtractEdgeData()) {
    return DataExtracter<Network, void, ExtractEdgeData, void>(std::forward<ExtractEdgeData>(get_edge_data));
  }



  template<class T>
  concept DataExtracterType = requires(T t, NodeDesc n) {
    t.get_node_label(n);
    t.get_node_data(n);
  };

}
