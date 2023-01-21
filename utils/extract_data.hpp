
#pragma once

#include "types.hpp"
#include "node.hpp"
#include "tags.hpp"

namespace PT {

  template<OptionalPhylogenyType Phylo> struct _NodeLabelOf { using type = mstd::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::LabelType>; };
  template<OptionalPhylogenyType Phylo> struct _NodeDataOf { using type = mstd::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::NodeData>; };
  template<OptionalPhylogenyType Phylo> struct _EdgeDataOf { using type = mstd::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::EdgeData>; };
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
  using ReturnableDataTypeOf = mstd::ReturnableType<DataTypeOf<Tag, Phylo>>;
 
  template<DataTag Tag, OptionalPhylogenyType Phylo> constexpr bool HasDataType = !std::is_void_v<DataTypeOf<Tag, Phylo>>;


  template<DataTag Tag, OptionalPhylogenyType Phylo>
  struct _DefaultExtractData {};

#warning "TODO: can we turn those into templates of constexpr lambdas?"
  // NOTE: when passing an rvalue-reference as Phylo, we will call the &&-qualified version of label() and data()
  template<PhylogenyType Phylo>
  struct _DefaultExtractData<Ex_node_label, Phylo> { decltype(auto) operator()(const NodeDesc u) const { return node_of<Phylo>(u).label(); } };

  template<PhylogenyType Phylo>
  struct _DefaultExtractData<Ex_node_data, Phylo> { decltype(auto) operator()(const NodeDesc u) const { return node_of<Phylo>(u).data(); } };

  template<PhylogenyType Phylo>
  struct _DefaultExtractData<Ex_edge_data, Phylo> {
    // NOTE: we have to be able to tell EdgeDataExtractors from NodeDataExtractors
    template<LooseEdgeType Edge>
    auto& operator()(Edge&& uv) const { return uv.data(); }

    template<AdjacencyType Adj> requires (!std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(const NodeDesc u, Adj&& v) const { return v.data(); }

    template<AdjacencyType Adj> requires (!std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(Adj&& u, const NodeDesc v) const { return u.data(); }

    template<AdjacencyType Adj> requires (!std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(Adj&& v) const { return v.data(); }

    auto& operator()(const NodeDesc u, const NodeDesc v) const {
      auto& u_children = node_of<Phylo>(u).children();
      const auto iter = mstd::find(u_children, v);
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


  

/*
 * this is a container class storing functions to extract node-data, edge-data, and node-labels
 * depending on the number and type of constructor parameters and depending on a Network type, the 3 functions are initialized smartly.
 */
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeLabel  = void>
  struct _DataExtracter_nl: public _ExtractNodeLabel {
    using ExtractNodeLabel = _ExtractNodeLabel;
    using ExtractNodeLabel::ExtractNodeLabel;

    static constexpr bool custom_node_label_maker = !std::is_same_v<ExtractNodeLabel, DefaultExtractData<Ex_node_label, Network>>;
    static constexpr bool ignoring_node_labels = false;
    ExtractNodeLabel get_node_label;

    decltype(auto) operator()(const Ex_node_label, const NodeDesc u) { return get_node_label(u); }
    decltype(auto) operator()(const Ex_node_label, const NodeDesc u) const { return get_node_label(u); }
  };
  template<OptionalPhylogenyType Network>
  struct _DataExtracter_nl<Network, void> {
    using ExtractNodeLabel = void;
    using NodeLabelReturnType = bool;
    using ConstNodeLabelReturnType = bool;

    // NOTE: unfortunately, operator() is not inherited if it is overwritten, so we'll "use Parent::operator()" to inherit them
    //       however, we then have to define an operator() even if we're not extracting anything :(
    bool operator()(const Ex_node_label, const NodeDesc) const { return false; }

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
    using Parent::operator();
 
    template<class... Args>
    using EdgeDataReturnType = decltype(std::declval<ExtractEdgeData>().operator()(std::declval<Args>()...));

    static constexpr bool custom_edge_data_maker  = !std::is_same_v<ExtractEdgeData,  DefaultExtractData<Ex_edge_data, Network>>;
    static constexpr bool ignoring_edge_data = false;
    ExtractEdgeData get_edge_data;
    
    _DataExtracter_ed_nl() = default;

    // if the node label maker is not custom, then the edge data maker gets everything
    template<class... Args> requires (!custom_node_label_maker)
    _DataExtracter_ed_nl(Args&&... args):
      Parent(),
      get_edge_data(std::forward<Args>(args)...)
    {}

    // if the edge data maker is not custom, then the node label maker gets everything
    template<class... Args> requires (!custom_edge_data_maker && custom_node_label_maker)
    _DataExtracter_ed_nl(Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_edge_data()
    {}

    // NOTE: we wont try to initialize our ExtractEdgeData with a NodeFunctionType
    //       instead, we pass that to the ExtractNodeLabel function and use the rest to initialize our ExtractEdgeData
    template<NodeFunctionType First, class... Args> requires (custom_edge_data_maker && custom_node_label_maker)
    _DataExtracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<First>(first)),
      get_edge_data(std::forward<Args>(args)...)
    {}
    template<class First, class... Args> requires (!NodeFunctionType<First> && custom_edge_data_maker && custom_node_label_maker)
    _DataExtracter_ed_nl(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_edge_data(std::forward<First>(first))
    {}


    template<EdgeType Edge>
    decltype(auto) operator()(const Ex_edge_data, Edge&& uv) {
      if constexpr (!std::invocable<ExtractEdgeData, Edge&&>){
        return get_edge_data(uv.tail(), std::forward<Edge>(uv).head());
      } else return get_edge_data(std::forward<Edge>(uv));
    }
    template<EdgeType Edge>
    decltype(auto) operator()(const Ex_edge_data, Edge&& uv) const {
      if constexpr (!std::invocable<ExtractEdgeData, Edge&&>){
        return get_edge_data(uv.tail(), std::forward<Edge>(uv).head());
      } else return get_edge_data(std::forward<Edge>(uv));
    }
  };

  template<OptionalPhylogenyType Network,
    OptionalNodeFunctionType ExtractNodeLabel>
  struct _DataExtracter_ed_nl<Network, void, ExtractNodeLabel>: public _DataExtracter_nl<Network, ExtractNodeLabel> {
    using Parent = _DataExtracter_nl<Network, ExtractNodeLabel>;
    using ExtractEdgeData = void;
    using Parent::operator();
 
    template<class... Args> _DataExtracter_ed_nl(Args&&... args): Parent(std::forward<Args>(args)...) {}

    // NOTE: unfortunately, operator() is not inherited if it is overwritten, so we'll "use Parent::operator()" to inherit them
    //       however, we then have to define an operator() even if we're not extracting anything :(
    template<class... Args>
    bool operator()(const Ex_edge_data, Args&&...) const { return false; }

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
    using Parent::operator();

    using Parent::custom_node_label_maker;
    using Parent::custom_edge_data_maker;
    static constexpr bool custom_node_data_maker = !std::is_same_v<ExtractNodeData, DefaultExtractData<Ex_node_data, Network>>;
    static constexpr bool ignoring_node_data = false;
    ExtractNodeData get_node_data;

    decltype(auto) operator()(const Ex_node_data, const NodeDesc u) { return get_node_data(u); }
    decltype(auto) operator()(const Ex_node_data, const NodeDesc u) const { return get_node_data(u); }

    _DataExtracter() = default;
    //_DataExtracter(const _DataExtracter&) = default;
    //_DataExtracter(_DataExtracter&&) = default;

    template<class... Args> requires (custom_node_data_maker && !custom_node_label_maker && !custom_edge_data_maker)
    _DataExtracter(Args&&... args):
      Parent(),
      get_node_data(std::forward<Args>(args)...)
    {}

    template<class... Args> requires (!custom_node_data_maker)
    _DataExtracter(Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data()
    {}

    template<class First, class... Args> requires (custom_node_data_maker && (custom_edge_data_maker || custom_node_label_maker))
    _DataExtracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data(std::forward<First>(first))
    {}

    template<NodeFunctionType First, class... Args> requires (sizeof...(Args) >= 2)
    _DataExtracter(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data(std::forward<First>(first))
    {}
  };

  template<OptionalPhylogenyType Network,
           class ExtractEdgeData,
           OptionalNodeFunctionType ExtractNodeLabel>
  struct _DataExtracter<Network, void, ExtractEdgeData, ExtractNodeLabel>: public _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>;
    using ExtractNodeData = void;
    using NodeDataReturnType = bool;
    using ConstNodeDataReturnType = bool;
    using Parent::operator();

    // NOTE: unfortunately, operator() is not inherited if it is overwritten, so we'll "use Parent::operator()" to inherit them
    //       however, we then have to define an operator() even if we're not extracting anything :(
    bool operator()(const Ex_node_data, const NodeDesc) const { return false; }

    template<class... Args> _DataExtracter(Args&&... args): Parent(std::forward<Args>(args)...) {}

    static constexpr bool custom_node_data_maker = false;
    static constexpr bool ignoring_node_data = true;
  };


  // accumulate the defined classes into one and provide a nice interface
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType ExtractNodeData = DefaultExtractData<Ex_node_data, Network>,
           class ExtractEdgeData                     = DefaultExtractData<Ex_edge_data, Network>,
           OptionalNodeFunctionType ExtractNodeLabel = DefaultExtractData<Ex_node_label, Network>>
  using DataExtracter = _DataExtracter<Network, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>;

  template<class T>
  concept StrictDataExtracterType = requires {
    { T::ignoring_node_labels } -> std::convertible_to<const bool>;
    { T::ignoring_edge_data } -> std::convertible_to<const bool>;
    { T::ignoring_node_data } -> std::convertible_to<const bool>;
  };
  template<class T> concept DataExtracterType = StrictDataExtracterType<std::remove_reference_t<T>>;



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

  // by default, a single NodeFunctionType is interpreted as node-data-extract, unless Phylo is non-void and has no node data
  template<OptionalPhylogenyType Phylo>
  struct _choose_node_function { using type = Ex_node_data; };
  template<OptionalPhylogenyType Phylo> requires (!std::is_void_v<Phylo> && !HasNodeData<Phylo>)
  struct _choose_node_function<Phylo> { using type = Ex_node_label; };
  template<OptionalPhylogenyType Phylo>
  using choose_node_function = typename _choose_node_function<Phylo>::type;

  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething> requires (!DataExtracterType<ExtractNodeSomething>)
  auto make_data_extracter(ExtractNodeSomething&& nds) {
    using tag = choose_node_function<mstd::VoidOr<TargetPhylo, SourcePhylo>>;
    return make_data_extracter<SourcePhylo, TargetPhylo>(tag{}, std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> 
             requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using tag = choose_node_function<mstd::VoidOr<TargetPhylo, SourcePhylo>>;
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
  //! In order to allow passing a pre-made data extracter to the make_emplacer helper functions, we allow passing one here
  template<OptionalPhylogenyType SourcePhylo,
           OptionalPhylogenyType TargetPhylo = SourcePhylo,
           DataExtracterType PremadeExtracter>
  auto make_data_extracter(PremadeExtracter&& extracter) {
    return extracter;
  }


}
