
#pragma once

#include "types.hpp"
#include "node.hpp"
#include "tags.hpp"

namespace PT {

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

#warning "TODO: can we turn those into templates of constexpr lambdas?"
#warning "TODO: when passing an rvalue-reference as Phylo, we should std::move the data out of its nodes"
  template<PhylogenyType Phylo>
  struct _DefaultExtractData<Ex_node_label, Phylo> { auto& operator()(const NodeDesc u) const { return node_of<Phylo>(u).label(); } };

  template<PhylogenyType Phylo>
  struct _DefaultExtractData<Ex_node_data, Phylo> { auto& operator()(const NodeDesc u) const { return node_of<Phylo>(u).data(); } };

  template<PhylogenyType Phylo>
  struct _DefaultExtractData<Ex_edge_data, Phylo> {
    // NOTE: we have to be able to tell EdgeDataExtractors from NodeDataExtractors, so we use the following requirement:
    template<LooseEdgeType Edge>
    auto& operator()(Edge&& uv) const { return uv.data(); }

    template<AdjacencyType Adj> requires (!std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(const NodeDesc u, Adj&& v) const { return v.data(); }

    auto& operator()(const NodeDesc u, const NodeDesc v) const {
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
  template<OptionalPhylogenyType Phylo>
  struct IgnoreExtractionFunction<Ex_edge_data, Phylo> {
    // NOTE: we have to be able to tell EdgeDataExtractors from NodeDataExtractors, so we use the following requirement:
    template<class Edge> requires (!std::is_same_v<std::remove_cvref_t<Edge>, NodeDesc>)
    auto operator()(Edge&& uv) { return ReturnableDataTypeOf<Ex_edge_data, Phylo>(); }
    auto operator()(const NodeDesc u, const NodeDesc v) { return ReturnableDataTypeOf<Ex_edge_data, Phylo>(); }
  };


  // NOTE: if the source phylo has data but the target phylo does not, it's no use extracting said data
  // NOTE: if the target phylo has data but the source phylo does not, then the data will be default constructed without any extraction function
  template<DataTag Tag, OptionalPhylogenyType SourcePhylo, OptionalPhylogenyType TargetPhylo = SourcePhylo>
  using DefaultExtractData = std::conditional_t<
                                  std::is_constructible_v<DataTypeOf<Tag, TargetPhylo>, DataTypeOf<Tag, SourcePhylo>>,
                                  _DefaultExtractData<Tag, std::remove_reference_t<SourcePhylo>>,
                                  void>;
//                                  IgnoreExtractionFunction<Tag, TargetPhylo>>;


  

/*
 * this is a container class storing functions to extract node-data, edge-data, and node-labels
 * depending on the number and type of constructor parameters and depending on a Network type, the 3 functions are initialized smartly.
 */
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeLabel  = void>
  struct _DataExtracter_nl: public _ExtractNodeLabel {
    using ExtractNodeLabel = _ExtractNodeLabel;
    using ExtractNodeLabel::ExtractNodeLabel;
    using NodeLabelReturnType = std::invoke_result_t<ExtractNodeLabel::operator(), NodeDesc>;
    using ConstNodeLabelReturnType = std::invoke_result_t<const ExtractNodeLabel::operator(), NodeDesc>;

    static constexpr bool custom_node_label_maker = !std::is_same_v<ExtractNodeLabel, DefaultExtractData<Ex_node_label, Network>>;
    static constexpr bool ignoring_node_labels = false;

    NodeLabelReturnType operator()(const Ex_node_label, const NodeDesc u) { return ExtractNodeLabel::operator()(u); }
    NodeLabelReturnType operator()(const Ex_node_label, const NodeDesc u) const { return ExtractNodeLabel::operator()(u); }
    NodeLabelReturnType get_node_label()(const NodeDesc u) { return ExtractNodeLabel::operator()(u); }
    NodeLabelReturnType get_node_label()(const NodeDesc u) const { return ExtractNodeLabel::operator()(u); }
  };
  template<OptionalPhylogenyType Network>
  struct _DataExtracter_nl<Network, void> {
    using ExtractNodeLabel = void;
    using NodeLabelReturnType = bool;
    using ConstNodeLabelReturnType = bool;
    static constexpr bool custom_node_label_maker = false;
    static constexpr bool ignoring_node_labels = true;
  };





  template<OptionalPhylogenyType Network,
    class _ExtractEdgeData                    = void,
    OptionalNodeFunctionType ExtractNodeLabel = void>
  struct _DataExtracter_ed_nl: public _DataExtracter_nl<Network, ExtractNodeLabel>, public _ExtractEdgeData {
    using Parent = _DataExtracter_nl<Network, ExtractNodeLabel>;
    using ExtractEdgeData = _ExtractEdgeData;
    using Parent::custom_node_label_maker;
 
    template<class... Args>
    using EdgeDataReturnType = decltype(std::declval<ExtractEdgeData>().operator()(std::declval<Args>()...));

    static constexpr bool custom_edge_data_maker  = !std::is_same_v<ExtractEdgeData,  DefaultExtractData<Ex_edge_data, Network>>;
    static constexpr bool ignoring_edge_data = false;
    
    _DataExtracter_ed_nl() = default;

    template<class... Args> requires (!custom_edge_data_maker && !custom_node_label_maker)
    _DataExtracter_ed_nl(Args&&... args):
      Parent(),
      ExtractEdgeData(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_edge_data_maker && custom_node_label_maker)
    _DataExtracter_ed_nl(Args&&... args):
      Parent(std::forward<Args>(args)...),
      ExtractEdgeData()
    {}

    template<class First, class... Args> requires (!NodeFunctionType<First> && custom_node_label_maker && custom_edge_data_maker)
    _DataExtracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      ExtractEdgeData(std::forward<First>(first))
    {}

    // NOTE: we wont try to initialize our ExtractEdgeData with a NodeFunctionType
    //       instead, we pass that to the ExtractNodeLabel function and use the rest to initialize our ExtractEdgeData
    template<NodeFunctionType First, class... Args>
    _DataExtracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<First>(first)),
      ExtractEdgeData(std::forward<Args>(args)...)
    {}

    template<class First, NodeFunctionType Second>
    _DataExtracter_ed_nl(First&& first, Second&& second):
      Parent(std::forward<Second>(second)),
      ExtractEdgeData(std::forward<First>(first))
    {}

    template<class... Args>
    EdgeDataReturnType operator()(const Ex_edge_data, Args&&... args) { return ExtractEdgeData::operator()(std::forward<Args>(args)...); }
    template<class... Args>
    EdgeDataReturnType operator()(const Ex_edge_data, Args&&... args) const { return ExtractEdgeData::operator()(std::forward<Args>(args)...); }
    template<class... Args>
    EdgeDataReturnType get_node_label()(Args&&... args) { return ExtractEdgeData::operator()(std::forward<Args>(args)...); }
    template<class... Args>
    EdgeDataReturnType get_node_label()(Args&&... args) const { return ExtractEdgeData::operator()(std::forward<Args>(args)...); }

  };

  template<OptionalPhylogenyType Network,
    OptionalNodeFunctionType ExtractNodeLabel>
  struct _DataExtracter_ed_nl<Network, void, ExtractNodeLabel>: public _DataExtracter_nl<Network, ExtractNodeLabel> {
    using Parent = _DataExtracter_nl<Network, ExtractNodeLabel>;
    using ExtractEdgeData = void;
    using Parent::Parent;
 
    static constexpr bool custom_edge_data_maker  = false;
    static constexpr bool ignoring_edge_data = true;
  };





  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeData = void,
           class ExtractEdgeData                     = void,
           OptionalNodeFunctionType ExtractNodeLabel = void>
  struct _DataExtracter: public _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>, public _ExtractNodeData {
    using Parent = _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>;
    using ExtractNodeData = _ExtractNodeData;
    using NodeDataReturnType = decltype(std::declval<ExtractNodeData>().operator()(NoNode));
    using ConstNodeDataReturnType = decltype(std::declval<const ExtractNodeData>().operator()(NoNode));

    using Parent::custom_node_label_maker;
    using Parent::custom_edge_data_maker;
    static constexpr bool custom_node_data_maker = !std::is_same_v<ExtractNodeData, DefaultExtractData<Ex_node_data, Network>>;
    static constexpr bool ignoring_node_data = false;

    NodeDataReturnType operator()(const Ex_node_data, const NodeDesc u) { return ExtractNodeData::operator()(u); }
    NodeDataReturnType operator()(const Ex_node_data, const NodeDesc u) const { return ExtractNodeData::operator()(u); }
    NodeDataReturnType get_node_data()(const NodeDesc u) { return ExtractNodeData::operator()(u); }
    NodeDataReturnType get_node_data()(const NodeDesc u) const { return ExtractNodeData::operator()(u); }

    _DataExtracter() = default;
    _DataExtracter(const _DataExtracter&) = default;
    _DataExtracter(_DataExtracter&&) = default;

    template<class... Args> requires (custom_node_data_maker && !custom_node_label_maker && !custom_edge_data_maker)
    _DataExtracter(Args&&... args):
      Parent(),
      ExtractNodeData(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_node_data_maker)
    _DataExtracter(Args&&... args):
      Parent(std::forward<Args>(args)...),
      ExtractNodeData()
    {}

    template<class First, class... Args> requires (custom_node_data_maker && (custom_edge_data_maker || custom_node_label_maker))
    _DataExtracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      ExtractNodeData(std::forward<First>(first))
    {}

    template<NodeFunctionType First, class... Args> requires (sizeof...(Args) >= 2)
    _DataExtracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      ExtractNodeData(std::forward<First>(first))
    {}
  };

  template<OptionalPhylogenyType Network,
           class ExtractEdgeData,
           OptionalNodeFunctionType ExtractNodeLabel>
  struct _DataExtracter<Network, void, ExtractEdgeData, ExtractNodeLabel>: public _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>;
    using ExtractNodeData = void;
    using Parent::Parent;
    using NodeDataReturnType = bool;
    using ConstNodeDataReturnType = bool;

    static constexpr bool custom_node_data_maker = false;
    static constexpr bool ignoring_node_data = true;
  };


  // accumulate the defined classes into one and provide a nice interface
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType ExtractNodeData = DefaultExtractData<Ex_node_data, Network>,
           class ExtractEdgeData                     = DefaultExtractData<Ex_edge_data, Network>,
           OptionalNodeFunctionType ExtractNodeLabel = DefaultExtractData<Ex_node_label, Network>>
  struct DataExtracter: public _DataExtracter<Network, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = _DataExtracter<Network, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>;
    using Parent::Parent;

    typename Parent::NodeDataReturnType operator()(const Ex_node_data, const NodeDesc u) { return ExtractNodeData::operator()(u); }
    typename Parent::ConstNodeDataReturnType operator()(const Ex_node_data, const NodeDesc u) const { return ExtractNodeData::operator()(u); }
    typename Parent::NodeLabelReturnType operator()(const Ex_node_label, const NodeDesc u) { return ExtractNodeLabel::operator()(u); }
    typename Parent::ConstNodeLabelReturnType operator()(const Ex_node_label, const NodeDesc u) const { return ExtractNodeLabel::operator()(u); }
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
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeData,
           class ExtractEdgeData,
           NodeFunctionType ExtractNodeLabel>
  auto make_data_extracter(ExtractNodeData&& get_node_data, ExtractEdgeData&& get_edge_data, ExtractNodeLabel&& get_node_label) {
    using Extracter = DataExtracter<SourcePhylo, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>;
    return Extracter(std::forward<ExtractNodeData>(get_node_data),
                     std::forward<ExtractEdgeData>(get_edge_data),
                     std::forward<ExtractNodeLabel>(get_node_label));
  }

  // if 2 NodeFunctionTypes are provided, the first is interpreted as DataExtract and the second as LabelExtract
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeData,
           NodeFunctionType ExtractNodeLabel>
  auto make_data_extracter(ExtractNodeData&& get_node_data, ExtractNodeLabel&& get_node_label) {
    using Extracter = DataExtracter<SourcePhylo,
                          ExtractNodeData,
                          DefaultExtractData<Ex_edge_data, SourcePhylo, TargetPhylo>,
                          ExtractNodeLabel>;
    return Extracter(std::forward<ExtractNodeData>(get_node_data), std::forward<ExtractNodeLabel>(get_node_label));
  }

  // if only 1 NodeFunctionType is given, the user can specify how to interpret it by passing either the Ex_node_label or the Ex_node_data tag
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething>
  auto make_data_extracter(Ex_node_label, ExtractNodeSomething&& nds) {
    using Extracter = DataExtracter<SourcePhylo,
                          DefaultExtractData<Ex_node_data, SourcePhylo, TargetPhylo>,
                          DefaultExtractData<Ex_edge_data, SourcePhylo, TargetPhylo>,
                          ExtractNodeSomething>;
    return Extracter(std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething>
  auto make_data_extracter(Ex_node_data, ExtractNodeSomething&& nds) {
    using Extracter = DataExtracter<SourcePhylo,
                          ExtractNodeSomething,
                          DefaultExtractData<Ex_edge_data, SourcePhylo, TargetPhylo>,
                          DefaultExtractData<Ex_node_label, SourcePhylo, TargetPhylo>>;
    return Extracter(std::forward<ExtractNodeSomething>(nds));
  }


  // if 1 NodeFunctionType and 1 Non-NodeFunctionType are given, the user may choose how to interpret the NodeFunctionType (node-data or -label)
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(Ex_node_label, ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using Extracter = DataExtracter<SourcePhylo,
                                    DefaultExtractData<Ex_node_data, SourcePhylo, TargetPhylo>,
                                    ExtractEdgeData,
                                    ExtractNodeSomething>;
    return Extracter(std::forward<ExtractEdgeData>(get_edge_data), std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(Ex_node_data, ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using Extracter = DataExtracter<SourcePhylo,
                          ExtractNodeSomething,
                          ExtractEdgeData,
                          DefaultExtractData<Ex_node_label, SourcePhylo, TargetPhylo>>;
    return Extracter(std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
  }

  // by default, a single NodeFunctionType is interpreted as node-data-extract, unless SourcePhylo is non-void and has no node data
  template<OptionalPhylogenyType SourcePhylo>
  struct _choose_node_function { using type = Ex_node_data; };
  template<OptionalPhylogenyType SourcePhylo> requires (!std::is_void_v<SourcePhylo> && !HasNodeData<SourcePhylo>)
  struct _choose_node_function<SourcePhylo> { using type = Ex_node_label; };
  template<OptionalPhylogenyType SourcePhylo>
  using choose_node_function = typename _choose_node_function<SourcePhylo>::type;

  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething>
  auto make_data_extracter(ExtractNodeSomething&& nds) {
    using tag = choose_node_function<SourcePhylo>;
    return make_data_extracter<SourcePhylo, TargetPhylo>(tag{}, std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> 
             requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using tag = choose_node_function<SourcePhylo>;
    return make_data_extracter<SourcePhylo, TargetPhylo>(tag{}, std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
  }


  // if only 1 argument is given and it's not a NodeFunctionType, then interpret it as edge-data-extraction
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           class ExtractEdgeData>
             requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(ExtractEdgeData&& get_edge_data) {
    using Extracter = DataExtracter<SourcePhylo,
                         DefaultExtractData<Ex_node_data, SourcePhylo, TargetPhylo>,
                         ExtractEdgeData,
                         DefaultExtractData<Ex_node_label, SourcePhylo, TargetPhylo>>;
    return Extracter(std::forward<ExtractEdgeData>(get_edge_data));
  }

  template<OptionalPhylogenyType SourcePhylo, OptionalPhylogenyType TargetPhylo = SourcePhylo>
  auto make_data_extracter() {
    using Extracter = DataExtracter<SourcePhylo,
                         DefaultExtractData<Ex_node_data, SourcePhylo, TargetPhylo>,
                         DefaultExtractData<Ex_edge_data, SourcePhylo, TargetPhylo>,
                         DefaultExtractData<Ex_node_label, SourcePhylo, TargetPhylo>>;
    return Extracter();
  }
  //! In order to allow passing a pre-made data extracter to the make_emplcer helper functions, we allow passing one here
  template<DataExtracterType PremadeExtracter>
  auto make_data_extracter(PremadeExtracter&& extracter) {
    return extracter;
  }


}
