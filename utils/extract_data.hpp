
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

  // ----------------- using tags -----------------------
  template<DataTag Tag, OptionalPhylogenyType Phylo>
  using DataTypeOf = std::conditional_t<is_node_label_tag<Tag>,
                            NodeLabelOf<Phylo>,
                            std::conditional_t<is_node_data_tag<Tag>,
                                    NodeDataOf<Phylo>,
                                    EdgeDataOf<Phylo>>>;
  template<DataTag Tag, OptionalPhylogenyType Phylo> constexpr bool HasDataType = !std::is_void_v<DataTypeOf<Tag, Phylo>>;



#warning "TODO: can we turn those into templates of constexpr lambdas?"
  template<DataTag Tag, PhylogenyType Phylo>
  struct ProtoDefaultExtractData {};

  // NOTE: when passing an rvalue-reference as Phylo, we will call the &&-qualified version of label() and data()
  template<StrictPhylogenyType Phylo> requires (Phylo::has_node_labels)
  struct ProtoDefaultExtractData<Ex_node_label, Phylo> { decltype(auto) operator()(const NodeDesc u) const { return node_of<Phylo>(u).label(); } };

  template<StrictPhylogenyType Phylo> requires (Phylo::has_node_data)
  struct ProtoDefaultExtractData<Ex_node_data, Phylo> { decltype(auto) operator()(const NodeDesc u) const { return node_of<Phylo>(u).data(); } };

  template<StrictPhylogenyType Phylo> requires (Phylo::has_edge_data)
  struct ProtoDefaultExtractData<Ex_edge_data, Phylo> {
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
  struct _DefaultExtractData { using type = ProtoDefaultExtractData<Tag, Phylo>; };
  template<DataTag Tag>
  struct _DefaultExtractData<Tag, void> { using type = void; };
  template<StrictPhylogenyType Phylo> requires (not Phylo::has_node_data)
  struct _DefaultExtractData<Ex_node_data, Phylo> { using type = void; };
  template<StrictPhylogenyType Phylo> requires (not Phylo::has_edge_data)
  struct _DefaultExtractData<Ex_edge_data, Phylo> { using type = void; };
  template<StrictPhylogenyType Phylo> requires (not Phylo::has_node_labels)
  struct _DefaultExtractData<Ex_node_label, Phylo> { using type = void; };

  template<DataTag Tag, OptionalPhylogenyType Phylo = void>
  using DefaultExtractData = typename _DefaultExtractData<Tag, std::remove_reference_t<Phylo>>::type;
 
  
  // ============== Start of main class hierarchy for data extraction =================
  
  // ============== Part 1: extract node labels =================

/*
 * this is a container class storing functions to extract node-data, edge-data, and node-labels
 * depending on the number and type of constructor parameters and depending on a Network type, the 3 functions are initialized smartly.
 */
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeLabel  = void>
  struct _DataExtracter_nl {
    using ExtractNodeLabel = _ExtractNodeLabel;

    static constexpr bool custom_node_label_maker = !std::is_same_v<ExtractNodeLabel, DefaultExtractData<Ex_node_label, Network>>;
    static constexpr bool ignoring_node_labels = false;
    ExtractNodeLabel get_node_label;

    decltype(auto) operator()(const Ex_node_label, const NodeDesc u) { return get_node_label(u); }
    decltype(auto) operator()(const Ex_node_label, const NodeDesc u) const { return get_node_label(u); }
  };
  template<OptionalPhylogenyType Network>
  struct _DataExtracter_nl<Network, void> {
    using ExtractNodeLabel = void;

    bool operator()(const Ex_node_label, const NodeDesc) = delete;
    bool operator()(const Ex_node_label, const NodeDesc) const = delete;

    static constexpr bool custom_node_label_maker = false;
    static constexpr bool ignoring_node_labels = true;
  };


  // ============== Part 2: extract edge data =================

  template<OptionalPhylogenyType Network,
    class _ExtractEdgeData                    = void,
    OptionalNodeFunctionType ExtractNodeLabel = void>
  struct _DataExtracter_ed_nl: public _DataExtracter_nl<Network, ExtractNodeLabel> {
    using Parent = _DataExtracter_nl<Network, ExtractNodeLabel>;
    using ExtractEdgeData = _ExtractEdgeData;
    using Parent::custom_node_label_maker;
    using Parent::operator();

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


    template<AdjacencyType Adj> // NOTE: if get_edge_data is not invocable with an adjacency, we'll make an edge from the adjacency
    decltype(auto) operator()(const Ex_edge_data, Adj&& v) {
      if constexpr (std::is_invocable_v<ExtractEdgeData, Adj&&>){
        return get_edge_data(std::forward<Adj>(v));
      } else return get_edge_data(Network::Edge(NoNode, std::forward<Adj>(v)));
    }

    template<AdjacencyType Adj>
    decltype(auto) operator()(const Ex_edge_data, Adj&& v) const {
      if constexpr (std::is_invocable_v<ExtractEdgeData, Adj&&>){
        return get_edge_data(std::forward<Adj>(v));
      } else return get_edge_data(Network::Edge(NoNode, std::forward<Adj>(v)));
    }

    template<EdgeType Edge> // NOTE: if get_edge_data is not invocable with an Edge, we'll extract the edge's adjacency
    decltype(auto) operator()(const Ex_edge_data, Edge&& uv) {
      if constexpr (std::is_invocable_v<ExtractEdgeData, Edge&&>){
        return get_edge_data(std::forward<Edge>(uv));
      } else return get_edge_data(uv.tail(), std::forward<Edge>(uv).head());
    }
    template<EdgeType Edge>
    decltype(auto) operator()(const Ex_edge_data, Edge&& uv) const {
      if constexpr (std::is_invocable_v<ExtractEdgeData, Edge&&>){
        return get_edge_data(std::forward<Edge>(uv));
      } else return get_edge_data(uv.tail(), std::forward<Edge>(uv).head());
    }
  };

  template<OptionalPhylogenyType Network,
    OptionalNodeFunctionType ExtractNodeLabel>
  struct _DataExtracter_ed_nl<Network, void, ExtractNodeLabel>: public _DataExtracter_nl<Network, ExtractNodeLabel> {
    using Parent = _DataExtracter_nl<Network, ExtractNodeLabel>;
    using Parent::operator();
    using ExtractEdgeData = void;
 
    _DataExtracter_ed_nl() = default;
    INHERIT_ALL_CONSTRUCTORS(_DataExtracter_ed_nl, Parent)

    template<class... Args> bool operator()(const Ex_edge_data, Args&&...) const = delete;
    template<class... Args> bool operator()(const Ex_edge_data, Args&&...) = delete;

    static constexpr bool custom_edge_data_maker  = false;
    static constexpr bool ignoring_edge_data = true;
  };


  // ============== Part 3: extract node data =================

  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType _ExtractNodeData = void,
           class ExtractEdgeData                     = void,
           OptionalNodeFunctionType ExtractNodeLabel = void>
  struct _DataExtracter: public _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = _DataExtracter_ed_nl<Network, ExtractEdgeData, ExtractNodeLabel>;
    using Parent::operator();
    using ExtractNodeData = _ExtractNodeData;

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
    using Parent::operator();
    using ExtractNodeData = void;
    
    _DataExtracter() = default;
    INHERIT_ALL_CONSTRUCTORS(_DataExtracter, Parent)

    bool operator()(const Ex_node_data, const NodeDesc) = delete;
    bool operator()(const Ex_node_data, const NodeDesc) const = delete;

    static constexpr bool custom_node_data_maker = false;
    static constexpr bool ignoring_node_data = true;
  };


  // ============== Putting it all together: DataExtracter  =================
  // accumulate the defined classes into one and provide a nice interface
  template<OptionalPhylogenyType Network,
           OptionalNodeFunctionType ExtractNodeData = DefaultExtractData<Ex_node_data, Network>,
           class ExtractEdgeData                     = DefaultExtractData<Ex_edge_data, Network>,
           OptionalNodeFunctionType ExtractNodeLabel = DefaultExtractData<Ex_node_label, Network>>
  using DataExtracter = _DataExtracter<Network, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>;


  // -------- DataExtracter: deduction guides --------------
  
  // -------- DataExtracter: concepts --------------
  template<class T>
  concept StrictDataExtracterType = requires {
    { T::ignoring_node_labels } -> std::convertible_to<const bool>;
    { T::ignoring_edge_data } -> std::convertible_to<const bool>;
    { T::ignoring_node_data } -> std::convertible_to<const bool>;
  };
  template<class T> concept DataExtracterType = StrictDataExtracterType<std::remove_reference_t<T>>;


  // -------- DataExtracter: defaults --------------

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
           NodeFunctionType ExtractNodeData,
           NodeFunctionType ExtractNodeLabel>
  auto make_data_extracter(ExtractNodeData&& get_node_data, ExtractNodeLabel&& get_node_label) {
    using Extracter = DataExtracter<SourcePhylo,
                          ExtractNodeData,
                          DefaultExtractData<Ex_edge_data, SourcePhylo>,
                          ExtractNodeLabel>;
    return Extracter(std::forward<ExtractNodeData>(get_node_data), std::forward<ExtractNodeLabel>(get_node_label));
  }

  // if only 1 NodeFunctionType is given, the user can specify how to interpret it by passing either the Ex_node_label or the Ex_node_data tag
  template<OptionalPhylogenyType SourcePhylo,
           NodeFunctionType ExtractNodeSomething>
  auto make_data_extracter(Ex_node_label, ExtractNodeSomething&& nds) {
    using Extracter = DataExtracter<SourcePhylo,
                          DefaultExtractData<Ex_node_data, SourcePhylo>,
                          DefaultExtractData<Ex_edge_data, SourcePhylo>,
                          ExtractNodeSomething>;
    return Extracter(std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo, NodeFunctionType ExtractNodeSomething>
  auto make_data_extracter(Ex_node_data, ExtractNodeSomething&& nds) {
    using Extracter = DataExtracter<SourcePhylo,
                          ExtractNodeSomething,
                          DefaultExtractData<Ex_edge_data, SourcePhylo>,
                          DefaultExtractData<Ex_node_label, SourcePhylo>>;
    return Extracter(std::forward<ExtractNodeSomething>(nds));
  }


  // if 1 NodeFunctionType and 1 Non-NodeFunctionType are given, the user may choose how to interpret the NodeFunctionType (node-data or -label)
  template<OptionalPhylogenyType SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(Ex_node_label, ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using Extracter = DataExtracter<SourcePhylo,
                                    DefaultExtractData<Ex_node_data, SourcePhylo>,
                                    ExtractEdgeData,
                                    ExtractNodeSomething>;
    return Extracter(std::forward<ExtractEdgeData>(get_edge_data), std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(Ex_node_data, ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using Extracter = DataExtracter<SourcePhylo,
                          ExtractNodeSomething,
                          ExtractEdgeData,
                          DefaultExtractData<Ex_node_label, SourcePhylo>>;
    return Extracter(std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
  }

  // by default, a single NodeFunctionType is interpreted as node-data-extract, unless Phylo is non-void and has no node data
  template<OptionalPhylogenyType Phylo>
  struct _choose_node_function { using type = Ex_node_data; };
  template<OptionalPhylogenyType Phylo> requires (!std::is_void_v<Phylo> && !HasNodeData<Phylo>)
  struct _choose_node_function<Phylo> { using type = Ex_node_label; };
  template<OptionalPhylogenyType Phylo>
  using choose_node_function = typename _choose_node_function<Phylo>::type;

  template<OptionalPhylogenyType SourcePhylo, NodeFunctionType ExtractNodeSomething>
    requires ((not DataExtracterType<ExtractNodeSomething>) && (not DataExtracterType<ExtractNodeSomething>))
  auto make_data_extracter(ExtractNodeSomething&& nds) {
    using tag = choose_node_function<SourcePhylo>;
    return make_data_extracter<SourcePhylo>(tag{}, std::forward<ExtractNodeSomething>(nds));
  }
  template<OptionalPhylogenyType SourcePhylo,
           NodeFunctionType ExtractNodeSomething,
           class ExtractEdgeData> 
             requires (!NodeFunctionType<ExtractEdgeData>)
  auto make_data_extracter(ExtractNodeSomething&& nds, ExtractEdgeData&& get_edge_data) {
    using tag = choose_node_function<SourcePhylo>;
    return make_data_extracter<SourcePhylo>(tag{}, std::forward<ExtractNodeSomething>(nds), std::forward<ExtractEdgeData>(get_edge_data));
  }


  // if only 1 argument is given and it's not a NodeFunctionType, then interpret it as edge-data-extraction
  template<OptionalPhylogenyType SourcePhylo, class ExtractEdgeData>
      requires (not NodeFunctionType<ExtractEdgeData> && (not DataExtracterType<ExtractEdgeData>))
  auto make_data_extracter(ExtractEdgeData&& get_edge_data) {
    using Extracter = DataExtracter<SourcePhylo,
                         DefaultExtractData<Ex_node_data, SourcePhylo>,
                         ExtractEdgeData,
                         DefaultExtractData<Ex_node_label, SourcePhylo>>;
    return Extracter(std::forward<ExtractEdgeData>(get_edge_data));
  }

  template<OptionalPhylogenyType SourcePhylo>
  auto make_data_extracter() {
    using Extracter = DataExtracter<SourcePhylo,
                         DefaultExtractData<Ex_node_data, SourcePhylo>,
                         DefaultExtractData<Ex_edge_data, SourcePhylo>,
                         DefaultExtractData<Ex_node_label, SourcePhylo>>;
    return Extracter();
  }
  //! In order to allow passing a pre-made data extracter to the make_emplacer helper functions, we allow passing one here
  template<OptionalPhylogenyType SourcePhylo, DataExtracterType PremadeExtracter>
  auto make_data_extracter(PremadeExtracter&& extracter) {
    return extracter;
  }

  template<OptionalPhylogenyType SourcePhylo>
  using DefaultDataExtracter = DataExtracter<SourcePhylo,
                         DefaultExtractData<Ex_node_data, SourcePhylo>,
                         DefaultExtractData<Ex_edge_data, SourcePhylo>,
                         DefaultExtractData<Ex_node_label, SourcePhylo>>;


}
