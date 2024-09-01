
#pragma once

#include "optional_tuple.hpp"

#include "types.hpp"
#include "extract_data.hpp"

namespace PT {
  // ========== EdgeEmplacement ==========

  // NOTE: if track_roots is false, the user is resposable to mark the root(s) in the new network
  // NOTE: moving the old_to_new translation into the Helper class was the easiest way to allow for it to be a reference
  //       while still letting the compiler infer the template parameters of the EdgeEmplacer. I apologize for the dirty hack.
  template<bool _track_roots,
           StrictPhylogenyType _TargetPhylo,
           OptionalPhylogenyType _SourcePhylo = void,
           OptionalMapsToNode _OldToNewTranslation = NodeTranslation>
  struct EdgeEmplacementHelper: public mstd::optional_tuple<mstd::prefer_pointer<_OldToNewTranslation>, std::conditional_t<_track_roots, NodeSet, void>>
  {
    using OldToNewTranslation = mstd::prefer_pointer<_OldToNewTranslation>;
    using SourcePhylo = _SourcePhylo;
    using TargetPhylo = _TargetPhylo;
    using Parent = mstd::optional_tuple<OldToNewTranslation, std::conditional_t<_track_roots, NodeSet, void>>;

    static constexpr bool indirect_translation = std::is_pointer_v<OldToNewTranslation>;
    static constexpr bool translating = not std::is_void_v<_OldToNewTranslation>;
    static constexpr bool track_roots = _track_roots;
    TargetPhylo* N = nullptr;
    
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

    template<class... Args>
    EdgeEmplacementHelper(TargetPhylo& _N, Args&&... args): Parent(std::forward<Args>(args)...), N(&_N) {}

    EdgeEmplacementHelper() = default;
    EdgeEmplacementHelper(const EdgeEmplacementHelper&) = default;
    EdgeEmplacementHelper(EdgeEmplacementHelper&&) = default;

    EdgeEmplacementHelper& operator=(const EdgeEmplacementHelper&) = default;
    EdgeEmplacementHelper& operator=(EdgeEmplacementHelper&&) = default;

  protected:
  public:

    auto register_node(const auto& x) { return old_to_new().emplace(std::piecewise_construct, std::tuple{x}, std::tuple{}); }

    template<class... Args>
    NodeDesc create_node(Args&&... args) {
      assert(N != nullptr);
      if constexpr ((not TargetPhylo::has_node_data) || (std::is_constructible_v<typename TargetPhylo::NodeData, Args&&...>)) {
        N->count_node();
        const NodeDesc v = N->create_node(std::forward<Args>(args)...);
        if constexpr (track_roots)
          mstd::append(root_candidates(), v);
        return v;
      } else throw mstd::MalformedInput{"cannot construct node-data with provided parameters"};
    }

    void set_label(const NodeDesc u, auto&& label) {
      if constexpr (TargetPhylo::has_node_labels) {
        TargetPhylo::label(u) = label;
        DEBUG4(std::cout << "set label of node "<< u <<" to '" << label <<'\n');
      }
    }

    template<class... Args>
    void add_an_edge(const NodeDesc u, const NodeDesc v, Args&&... args) {
      assert(N != nullptr);
      if constexpr ((not TargetPhylo::has_edge_data) || (std::is_constructible_v<typename TargetPhylo::EdgeData, Args&&...>)) {
        if constexpr (track_roots)
          mstd::erase(root_candidates(), v); 
        N->add_edge(u,v, std::forward<Args>(args)...);
      } else throw mstd::MalformedInput{"cannot construct edge-data with provided parameters"};
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
        DEBUG3(std::cout << "committing roots: "<<root_candidates()<<"\n");
        for(const NodeDesc r: root_candidates())
          if(N->in_degree(r) == 0)
            mstd::append(N->_roots, r);
        root_candidates().clear();
      }
    }

    ~EdgeEmplacementHelper() {
      if constexpr (track_roots) commit_roots();
    }

  };

  // ------- Emplacement Helper: deduction guides ---------
  // ------- Emplacement Helper: concepts ---------
  // T is an EmplacementHelper iff T::add_an_edge can be invoked with 2 NodeDesc's
  template<class T> concept StrictEmplacementHelperType = requires(T& t, NodeDesc u) { t.add_an_edge(u,u); };
  template<class T> concept EmplacementHelperType = StrictEmplacementHelperType<std::remove_cvref_t<T>>;

  // ------- Emplacement Helper: defaults ---------
  template<StrictPhylogenyType TargetPhylo, bool track_roots = true, OptionalPhylogenyType SourcePhylo = void, OptionalMapsToNode OldToNew = NodeTranslation>
  using DefaultEdgeEmplacementHelper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, OldToNew>;

  // ============== Edge Emplacer =================
  template<StrictEmplacementHelperType _Helper, StrictDataExtracterType Extracter>
  struct EdgeEmplacer {
#warning "TODO: make this inherit from the Helper"
    using Helper = _Helper;
    using SourcePhylo = typename Helper::SourcePhylo;
    using TargetPhylo = typename Helper::TargetPhylo;
    using OldToNewTranslation = typename Helper::OldToNewTranslation;
    static constexpr bool track_roots = Helper::track_roots;
    static constexpr bool extract_labels = not Extracter::ignoring_node_labels;
    static constexpr bool extract_node_data = not Extracter::ignoring_node_data;
    static constexpr bool extract_edge_data = not Extracter::ignoring_edge_data;

    Helper helper;
    Extracter data_extracter;

    EdgeEmplacer(const EdgeEmplacer& other) = default;
    EdgeEmplacer(EdgeEmplacer&& other) = default;

    // passing both Helper and Extracter
    template<EmplacementHelperType EH, class... Args>
    EdgeEmplacer(EH&& _helper, Args&&... args): helper(std::forward<EH>(_helper)), data_extracter(std::forward<Args>(args)...) {}
    template<DataExtracterType DET, class... Args>
    EdgeEmplacer(DET&& _data_extracter, Args&&... args): helper(std::forward<Args>(args)...), data_extracter(std::forward<DET>(_data_extracter)) {}

    // piecewise constructing Helper and Extracter
    template<class HelperInit, class ExtracterInit>
    EdgeEmplacer(const std::piecewise_construct_t, HelperInit&& _helper, ExtracterInit&& _extracter):
      helper(std::make_from_tuple<Helper>(std::forward<HelperInit>(_helper))),
      data_extracter(std::make_from_tuple<Extracter>(std::forward<ExtracterInit>(_extracter))) {}

    // constructing Helper and Extracter
    template<PhylogenyType Phylo, MapsToNode OldToNew, class... Args>
      requires std::is_constructible_v<Helper, Phylo&&, OldToNew&&>
    EdgeEmplacer(Phylo&& N, OldToNew&& old_to_new, Args&&... extracter_args):
      helper(std::forward<Phylo>(N), std::forward<OldToNew>(old_to_new)), data_extracter(std::forward<Args>(extracter_args)...) {}
    template<PhylogenyType Phylo, class First, class... Args>
      requires (std::is_constructible_v<Helper, Phylo&&> && !MapsToNode<First>)
    EdgeEmplacer(Phylo&& N, First&& first, Args&&... extracter_args):
      helper(std::forward<Phylo>(N)), data_extracter(std::forward<First>(first), std::forward<Args>(extracter_args)...) {}
    template<PhylogenyType Phylo>
      requires (std::is_constructible_v<Helper, Phylo&&> && std::is_default_constructible_v<Extracter>)
    EdgeEmplacer(Phylo&& N):
      helper(std::forward<Phylo>(N)), data_extracter() {}

    // copy and move construction
    template<PhylogenyType Phylo>
      requires std::is_constructible_v<Helper, const Helper&, Phylo&&>
    EdgeEmplacer(const EdgeEmplacer& other, Phylo&& N):
      helper(other.helper, std::forward<Phylo>(N)), data_extracter(other.data_extracter) {}
    template<PhylogenyType Phylo>
      requires std::is_constructible_v<Helper, Helper&&, Phylo&&>
    EdgeEmplacer(EdgeEmplacer&& other, Phylo&& N):
      helper(std::move(other).helper, std::forward<Phylo>(N)), data_extracter(std::move(other).data_extracter) {}


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
      DEBUG4(std::cout << "extracting node data? "<<extract_node_data<<'\n');
      if constexpr (extract_node_data && (std::is_invocable_v<Extracter, Ex_node_data, Args&&...>)) {
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
      DEBUG5(std::cout << "\ncreating a copy of "<<other_u<<" in translation @"<<&(helper.old_to_new())<<'\n');
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
    NodeDesc emplace_edge_raw(const NodeDesc u, const NodeDesc v, MoreArgs&&... args) {
      DEBUG5(std::cout << "only adding edge "<< u <<" ----> "<< v <<"\n");
      // if the data-extracter can be called with MoreArgs, then use it to make data, otherwise, just pass MoreArgs to the edge creation
      if constexpr (extract_edge_data && (std::is_invocable_v<Extracter, Ex_edge_data, MoreArgs&&...>)) {
        helper.add_an_edge(u, v, data_extracter(Ex_edge_data{}, std::forward<MoreArgs>(args)...));
      } else helper.add_an_edge(u, v, std::forward<MoreArgs>(args)...);
      return v;
    }

    template<class... MoreArgs>
    NodeDesc emplace_edge(const auto& other_u, const auto& other_v, MoreArgs&&... args) {
      DEBUG5(std::cout << "treating ("<<other_u<<" "<<other_v<<")\n");
      return emplace_edge_raw(create_copy_of(other_u), create_copy_of(other_v), std::forward<MoreArgs>(args)...);
    }

    template<class P, class Q, class... MoreArgs>
    NodeDesc emplace_edge(const std::pair<P,Q>& other_uv, MoreArgs&&... args) {
      return emplace_edge(other_uv.first, other_uv.second, std::forward<MoreArgs>(args)...);
    }

    template<EdgeType Edge, class... MoreArgs>
    NodeDesc emplace_edge(Edge&& uv, MoreArgs&&... args) {
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
    void mark_roots(const auto& source) {
      for(const NodeDesc r: source.roots()) mark_root(r);
    }
    // commit the root-candidates to N
    void commit_roots() { helper.commit_roots(); }
  };

  // -------- Edge Emplacer: deduction guides --------------
  // deduction guide for the emplacer
  template<EmplacementHelperType _Helper, DataExtracterType Extracter>
  EdgeEmplacer(_Helper&&, Extracter&&) -> EdgeEmplacer<std::remove_cvref_t<_Helper>, std::remove_cvref_t<Extracter>>;

  // -------- Edge Emplacer: concepts --------------
  template<class T> concept StrictEdgeEmplacerType = EmplacementHelperType<typename T::Helper>;
  template<class T> concept EdgeEmplacerType = StrictEdgeEmplacerType<std::remove_cvref_t<T>>;

  // -------- Edge Emplacer: defaults  --------------
  template<StrictPhylogenyType TargetPhylo, bool track_roots = true, OptionalPhylogenyType SourcePhylo = void, OptionalMapsToNode OldToNew = NodeTranslation>
  using DefaultEdgeEmplacer = EdgeEmplacer<EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, OldToNew>, DefaultDataExtracter<SourcePhylo>>;

  // this saves you from writing 'EdgeEmplacementHelper' if you want to specify the Emplacer directly
  template<bool _track_roots,
           StrictPhylogenyType _TargetPhylo,
           OptionalPhylogenyType _SourcePhylo = void,
           OptionalMapsToNode OldToNewTranslation = NodeTranslation,
           DataExtracterType Extracter = DataExtracter<_SourcePhylo>>
  using EdgeEmplacerWithHelper = EdgeEmplacer<EdgeEmplacementHelper<_track_roots, _TargetPhylo, _SourcePhylo, OldToNewTranslation>, Extracter>;


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
      using Helper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, OldToNewTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo>(std::forward<Args>(args)...));
      return EdgeEmplacer<Helper, Extracter>(
          Helper(N, std::forward<OldToNewTranslation>(old_to_new)),
          make_data_extracter<SourcePhylo>(std::forward<Args>(args)...));
    }

    // allow giving a custom emplacement helper
    // NOTE: we will overwrite the Network* inside the helper with a pointer to the correct network
    template<StrictPhylogenyType TargetPhylo, EmplacementHelperType Helper, class... Args>
    static auto make_emplacer(TargetPhylo& N, Helper&& helper, Args&&... args) {
      using Extracter = decltype(make_data_extracter<SourcePhylo>(std::forward<Args>(args)...));
      helper.N = &N; // set the network of the helper
      return EdgeEmplacer<Helper, Extracter>(
          std::forward<Helper>(helper),
          make_data_extracter<SourcePhylo>(std::forward<Args>(args)...));
    }

    template<StrictPhylogenyType TargetPhylo, class T, class... Args> requires (!MapsToNode<T> && !EmplacementHelperType<T>)
    static auto make_emplacer(TargetPhylo& N, T&& t, Args&&... args) {
      using Helper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, NodeTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo>(std::forward<T>(t), std::forward<Args>(args)...));
      return EdgeEmplacer<Helper, Extracter>(
          Helper{N},
          make_data_extracter<SourcePhylo>(std::forward<T>(t), std::forward<Args>(args)...));
    }
    template<StrictPhylogenyType TargetPhylo>
    static auto make_emplacer(TargetPhylo& N) {
      using Helper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, NodeTranslation>;
      //using Extracter = decltype(make_data_extracter<SourcePhylo>());
      return EdgeEmplacer(Helper{N}, make_data_extracter<SourcePhylo>());
    }
  };


}

