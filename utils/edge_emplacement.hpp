
#pragma once

#include <exception>

#include "optional_tuple.hpp"

#include "types.hpp"
#include "extract_data.hpp"

namespace PT {
  // ========== EdgeEmplacement ==========

  // ------- Emplacement Helper: helpers ---------
  struct EmplacerOptions {
    bool forbid_non_binary :1 = false;
    bool forbid_junctions :1 = false;
    bool forbid_parallel_edges :1 = false;

    EmplacerOptions operator+(const EmplacerOptions other) const {
      return EmplacerOptions{
        forbid_non_binary || other.forbid_non_binary,
        forbid_junctions  || other.forbid_junctions,
        forbid_parallel_edges || other.forbid_parallel_edges
      };
    };
    // check if an edge-addition is OK
    template<StrictPhylogenyType Phylo>
    void sanity_check(const Phylo& N, const NodeDesc u, const NodeDesc v) const {
      if(forbid_parallel_edges && test(Phylo::children(u), v))
        throw mstd::MalformedInput("trying to make double edge, which was explicitly forbidden");
      if(forbid_non_binary && ((Phylo::out_degree(u) > 1) || (Phylo::in_degree(v) > 1)))
        throw mstd::MalformedInput("found non-binary node, which was explicitly forbidden");
      if(forbid_junctions) {
        if((Phylo::in_degree(v) > 0) && (Phylo::out_degree(v) > 1))
          throw mstd::MalformedInput("found reticulation with multiple children ('junction') which was explicitly forbidden");
        if((Phylo::in_degree(u) > 1) && (Phylo::out_degree(u) > 0))
          throw mstd::MalformedInput("found reticulation with multiple children ('junction') which was explicitly forbidden");
      }
    }
  };

  EmplacerOptions EO_forbid_non_binary{.forbid_non_binary = true};
  EmplacerOptions EO_forbid_junctions{.forbid_junctions = true};
  EmplacerOptions EO_forbid_parallel_edges{.forbid_parallel_edges = true};
  static_assert(sizeof(EmplacerOptions) == 1);

  template<class T> constexpr bool is_emplacer_options = false;
  template<> constexpr bool is_emplacer_options<EmplacerOptions> = true;


  // ------- Emplacement Helper: main class ---------
  // NOTE: if track_roots is false, the user is resposable to mark the root(s) in the new network
  // NOTE: moving the old_to_new translation into the Helper class was the easiest way to allow for it to be a reference
  //       while still letting the compiler infer the template parameters of the EdgeEmplacer. I apologize for the dirty hack.
  template<StrictPhylogenyType TargetPhylo_,
           bool _track_roots = true,
           OptionalMapsToNode OldToNewTranslation_ = NodeTranslation>
  struct EdgeEmplacementHelper:
    public mstd::optional_tuple<mstd::NoRef<OldToNewTranslation_>, std::conditional_t<_track_roots, NodeSet, void>>
  {
    using OldToNewTranslation = mstd::NoRef<OldToNewTranslation_>;
    using TargetPhylo = TargetPhylo_;
    using Parent = mstd::optional_tuple<OldToNewTranslation, std::conditional_t<_track_roots, NodeSet, void>>;

    static constexpr bool indirect_translation = std::is_pointer_v<OldToNewTranslation>;
    static constexpr bool translating = not std::is_void_v<OldToNewTranslation_>;
    static constexpr bool track_roots = _track_roots;
    TargetPhylo* N = nullptr;
    EmplacerOptions options;
    
    auto& old_to_new() requires (translating) {
      auto& result = this->template get<0>();
      if constexpr (indirect_translation) return *result; else return result;
    }
    const auto& old_to_new() const requires (translating) {
      const auto& result = this->template get<0>();
      if constexpr (indirect_translation) return *result; else return result;
    }
    auto& root_candidates() requires (track_roots) { return this->template get<1>(); }
    const auto& root_candidates() const requires (track_roots) { return this->template get<1>(); }

    template<MapsToNode<mstd::TR_ConstRefPtrOK> OldToNew>
      requires (not std::is_void_v<OldToNewTranslation>)
    EdgeEmplacementHelper(TargetPhylo& N_, EmplacerOptions opts, OldToNew&& old_to_new):
      Parent(std::forward<OldToNew>(old_to_new)),
      N{&N_},
      options{opts}
    {}

    template<MapsToNode<mstd::TR_ConstRefPtrOK> OldToNew>
      requires (not std::is_void_v<OldToNewTranslation>)
    EdgeEmplacementHelper(TargetPhylo& N_, OldToNew&& old_to_new):
      EdgeEmplacementHelper(N_, EmplacerOptions{}, std::forward<OldToNew>(old_to_new))
    {}

    EdgeEmplacementHelper(TargetPhylo& N_, EmplacerOptions opts):
      Parent(), N{&N_}, options{opts}
    {}

    EdgeEmplacementHelper(TargetPhylo& N_):
      EdgeEmplacementHelper(N_, EmplacerOptions{})
    {}

    EdgeEmplacementHelper() = default;
    EdgeEmplacementHelper(const EdgeEmplacementHelper&) = default;
    EdgeEmplacementHelper(EdgeEmplacementHelper&&) = default;

    EdgeEmplacementHelper& operator=(const EdgeEmplacementHelper&) = default;
    EdgeEmplacementHelper& operator=(EdgeEmplacementHelper&&) = default;

    auto register_node(const auto& x) { return old_to_new().emplace(std::piecewise_construct, std::tuple{x}, std::tuple{}); }

    template<class... Args>
      requires ((not TargetPhylo::has_node_data) || (std::is_constructible_v<typename TargetPhylo::NodeData, Args&&...>))
    NodeDesc create_node(Args&&... args) {
      assert(N != nullptr);
      N->count_node();
      const NodeDesc v = N->create_node(std::forward<Args>(args)...);
      if constexpr (track_roots)
        mstd::append(root_candidates(), v);
      return v;
    }

    void set_label(const NodeDesc u, auto&& label) {
      if constexpr (TargetPhylo::has_node_labels) {
        TargetPhylo::label(u) = label;
        DEBUG5(std::cout << "set label of node "<< u <<" to '" << label <<"'\n");
      }
    }

    template<class... Args>
      requires ((not TargetPhylo::has_edge_data) or (std::is_constructible_v<typename TargetPhylo::EdgeData, Args&&...>))
    auto add_an_edge(const NodeDesc u, const NodeDesc v, Args&&... args) {
      assert(N != nullptr);
      if constexpr (track_roots)
        mstd::erase(root_candidates(), v);
      options.sanity_check(*N, u, v);
      return N->add_edge(u,v, std::forward<Args>(args)...);
    }
 
    // subdivide uv with w
    template<EdgeType Edge, class Data>
    void subdivide_edge(Edge&& uv, const NodeDesc w, Data&& data) {
      assert(N != nullptr);
      N->subdivide_edge(uv, w, [&](auto&&, auto& wv){ wv._data = std::forward<Data>(data); });
    }
    template<EdgeType Edge>
    void subdivide_edge(Edge&& uv, const NodeDesc w) {
      assert(N != nullptr);
      N->subdivide_edge(uv, w);
    }
 

    bool mark_root(const auto& r) requires (translating) {
      return mark_root_directly(old_to_new().at(r));
    }
    bool mark_root_directly(const NodeDesc r) {
      assert(N != nullptr);
      assert(N->in_degree(r) == 0);
      return mstd::append(N->_roots, r).second;
    }

    void clear() {
      if constexpr (track_roots) root_candidates().clear();
      if constexpr (translating) old_to_new().clear();
    }

    // NOTE: this is not private, so the user can commit roots at any point
    //       However, be aware that roots will also be committed upon destruction!
    // NOTE: only call this if you're tracking roots
    void commit_roots() {
      assert(N != nullptr);
      if constexpr (track_roots) {
        DEBUG4(std::cout << "committing roots: "<<root_candidates()<<"\n");
        for(const NodeDesc r: root_candidates())
          if(N->in_degree(r) == 0)
            mstd::append(N->_roots, r);
        root_candidates().clear();
      }
    }

    // NOTE: we're only going to commit roots if we're not currently handling an exception
    //        (since mstd::singleton will throw a new exception on top if we're trying to add multiple roots to it here)
    ~EdgeEmplacementHelper() {
      if constexpr (track_roots)
        if(std::uncaught_exceptions() == 0)
          commit_roots();
    }

  };

  // ------- Emplacement Helper: deduction guides ---------
  // ------- Emplacement Helper: concepts ---------
  // T is an EmplacementHelper iff T::add_an_edge can be invoked with 2 NodeDesc's (this implies that T is not const)
  template<class T> concept StrictEmplacementHelperType = requires(T& t, NodeDesc u) { t.add_an_edge(u,u); };
  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept EmplacementHelperType = mstd::apply_rune_v<T, rune> or StrictEmplacementHelperType<mstd::apply_rune_t<T, rune>>;


  // ------- Emplacement Helper: defaults ---------
  template<StrictPhylogenyType TargetPhylo,
           bool track_roots = true,
           OptionalMapsToNode OldToNew = NodeTranslation>
  using DefaultEdgeEmplacementHelper = EdgeEmplacementHelper<TargetPhylo, track_roots, OldToNew>;



  // ============== Edge Emplacer =================
  // ------- Edge Emplacer: helpers -----------
  // ------- Edge Emplacer: main class --------
  template<StrictEmplacementHelperType Helper_, StrictDataExtracterType Extracter_>
  struct EdgeEmplacer {
#warning "TODO: make this inherit from the Helper"
    using Helper = Helper_;
    using Extracter = Extracter_;
    using TargetPhylo = typename Helper::TargetPhylo;
    using OldToNewTranslation = typename Helper::OldToNewTranslation;
    static constexpr bool track_roots = Helper::track_roots;
    static constexpr bool extract_labels = not Extracter::ignoring_node_labels;
    static constexpr bool extract_node_data = not Extracter::ignoring_node_data;
    static constexpr bool extract_edge_data = not Extracter::ignoring_edge_data;

    Helper helper;
    Extracter data_extracter;

    // copy and move construction
    EdgeEmplacer(const EdgeEmplacer& other) = default;
    EdgeEmplacer(EdgeEmplacer&& other) = default;

    // passing both Helper and Extracter
    template<EmplacementHelperType EH, class... Args>
    EdgeEmplacer(EH&& _helper, Args&&... args):
      helper(std::forward<EH>(_helper)),
      data_extracter(std::forward<Args>(args)...)
    {}

    template<DataExtracterType DET, class... Args>
    EdgeEmplacer(DET&& _data_extracter, Args&&... args):
      helper(std::forward<Args>(args)...),
      data_extracter(std::forward<DET>(_data_extracter))
    {}

    // piecewise constructing Helper and Extracter
    template<mstd::TupleType HelperInit, mstd::TupleType ExtracterInit>
    EdgeEmplacer(const std::piecewise_construct_t, HelperInit&& _helper, ExtracterInit&& _extracter):
      helper(std::make_from_tuple<Helper>(std::forward<HelperInit>(_helper))),
      data_extracter(std::make_from_tuple<Extracter>(std::forward<ExtracterInit>(_extracter))) {}


    // constructing Helper and Extracter
    // NOTE: you can only pass <=2 additional arguments to the helper (an EmplacerOptions struct and anything to initialize the OldToNew translation with)
    //        everything else will go to the initialization of the DataExtracter
    // NOTE: std::is_constructible ONLY CHECKS THE "IMMEDIATE CONTEXT", so it could be that Helper CANNOT indeed be constructed that way...
    //        I'll try to work around that by constraining the construction of the helper as much as possible....
    // 1. 0 arguments
    template<PhylogenyType Phylo>
      requires (std::is_constructible_v<Helper, Phylo&&> and std::is_default_constructible_v<Extracter>)
    EdgeEmplacer(Phylo&& N):
      helper(std::forward<Phylo>(N)), data_extracter() {}

    // 2. 1 argument
    template<PhylogenyType Phylo, class Arg1>
      requires (std::is_constructible_v<Helper, Phylo&&, Arg1&&> and std::is_default_constructible_v<Extracter>)
    EdgeEmplacer(Phylo&& N, Arg1&& arg1):
      helper(std::forward<Phylo>(N), std::forward<Arg1>(arg1)), data_extracter() {}

    template<PhylogenyType Phylo, class Arg1>
      requires (not std::is_constructible_v<Helper, Phylo&&, Arg1&&> and 
                std::is_constructible_v<Helper, Phylo&&> and
                std::is_constructible_v<Extracter, Arg1&&>)
    EdgeEmplacer(Phylo&& N, Arg1&& arg1):
      helper(std::forward<Phylo>(N)), data_extracter(std::forward<Arg1>(arg1)) {}

    // 3. 2 or more arguments
    template<PhylogenyType Phylo, class Arg1, class Arg2, class... MoreArgs>
      requires (std::is_constructible_v<Helper, Phylo&&, Arg1&&, Arg2&&> and std::is_constructible_v<Extracter, MoreArgs&&...>)
    EdgeEmplacer(Phylo&& N, Arg1&& arg1, Arg2&& arg2, MoreArgs&&... args):
      helper(std::forward<Phylo>(N), std::forward<Arg1>(arg1), std::forward<Arg2>(arg2)), data_extracter(std::forward<MoreArgs>(args)...) {}

    template<PhylogenyType Phylo, class Arg1, class Arg2, class... MoreArgs>
      requires (not std::is_constructible_v<Helper, Phylo&&, Arg1&&, Arg2&&> and
                std::is_constructible_v<Helper, Phylo&&, Arg1&&> and
                std::is_constructible_v<Extracter, Arg2&&, MoreArgs&&...>)
    EdgeEmplacer(Phylo&& N, Arg1&& arg1, Arg2&& arg2, MoreArgs&&... args):
      helper(std::forward<Phylo>(N), std::forward<Arg1>(arg1)), data_extracter(std::forward<Arg2>(arg2), std::forward<MoreArgs>(args)...) {}

    template<PhylogenyType Phylo, class Arg1, class Arg2, class... MoreArgs>
      requires (not std::is_constructible_v<Helper, Phylo&&, Arg1&&, Arg2&&> and
                not std::is_constructible_v<Helper, Phylo&&, Arg1&&> and
                std::is_constructible_v<Helper, Phylo&&> and
                std::is_constructible_v<Extracter, Arg1&&, Arg2&&, MoreArgs&&...>)
    EdgeEmplacer(Phylo&& N, Arg1&& arg1, Arg2&& arg2, MoreArgs&&... args):
      helper(std::forward<Phylo>(N)), data_extracter(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), std::forward<MoreArgs>(args)...) {}


    bool contains(const NodeDesc u) { return helper.old_to_new().contains(u); }
    NodeDesc at(const NodeDesc u) { assert(contains(u)); return helper.old_to_new().at(u); }
    NodeDesc lookup(const NodeDesc u, const NodeDesc _default = NoNode) const {
      return_map_lookup(helper.old_to_new(), u, _default);
    }

    void clear() { helper.clear(); }


    // --------------- labels --------------------
    template<class... Args>
    void set_label(Args&&... args) { helper.set_label(std::forward<Args>(args)...); }


    // --------------- nodes --------------------
    template<class... Args>
    NodeDesc create_copy_of_raw(Args&&... args) {
      DEBUG6(std::cout << "extracting node data? "<<extract_node_data<<'\n');
      if constexpr (extract_node_data and (std::is_invocable_v<Extracter, Ex_node_data, Args&&...>)) {
        return helper.create_node(data_extracter(Ex_node_data{}, std::forward<Args>(args)...));
      } else return helper.create_node(std::forward<Args>(args)...);
    }
    template<class... Args>
    NodeDesc create_node(Args&&... args) { return create_copy_of_raw(std::forward<Args>(args)...); }
    template<class... Args>
    NodeDesc create_root(Args&&... args) {
      const NodeDesc result = create_copy_of_raw(std::forward<Args>(args)...);
      helper.mark_root_directly(result);
      return result;
    }


    // call to create a copy of the node other_u and either extract its data & label, or pass the data and ignore the label (set it yourself later)
    template<class... Args> requires (Helper::translating)
    NodeDesc create_copy_of(const auto& other_u, Args&&... args) {
      DEBUG6(std::cout << "creating a copy of "<<other_u<<" in translation @"<<&(helper.old_to_new())<<'\n');
      // check if other_u is known to the translation
      const auto [u_iter, u_success] = helper.register_node(other_u);
      NodeDesc& u_copy = u_iter->second;
      if(u_success) {
        // if other_u has not been seen before, insert it as new root
        if constexpr (extract_node_data) {
          u_copy = create_copy_of_raw(other_u, std::forward<Args>(args)...);
        } else u_copy = create_copy_of_raw(std::forward<Args>(args)...);
        DEBUG4(std::cout << "created copy " << u_copy << " of "<< other_u<<"\n");
        // copy label from the source 
        if constexpr (extract_labels) {
          set_label(u_copy, data_extracter(Ex_node_label{}, other_u));
        }
      }
      return u_copy;
    }

    
    // --------------- edges --------------------
    template<class... MoreArgs>
    auto emplace_edge_raw(const NodeDesc u, const NodeDesc v, MoreArgs&&... args) {
      DEBUG6(std::cout << "only adding edge "<< u <<" ----> "<< v <<"\n");
      // if the data-extracter can be called with MoreArgs, then use it to make data, otherwise, just pass MoreArgs to the edge creation
      if constexpr (extract_edge_data && (std::is_invocable_v<Extracter, Ex_edge_data, MoreArgs&&...>)) {
        return helper.add_an_edge(u, v, data_extracter(Ex_edge_data{}, std::forward<MoreArgs>(args)...));
      } else return helper.add_an_edge(u, v, std::forward<MoreArgs>(args)...);
    }

    template<class... MoreArgs>
    auto emplace_edge(const auto& other_u, const auto& other_v, MoreArgs&&... args) {
      DEBUG5(std::cout << "treating ("<<other_u<<" "<<other_v<<")\n");
      return emplace_edge_raw(create_copy_of(other_u), create_copy_of(other_v), std::forward<MoreArgs>(args)...);
    }

    template<class P, class Q, class... MoreArgs>
    auto emplace_edge(const std::pair<P,Q>& other_uv, MoreArgs&&... args) {
      return emplace_edge(other_uv.first, other_uv.second, std::forward<MoreArgs>(args)...);
    }

    template<EdgeType Edge, class... MoreArgs>
    auto emplace_edge(Edge&& uv, MoreArgs&&... args) {
      if constexpr (extract_edge_data)
        return emplace_edge(uv.as_pair(), std::forward<Edge>(uv), std::forward<MoreArgs>(args)...);
      else
        return emplace_edge(uv.as_pair(), std::forward<MoreArgs>(args)...);
    }

    // subdivide uv with w
    template<EdgeType Edge, class... MoreArgs>
    void subdivide_edge(Edge&& uv, const NodeDesc w, MoreArgs&&... args) {
      if constexpr (extract_edge_data && (std::is_invocable_v<Extracter, Ex_edge_data, MoreArgs&&...>)) {
        helper.subdivide_edge(std::forward<Edge>(uv), w, data_extracter(Ex_edge_data{}, std::forward<MoreArgs>(args)...));
      } else helper.subdivide_edge(std::forward<Edge>(uv), w, std::forward<MoreArgs>(args)...);
    }


    // --------------- roots --------------------
    bool mark_root(const NodeDesc r) { return helper.mark_root(r); }
    bool mark_root_directly(const NodeDesc r) { return helper.mark_root_directly(r); }

    // translate the roots of N to use as our roots
    template<NodeIterableType Roots>
    void mark_roots(const Roots& rts) {
      for(const NodeDesc r: rts)
        mark_root(r);
    }
    // commit the root-candidates to N
    void commit_roots() { helper.commit_roots(); }

    // get node data
    static constexpr auto& get_node(const NodeDesc u) { return node_of<TargetPhylo>(u); }
  };

  // -------- Edge Emplacer: deduction guides --------------
  template<EmplacementHelperType Helper_, DataExtracterType Extracter>
  EdgeEmplacer(Helper_&&, Extracter&&) -> EdgeEmplacer<std::remove_cvref_t<Helper_>, std::remove_cvref_t<Extracter>>;

  // -------- Edge Emplacer: concepts --------------
  template<class T> concept StrictEdgeEmplacerType = EmplacementHelperType<typename T::Helper>;
  template<class T, mstd::TypeRune rune = mstd::TR_ConstRefOK>
  concept EdgeEmplacerType = mstd::apply_rune_v<T, rune> or StrictEdgeEmplacerType<mstd::apply_rune_t<T, rune>>;

  // -------- Edge Emplacer: defaults  -------------- 
  template<StrictPhylogenyType TargetPhylo,
           bool track_roots = true,
           class SourceOrExtract = void,
           OptionalMapsToNode OldToNew = NodeTranslation>
  using DefaultEdgeEmplacer = EdgeEmplacer<EdgeEmplacementHelper<TargetPhylo, track_roots, OldToNew>, DefaultDataExtracter<SourceOrExtract>>;


  // the rest of this file are convenience functions to not have to write sooo much all the time

  // this allows you to write "auto emp = EdgeEmplacers<track_roots>::make_emplacer(network, old_to_new, ...)" without specifying template parameters where
  // network = the network
  // old_to_new = a node translation
  // ... = data extracter functions as specified in <extract_data.hpp>
  template<bool track_roots, OptionalPhylogenyType SourcePhylo = void>
  struct EdgeEmplacers {
    template<StrictPhylogenyType TargetPhylo, MapsToNode OldToNewTranslation, class... Args>
    static auto make_emplacer(TargetPhylo& N, OldToNewTranslation&& old_to_new, Args&&... args) {
      // note that 'OldToNewTranslation' is an lvalue if old_to_new is an rvalue-ref and and lvalue-ref if old_to_new is an lvalue-ref...
      // thus, if an existing translation is passed, the helper will contain a reference to this translation, otherwise the helper has its own translation
      using Helper = EdgeEmplacementHelper<TargetPhylo, track_roots, OldToNewTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo>(std::forward<Args>(args)...));
      return EdgeEmplacer<Helper, Extracter>(
          Helper(N, std::forward<OldToNewTranslation>(old_to_new)),
          std::forward<Args>(args)...);
    }

    // allow giving a custom emplacement helper
    // NOTE: we will overwrite the Network* inside the helper with a pointer to the correct network
    template<StrictPhylogenyType TargetPhylo, EmplacementHelperType Helper, class... Args>
    static auto make_emplacer(TargetPhylo& N, Helper&& helper, Args&&... args) {
      using Extracter = decltype(make_data_extracter<SourcePhylo>(std::forward<Args>(args)...));
      helper.N = &N; // set the network of the helper
      return EdgeEmplacer<Helper, Extracter>(
          std::forward<Helper>(helper),
          std::forward<Args>(args)...);
    }

    template<StrictPhylogenyType TargetPhylo, class T, class... Args>
      requires (not MapsToNode<T> and not EmplacementHelperType<T>)
    static auto make_emplacer(TargetPhylo& N, T&& t, Args&&... args) {
      using Helper = EdgeEmplacementHelper<TargetPhylo, track_roots, NodeTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo>(std::forward<T>(t), std::forward<Args>(args)...));
      return EdgeEmplacer<Helper, Extracter>(
          Helper{N},
          std::forward<T>(t), std::forward<Args>(args)...);
    }
    template<StrictPhylogenyType TargetPhylo>
    static auto make_emplacer(TargetPhylo& N) {
      using Helper = EdgeEmplacementHelper<TargetPhylo, track_roots, NodeTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo>());
      return EdgeEmplacer<Helper, Extracter>(N);
    }
  };


}

