
#pragma once

#include "types.hpp"
#include "node.hpp"
#include "tags.hpp"

namespace PT {

  template<OptionalPhylogenyType Phylo> struct NodeLabelOf_ { using type = mstd::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::LabelType>; };
  template<OptionalPhylogenyType Phylo> struct NodeDataOf_ { using type = mstd::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::NodeData>; };
  template<OptionalPhylogenyType Phylo> struct EdgeDataOf_ { using type = mstd::copy_cvref_t<Phylo, typename std::remove_reference_t<Phylo>::EdgeData>; };
  template<> struct NodeLabelOf_<void> { using type = void; };
  template<> struct NodeDataOf_<void> { using type = void; };
  template<> struct EdgeDataOf_<void> { using type = void; };

  template<OptionalPhylogenyType Phylo> using NodeLabelOf = typename NodeLabelOf_<Phylo>::type;
  template<OptionalPhylogenyType Phylo> using NodeDataOf = typename NodeDataOf_<Phylo>::type;
  template<OptionalPhylogenyType Phylo> using EdgeDataOf = typename EdgeDataOf_<Phylo>::type;

  template<OptionalPhylogenyType Phylo> constexpr bool HasNodeLabel = not std::is_void_v<NodeLabelOf<Phylo>>;
  template<OptionalPhylogenyType Phylo> constexpr bool HasNodeData  = not std::is_void_v<NodeDataOf<Phylo>>;
  template<OptionalPhylogenyType Phylo> constexpr bool HasEdgeData  = not std::is_void_v<EdgeDataOf<Phylo>>;

  // ----------------- using tags -----------------------
  template<DataTag Tag, OptionalPhylogenyType Phylo>
  using DataTypeOf = std::conditional_t<is_node_label_tag<Tag>,
                            NodeLabelOf<Phylo>,
                            std::conditional_t<is_node_data_tag<Tag>,
                                    NodeDataOf<Phylo>,
                                    EdgeDataOf<Phylo>>>;
  template<DataTag Tag, OptionalPhylogenyType Phylo> constexpr bool HasDataType = not std::is_void_v<DataTypeOf<Tag, Phylo>>;



#warning "TODO: can we turn those into templates of constexpr lambdas?"
  template<DataTag Tag, StrictPhylogenyType Phylo>
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

    template<AdjacencyType Adj> requires (not std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(const NodeDesc u, Adj&& v) const { return v.data(); }

    template<AdjacencyType Adj> requires (not std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(Adj&& u, const NodeDesc v) const { return u.data(); }

    template<AdjacencyType Adj> requires (not std::is_same_v<std::remove_cvref_t<Adj>, NodeDesc>)
    auto& operator()(Adj&& v) const { return v.data(); }

    auto& operator()(const NodeDesc u, const NodeDesc v) const {
      auto& u_children = node_of<Phylo>(u).children();
      const auto iter = mstd::find(u_children, v);
      if(iter == u_children.end()) throw std::logic_error("trying to get data from a non-edge");
      return iter->data();
    }
  };

  template<DataTag Tag, OptionalStrictPhylogenyType Phylo>
  struct DefaultExtractData_ { using type = ProtoDefaultExtractData<Tag, Phylo>; };
  template<DataTag Tag>
  struct DefaultExtractData_<Tag, void> { using type = void; };
  template<StrictPhylogenyType Phylo> requires (not Phylo::has_node_data)
  struct DefaultExtractData_<Ex_node_data, Phylo> { using type = void; };
  template<StrictPhylogenyType Phylo> requires (not Phylo::has_edge_data)
  struct DefaultExtractData_<Ex_edge_data, Phylo> { using type = void; };
  template<StrictPhylogenyType Phylo> requires (not Phylo::has_node_labels)
  struct DefaultExtractData_<Ex_node_label, Phylo> { using type = void; };

  template<DataTag Tag, OptionalPhylogenyType Phylo = void>
  using DefaultExtractData = typename DefaultExtractData_<Tag, std::remove_reference_t<Phylo>>::type;
 
  
  // ============== Start of main class hierarchy for data extraction =================
  
  // ============== Part 1: extract node labels =================

  // this is a container class storing functions to extract node-data, edge-data, and node-labels
  // depending on the number and type of constructor parameters and depending on a Network type, the 3 functions are initialized smartly.
  template<OptionalPhylogenyType Network,
           class ExtractNodeLabel_  = void>
  struct DataExtracter_nl_ {
    using ExtractNodeLabel = ExtractNodeLabel_;

    static constexpr bool custom_node_label_maker = not std::is_same_v<ExtractNodeLabel, DefaultExtractData<Ex_node_label, Network>>;
    static constexpr bool ignoring_node_labels = false;

    [[ no_unique_address ]] ExtractNodeLabel get_node_label;

    template<class... Args> requires std::is_invocable_v<ExtractNodeLabel, Args&&...>
    decltype(auto) operator()(const Ex_node_label, Args&&... args) { return get_node_label(std::forward<Args>(args)...); }
    template<class... Args> requires std::is_invocable_v<ExtractNodeLabel, Args&&...>
    decltype(auto) operator()(const Ex_node_label, Args&&... args) const { return get_node_label(std::forward<Args>(args)...); }

    DataExtracter_nl_() = default;

    template<class First, class... Args>
      requires (not mstd::is_any_of<First, DataExtracter_nl_, Ex_node_label>)
    DataExtracter_nl_(First&& first, Args&&... args):
      get_node_label(std::forward<First>(first), std::forward<Args>(args)...)
    {}

    template<class... Args>
    DataExtracter_nl_(Ex_node_label, Args&&... args):
      DataExtracter_nl_(std::forward<Args>(args)...)
    {}

    template<mstd::TupleType NLInit>
    DataExtracter_nl_(std::piecewise_construct_t, NLInit&& nl_init):
      get_node_label(std::make_from_tuple<ExtractNodeLabel>(std::forward<NLInit>(nl_init)))
    {}
  };
  template<OptionalPhylogenyType Network>
  struct DataExtracter_nl_<Network, void> {
    using ExtractNodeLabel = void;

    bool operator()() = delete;

    DataExtracter_nl_() = default;
    DataExtracter_nl_(std::piecewise_construct_t) {}

    static constexpr bool custom_node_label_maker = false;
    static constexpr bool ignoring_node_labels = true;
  };


  // ============== Part 2: extract edge data =================
  template<OptionalPhylogenyType Network,
    class ExtractEdgeData_ = void,
    class ExtractNodeLabel = void>
  struct DataExtracter_ed_nl_: public DataExtracter_nl_<Network, ExtractNodeLabel> {
    using Parent = DataExtracter_nl_<Network, ExtractNodeLabel>;
    using ExtractEdgeData = ExtractEdgeData_;
    using Parent::custom_node_label_maker;
    using Parent::operator();

    static constexpr bool custom_edge_data_maker  = not std::is_same_v<ExtractEdgeData,  DefaultExtractData<Ex_edge_data, Network>>;
    static constexpr bool ignoring_edge_data = false;

    [[ no_unique_address ]] ExtractEdgeData get_edge_data;
    
    DataExtracter_ed_nl_() = default;

    // Ex_node_label or Ex_edge_data are given explicitly
    template<class... Args>
    DataExtracter_ed_nl_(Ex_edge_data, Args&&... args):
      DataExtracter_ed_nl_(std::forward<Args>(args)...)
    {}
    template<class... Args>
    DataExtracter_ed_nl_(Ex_node_label, Args&&... args):
      Parent(std::forward<Args>(args)...)
    {}

    // if the node-label maker is not custom, then the edge data maker gets everything
    template<class First, class... Args>
      requires (not custom_node_label_maker and not mstd::is_any_of<First, DataExtracter_ed_nl_, Ex_edge_data, Ex_node_label>)
    DataExtracter_ed_nl_(First&& first, Args&&... args):
      Parent(),
      get_edge_data(std::forward<First>(first), std::forward<Args>(args)...)
    {}
    // if the edge_data_maker is not custom, then the node_label_maker gets everything
    template<class First, class... Args>
      requires (not custom_edge_data_maker and not mstd::is_any_of<First, DataExtracter_ed_nl_, Ex_edge_data, Ex_node_label>)
    DataExtracter_ed_nl_(Args&&... args):
      Parent(std::forward<Args>(args)...)
    {}

    // piecewise construct case
    template<mstd::TupleType EDInit, class... Args>
      requires (custom_edge_data_maker and custom_node_label_maker)
    DataExtracter_ed_nl_(std::piecewise_construct_t, EDInit&& ed_init, Args&&... args):
      Parent(std::piecewise_construct, std::forward<Args>(args)...),
      get_edge_data(std::make_from_tuple<ExtractEdgeData>(std::forward<EDInit>(ed_init)))
    {}

    // if we have no idea how many arguments are for the edge-data maker, then just use one
    template<class First, class... Args> requires (custom_edge_data_maker and custom_node_label_maker)
    DataExtracter_ed_nl_(First&& first, Args&&... args):
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
    template<class First, class... Args> requires (not EdgeType<First> and not AdjacencyType<First>)
    decltype(auto) operator()(const Ex_edge_data, First&& first, Args&&... args) const {
      return get_edge_data(std::forward<First>(first), std::forward<Args>(args)...);
    }
    template<class First, class... Args> requires (not EdgeType<First> and not AdjacencyType<First>)
    decltype(auto) operator()(const Ex_edge_data, First&& first, Args&&... args) {
      return get_edge_data(std::forward<First>(first), std::forward<Args>(args)...);
    }
    decltype(auto) operator()(const Ex_edge_data) const requires std::is_invocable_v<ExtractEdgeData> { return get_edge_data(); }
    decltype(auto) operator()(const Ex_edge_data) requires std::is_invocable_v<ExtractEdgeData> { return get_edge_data(); }

  };

  template<OptionalPhylogenyType Network, class ExtractNodeLabel>
  struct DataExtracter_ed_nl_<Network, void, ExtractNodeLabel>: public DataExtracter_nl_<Network, ExtractNodeLabel> {
    using Parent = DataExtracter_nl_<Network, ExtractNodeLabel>;
    using ExtractEdgeData = void;
 
    //DataExtracter_ed_nl_() = default;
    INHERIT_ALL_CONSTRUCTORS(DataExtracter_ed_nl_, Parent)
    DataExtracter_ed_nl_(std::piecewise_construct_t) {}

    static constexpr bool custom_edge_data_maker  = false;
    static constexpr bool ignoring_edge_data = true;
  };


  // ============== Part 3: extract node data =================
  template<OptionalPhylogenyType Network,
           class ExtractNodeData_ = void,
           class ExtractEdgeData  = void,
           class ExtractNodeLabel = void>
  struct DataExtracter_: public DataExtracter_ed_nl_<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = DataExtracter_ed_nl_<Network, ExtractEdgeData, ExtractNodeLabel>;
    using Parent::operator();
    using ExtractNodeData = ExtractNodeData_;

    using Parent::custom_node_label_maker;
    using Parent::custom_edge_data_maker;
    static constexpr bool custom_edge_data_or_label_maker = custom_edge_data_maker or custom_node_label_maker;
    static constexpr bool custom_node_data_maker = not std::is_same_v<ExtractNodeData, DefaultExtractData<Ex_node_data, Network>>;
    static constexpr bool ignoring_node_data = false;
    
    [[ no_unique_address ]] ExtractNodeData get_node_data;

    template<class... Args> requires std::is_invocable_v<ExtractNodeData, Args&&...>
    decltype(auto) operator()(const Ex_node_data, Args&&... args) { return get_node_data(std::forward<Args>(args)...); }
    template<class... Args> requires std::is_invocable_v<ExtractNodeData, Args&&...>
    decltype(auto) operator()(const Ex_node_data, Args&&... args) const { return get_node_data(std::forward<Args>(args)...); }

    DataExtracter_() = default;

    // Ex_node_label or Ex_edge_data or Ex_node_data are given explicitly
    template<class... Args>
    DataExtracter_(Ex_node_data, Args&&... args):
      DataExtracter_(std::forward<Args>(args)...)
    {}
    template<class First, class... Args>
      requires mstd::is_any_of<First, Ex_node_label, Ex_edge_data>
    DataExtracter_(First first, Args&&... args):
      Parent(first, std::forward<Args>(args)...)
    {}

    // if the other makers are not custom, then the node data maker gets everything
    template<class First, class... Args>
      requires (not custom_edge_data_or_label_maker and not mstd::is_any_of<First, DataExtracter_, Ex_edge_data, Ex_node_label, Ex_node_data>)
    DataExtracter_(First&& first, Args&&... args):
      Parent(),
      get_node_data(std::forward<First>(first), std::forward<Args>(args)...)
    {}
    // if the node-data maker is not custom, then the others get everything
    template<class First, class... Args>
      requires (not custom_node_data_maker and not mstd::is_any_of<First, DataExtracter_, Ex_edge_data, Ex_node_label, Ex_node_data>)
    DataExtracter_(Args&&... args):
      Parent(std::forward<Args>(args)...)
    {}

    // piecewise construct case
    template<mstd::TupleType NDInit, class... Args>
      requires (custom_node_data_maker and custom_edge_data_or_label_maker)
    DataExtracter_(std::piecewise_construct_t, NDInit&& nd_init, Args&&... args):
      Parent(std::piecewise_construct, std::forward<Args>(args)...),
      get_node_data(std::make_from_tuple<ExtractNodeData>(std::forward<NDInit>(nd_init)))
    {}

    // if we have no idea how many arguments are for the node-data maker, then just use one
    template<class First, class... Args> requires (custom_node_data_maker and custom_edge_data_or_label_maker)
    DataExtracter_(First&& first, Args&&... args):
      Parent(std::forward<Args>(args)...),
      get_node_data(std::forward<First>(first))
    {}

  };

  template<OptionalPhylogenyType Network,
           class ExtractEdgeData,
           class ExtractNodeLabel>
  struct DataExtracter_<Network, void, ExtractEdgeData, ExtractNodeLabel>: public DataExtracter_ed_nl_<Network, ExtractEdgeData, ExtractNodeLabel> {
    using Parent = DataExtracter_ed_nl_<Network, ExtractEdgeData, ExtractNodeLabel>;
    using ExtractNodeData = void;
    
    //DataExtracter_() = default;
    INHERIT_ALL_CONSTRUCTORS(DataExtracter_, Parent)

    DataExtracter_(std::piecewise_construct_t) {}

    static constexpr bool custom_node_data_maker = false;
    static constexpr bool ignoring_node_data = true;
  };


  // ============== Putting it all together: DataExtracter  =================
  // accumulate the defined classes into one and provide a nice interface
  template<OptionalPhylogenyType Network,
           class ExtractNodeData  = DefaultExtractData<Ex_node_data, Network>,
           class ExtractEdgeData  = DefaultExtractData<Ex_edge_data, Network>,
           class ExtractNodeLabel = DefaultExtractData<Ex_node_label, Network>>
  using DataExtracter = DataExtracter_<Network, ExtractNodeData, ExtractEdgeData, ExtractNodeLabel>;


  // -------- DataExtracter: deduction guides --------------
  
  // -------- DataExtracter: concepts --------------
  // see types

  // -------- DataExtracter: defaults --------------
  // To make a data extracter, you can use make_data_extracter<SourcePhylo>(),
  // where SourcePhylo may be void, indicating that data is generated, not extracted.
  //
  // Always pass arguments in the following order:
  // 1. node-data extracter/generator
  // 2. edge-data extracter/generator
  // 3. node-label extracter/generator
  //
  // NOTE: in order to pass only a subset of the extracters/generators, prepend your extracters/generators with
  //    Ex_node_data{} or Ex_edge_data{} or Ex_node_label{}
  // NOTE: always keep the correct order (node-data, edge-data, node-label), even when passing a subset of functors
  //    For example, 
  //      make_data_extracter(Ex_edge_data{}, A) will only extract edge-data using the functor A
  //      make_data_extracter(A, Ex_node_label{}, B) will extract node-data via A and node-labels via B
  //      make_data_extracter(A, B) will extract node-data via A and edge-data via B
  //      make_data_extracter(Ex_edge_data{}, A, B) will extract edge-data via A and node-labels via B
  //      make_data_extracter(Ex_node_label{}, A, B) will not compile
  // NOTE: these functions are chiefly for use within decltype() to determine the type of the Extracter before its initialization

  // stage 1:
  template<OptionalPhylogenyType First, class NL> requires (not mstd::is_any_of<NL, Ex_node_data, Ex_edge_data, Ex_node_label>)
  auto make_data_extracter_nl(NL&& nl) {
    return DataExtracter_nl_<First, std::remove_reference_t<NL>>(std::forward<NL>(nl));
  }

  template<OptionalPhylogenyType First, class NL>
  auto make_data_extracter_nl(Ex_node_label, NL&& nl) { return make_data_extracter_nl<First>(std::forward<NL>(nl)); }

  template<OptionalPhylogenyType First>
  auto make_data_extracter_nl() { return DataExtracter_nl_<First>(); }


  // stage 2:
  template<OptionalPhylogenyType First, class ED, class... Args> requires (not mstd::is_any_of<ED, Ex_node_data, Ex_edge_data, Ex_node_label>)
  auto make_data_extracter_ed_nl(ED&& ed, Args&&... args) {
    if constexpr (sizeof...(Args) != 0) {
      using NL_Extract = decltype(make_data_extracter_nl<First>(std::forward<Args>(args)...));
      using ExtractNodeLabel = typename NL_Extract::ExtractNodeLabel;
      return DataExtracter_ed_nl_<First, std::remove_reference_t<ED>, ExtractNodeLabel>(Ex_edge_data{}, std::forward<ED>(ed), std::forward<Args>(args)...);
    } else return DataExtracter_ed_nl_<First, std::remove_reference_t<ED>, void>(Ex_edge_data{}, std::forward<ED>(ed));
  }
  template<OptionalPhylogenyType First, class... Args>
  auto make_data_extracter_ed_nl(Ex_node_label, Args&&... args) {
    using NL_Extract = decltype(make_data_extracter_nl<First>(std::forward<Args>(args)...));
    using ExtractNodeLabel = typename NL_Extract::ExtractNodeLabel;
    return DataExtracter_ed_nl_<First, void, ExtractNodeLabel>(std::forward<Args>(args)...);
  }
  template<OptionalPhylogenyType First, class... Args>
  auto make_data_extracter_ed_nl(Ex_edge_data, Args&&... args) {  return make_data_extracter_ed_nl<First>(std::forward<Args>(args)...); }
  template<OptionalPhylogenyType First>
  auto make_data_extracter_ed_nl() { return DataExtracter_ed_nl_<First>(); }


  // stage 3:
  template<OptionalPhylogenyType First, class ND, class... Args>
    requires (not DataExtracterType<ND> and not mstd::is_any_of<ND, Ex_node_data, Ex_edge_data, Ex_node_label>)
  auto make_data_extracter(ND&& nd, Args&&... args) {
    if constexpr (sizeof...(Args) != 0) {
      using EDNL_Extract = decltype(make_data_extracter_ed_nl<First>(std::forward<Args>(args)...));
      using ExtractNodeLabel = typename EDNL_Extract::ExtractNodeLabel;
      using ExtractEdgeData = typename EDNL_Extract::ExtractEdgeData;
      return DataExtracter_<First, std::remove_reference_t<ND>, ExtractEdgeData, ExtractNodeLabel>(std::forward<ND>(nd), std::forward<Args>(args)...);
    } else return DataExtracter_<First, std::remove_reference_t<ND>, void, void>(Ex_node_data{}, std::forward<ND>(nd));
  }
  template<OptionalPhylogenyType First, class Tag, class... Args> requires (mstd::is_any_of<Tag, Ex_node_label, Ex_edge_data>)
  auto make_data_extracter(Tag tag, Args&&... args) {
    using EDNL_Extract = decltype(make_data_extracter_ed_nl<void>(tag, std::forward<Args>(args)...));
    using ExtractNodeLabel = typename EDNL_Extract::ExtractNodeLabel;
    using ExtractEdgeData = typename EDNL_Extract::ExtractEdgeData;
    return DataExtracter_<First, void, ExtractEdgeData, ExtractNodeLabel>(std::forward<Args>(args)...);
  }
  template<OptionalPhylogenyType First, class... Args>
  auto make_data_extracter(Ex_node_data, Args&&... args) {  return make_data_extracter<First>(std::forward<Args>(args)...); }
  template<OptionalPhylogenyType First>
  auto make_data_extracter() { return DataExtracter_<First>(); }


  //! In order to allow passing a pre-made data extracter to the make_emplacer helper functions, we allow passing one here
  template<OptionalPhylogenyType SourcePhylo, DataExtracterType PremadeExtracter>
  auto make_data_extracter(PremadeExtracter&& extracter) { return extracter; }


  template<class T> struct DefaultDataExtracter_{};
 
  template<DataExtracterType Extracter> 
  struct DefaultDataExtracter_<Extracter>{ using type = Extracter; };
 
  template<OptionalPhylogenyType SourcePhylo>
  struct DefaultDataExtracter_<SourcePhylo>
  { using type = DataExtracter<SourcePhylo,
                         DefaultExtractData<Ex_node_data, SourcePhylo>,
                         DefaultExtractData<Ex_edge_data, SourcePhylo>,
                         DefaultExtractData<Ex_node_label, SourcePhylo>>;
  };

  template<class T> using DefaultDataExtracter = typename DefaultDataExtracter_<T>::type;

}
