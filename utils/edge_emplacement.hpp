
#pragma once

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
           NodeTranslationType _OldToNewTranslation = NodeTranslation>
  struct EdgeEmplacementHelper: mstd::optional_tuple<std::conditional_t<_track_roots, NodeSet, void>> {
    using SourcePhylo = _SourcePhylo;
    using TargetPhylo = _TargetPhylo;
    using OldToNewTranslation = _OldToNewTranslation;
    using StrictTranslation = std::remove_reference_t<OldToNewTranslation>;
    static constexpr bool track_roots = _track_roots;

    TargetPhylo& N;
    OldToNewTranslation old_to_new;

    EdgeEmplacementHelper(TargetPhylo& _N): N(_N) {}
    EdgeEmplacementHelper(TargetPhylo& _N, const StrictTranslation& _old_to_new): N(_N), old_to_new(_old_to_new) {}
    EdgeEmplacementHelper(TargetPhylo& _N, StrictTranslation& _old_to_new): N(_N), old_to_new(_old_to_new) {}
    EdgeEmplacementHelper(TargetPhylo& _N, StrictTranslation&& _old_to_new): N(_N), old_to_new(std::move(_old_to_new)) {}
    EdgeEmplacementHelper(const EdgeEmplacementHelper&) = default;
    EdgeEmplacementHelper(EdgeEmplacementHelper&&) = default;

  protected:
    inline NodeSet& root_candidates() {
      static_assert(track_roots);
      return this->template get<0>();
    }
  public:

    template<class... Args> requires (track_roots)
    void add_an_edge(const NodeDesc u, const NodeDesc v, Args&&... args) {
      mstd::erase(root_candidates(), v);
      N.add_edge(u,v, std::forward<Args>(args)...);
    }
 
    template<class... Args> requires (!track_roots)
    void add_an_edge(const NodeDesc u, const NodeDesc v, Args&&... args) const {
      N.add_edge(u,v, std::forward<Args>(args)...);
    }
   
    // copy/move a node other_x and place it below x; if x == NoNode, the copy will be a new root
    // return the description of the copy of other_x
    template<class Data, class... Args>
    NodeDesc create_node_below_with_data(const NodeDesc u, Data&& data, Args&&... args) {
      if(u != NoNode) {
        NodeDesc v;
        DEBUG4(std::cout << "creating node with data "<<data<<"\n");
        if constexpr (_TargetPhylo::has_node_data)
          v = N.create_node(std::forward<Data>(data));
        else v = N.create_node();
        const bool success = N.add_child(u, v, std::forward<Args>(args)...).second;
        assert(success);
        if constexpr (track_roots) mstd::erase(root_candidates(), v);
        return v;
      } else return create_root(std::forward<Data>(data));
    }

    template<class... Args>
    NodeDesc create_node_below_no_data(const NodeDesc u, Args&&... args) {
      if(u != NoNode) {
        const NodeDesc v = N.create_node();
        const bool success = N.add_child(u, v, std::forward<Args>(args)...).second;
        assert(success);
        if constexpr (track_roots) mstd::erase(root_candidates(), v);
        return v;
      } else return create_root();
    }

    template<class... Args>
    NodeDesc create_root(Args&&... args) {
      N.count_node();
      const NodeDesc v = N.create_node(std::forward<Args>(args)...);
      if constexpr (track_roots) mstd::append(root_candidates(), v);
      return v;
    }

    bool mark_root(const NodeDesc r) {
      assert(N.in_degree(r) == 0);
      assert(old_to_new.contains(r));
      return mstd::append(N._roots, old_to_new.at(r)).second;
    }
    bool mark_root_directly(const NodeDesc r) {
      return mstd::append(N._roots, r).second;
    }

    void clear() {
      if constexpr (track_roots) root_candidates().clear();
      old_to_new.clear();
    }

    // NOTE: this is not private, so the user can commit roots at any point
    //       However, be aware that roots will also be committed upon destruction!
    // NOTE: only call this if you're tracking roots
    void commit_roots() {
      if constexpr (track_roots) {
        DEBUG3(std::cout << "committing roots: "<<root_candidates()<<"\n");
        for(const NodeDesc r: root_candidates())
          if(N.in_degree(r) == 0)
            mstd::append(N._roots, r);
        root_candidates().clear();
      }
    }

    ~EdgeEmplacementHelper() {
      if constexpr (track_roots) commit_roots();
    }

  };

  // T is an EmplacementHelper iff T::add_an_edge can be invoked with 2 NodeDesc's
  template<class T> concept StrictEmplacementHelperType = requires(T& t, NodeDesc u) { t.add_an_edge(u,u); };
  template<class T> concept EmplacementHelperType = StrictEmplacementHelperType<std::remove_cvref_t<T>>;


  template<StrictEmplacementHelperType _Helper, DataExtracterType Extracter>
  struct EdgeEmplacer {
    using Helper = _Helper;
    using StrictExtracter = std::remove_reference_t<Extracter>;
    using SourcePhylo = typename Helper::SourcePhylo;
    using TargetPhylo = typename Helper::TargetPhylo;
    static constexpr bool track_roots = Helper::track_roots;
    static constexpr bool extract_labels = !StrictExtracter::ignoring_node_labels;
    static constexpr bool extract_node_data = !StrictExtracter::ignoring_node_data;
    static constexpr bool extract_edge_data = !StrictExtracter::ignoring_edge_data;

    Helper helper;
    Extracter data_extracter;

    EdgeEmplacer(const Helper& _helper, const StrictExtracter& _data_extracter): helper(_helper), data_extracter(_data_extracter) {}
    EdgeEmplacer(const Helper& _helper, StrictExtracter& _data_extracter): helper(_helper), data_extracter(_data_extracter) {}
    EdgeEmplacer(const Helper& _helper, StrictExtracter&& _data_extracter): helper(_helper), data_extracter(std::move(_data_extracter)) {}
    EdgeEmplacer(Helper&& _helper, const StrictExtracter& _data_extracter): helper(std::move(_helper)), data_extracter(_data_extracter) {}
    EdgeEmplacer(Helper&& _helper, StrictExtracter& _data_extracter): helper(std::move(_helper)), data_extracter(_data_extracter) {}
    EdgeEmplacer(Helper&& _helper, StrictExtracter&& _data_extracter): helper(std::move(_helper)), data_extracter(std::move(_data_extracter)) {}

    template<class... Args>
    EdgeEmplacer(const StrictExtracter& _data_extracter, Args&&... helper_args):
      helper(std::forward<Args>(helper_args)...), data_extracter(_data_extracter) {}
    template<class... Args>
    EdgeEmplacer(StrictExtracter& _data_extracter, Args&&... helper_args):
      helper(std::forward<Args>(helper_args)...), data_extracter(_data_extracter) {}
    template<class... Args>
    EdgeEmplacer(StrictExtracter&& _data_extracter, Args&&... helper_args):
      helper(std::forward<Args>(helper_args)...), data_extracter(std::move(_data_extracter)) {}

    template<class HelperInit, class ExtracterInit>
    EdgeEmplacer(const std::piecewise_construct_t, HelperInit&& _helper, ExtracterInit&& _extracter):
      helper(std::make_from_tuple<Helper>(std::forward<HelperInit>(_helper))),
      data_extracter(std::make_from_tuple<Extracter>(std::forward<ExtracterInit>(_extracter))) {}



    NodeDesc create_copy_of(const NodeDesc other_u) {
      DEBUG5(std::cout << "\ncreating a copy of "<<other_u<<" in translation @"<<&(helper.old_to_new)<<'\n');
      // check if other_u is known to the translation
      const auto [u_iter, u_success] = helper.old_to_new.try_emplace(other_u);
      NodeDesc& u_copy = u_iter->second;
      // if other_u has not been seen before, insert it as new root
      if(u_success) {
        DEBUG4(std::cout << "extracting node data? "<<extract_node_data<<'\n');
        if constexpr (extract_node_data) {
          u_copy = helper.create_root(data_extracter(Ex_node_data{}, other_u));
          DEBUG4(std::cout << "data of "<<u_copy<<" is now "<<helper.N[u_copy].data() << "\n");
        } else u_copy = helper.create_root();
        DEBUG4(std::cout << "created copy " << u_copy << " of "<< other_u<<'\n');
        if constexpr (extract_labels) {
          const auto& other_u_label = data_extracter(Ex_node_label{}, other_u);
          auto& u_copy_label = node_of<TargetPhylo>(u_copy).label();
          DEBUG4(std::cout << "copying extracted label "<<other_u_label<<" to "<<u_copy<<"\n");
          u_copy_label = other_u_label;
          DEBUG4(std::cout << "set label of node "<<u_copy<<" to '" << u_copy_label <<"' (@"<<&u_copy_label<<")\n");
        }
      }
      return u_copy;
    }

    template<class... MoreArgs> // more args to be passed to create_node_below() when dangling v from u
    NodeDesc emplace_edge(const NodeDesc other_u, const NodeDesc other_v, MoreArgs&&... args) {
      DEBUG5(std::cout << "  treating edge "<<other_u<<" --> "<<other_v<<"\n");
      const NodeDesc u_copy = create_copy_of(other_u);
      // ckeck if we've already seen other_v before
      const auto [v_iter, success] = helper.old_to_new.try_emplace(other_v);
      NodeDesc& v_copy = v_iter->second;
      if(!success) { // if other_v is already in the translate map, then add a new edge
        DEBUG5(std::cout << "only adding edge "<<u_copy<<" --> "<<v_copy<<"\n");
        helper.add_an_edge(u_copy, v_copy, std::forward<MoreArgs>(args)...);
      } else { // if other_v is not in the translate map yet, then add it
        DEBUG5(std::cout << "adding new child to "<<u_copy<<"\n");
        if constexpr (extract_node_data)
          v_copy = helper.create_node_below_with_data(u_copy, data_extracter(Ex_node_data{}, other_v), std::forward<MoreArgs>(args)...);
        else
          v_copy = helper.create_node_below_no_data(u_copy, std::forward<MoreArgs>(args)...);
        if constexpr (extract_labels) node_of<TargetPhylo>(v_copy).label() = data_extracter(Ex_node_label{}, other_v);
      }
      return v_copy;
    }

    template<class... MoreArgs>
    NodeDesc emplace_edge(const NodePair& other_uv, MoreArgs&&... args) {
      return emplace_edge(other_uv.first, other_uv.second, std::forward<MoreArgs>(args)...);
    }

    template<EdgeType Edge, class... MoreArgs>
    NodeDesc emplace_edge(Edge&& uv, MoreArgs&&... args) {
      if constexpr (extract_edge_data)
        return emplace_edge(uv.as_pair(), std::forward<MoreArgs>(args)..., data_extracter(std::forward<Edge>(uv)));
      else
        return emplace_edge(uv.as_pair(), std::forward<MoreArgs>(args)...);
    }

    bool mark_root(const NodeDesc r) { return helper.mark_root(r); }
    bool mark_root_directly(const NodeDesc r) { return helper.mark_root_directly(r); }

    // translate the roots of N to use as our roots
    void mark_roots(const auto& source) {
      for(const NodeDesc r: source.roots()) mark_root(r);
    }
    // commit the root-candidates to N
    void commit_roots() { helper.commit_roots(); }

    template<class... Args>
    void finalize(Args&&... args) {
      if constexpr (track_roots)
        commit_roots();
      else if constexpr (!std::is_void_v<std::remove_cvref_t<SourcePhylo>>)
        mark_roots(std::forward<Args>(args)...);
    }

    NodeDesc at(const NodeDesc u) const { assert(contains(u)); return helper.old_to_new.at(u); }
    bool contains(const NodeDesc u) const { return helper.old_to_new.contains(u); }
    NodeDesc lookup(const NodeDesc u, const NodeDesc _default = NoNode) const {
      return_map_lookup(helper.old_to_new, u, _default);
    }

    void clear() { helper.clear(); }
  };


  // this saves you from writing 'EdgeEmplacementHelper' if you want to specify the Emplacer directly
  template<bool _track_roots,
           StrictPhylogenyType _TargetPhylo,
           OptionalPhylogenyType _SourcePhylo = void,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           DataExtracterType Extracter = DataExtracter<_SourcePhylo, _TargetPhylo>>
  using EdgeEmplacerWithHelper = EdgeEmplacer<EdgeEmplacementHelper<_track_roots, _TargetPhylo, _SourcePhylo, OldToNewTranslation>, Extracter>;

  template<class T> concept StrictEdgeEmplacerType = EmplacementHelperType<typename T::Helper>;
  template<class T> concept EdgeEmplacerType = StrictEdgeEmplacerType<std::remove_cvref_t<T>>;


  // the rest of this file are convenience functions to not have to write sooo much all the time

  // this allows you to write "auto emp = EdgeEmplacers<track_roots>::make_emplacer(network, old_to_new, ...)" without specifying template parameters where
  // network = the network
  // old_to_new = a node translation
  // ... = data extracter functions as specified in <extract_data.hpp>
  template<bool track_roots, OptionalPhylogenyType SourcePhylo = void>
  struct EdgeEmplacers {
    template<StrictPhylogenyType TargetPhylo, NodeTranslationType OldToNewTranslation, class... Args>
    static auto make_emplacer(TargetPhylo& N, OldToNewTranslation&& old_to_new, Args&&... args) {
      // note that 'OldToNewTranslation' is an lvalue if old_to_new is an rvalue-ref and and lvalue-ref if old_to_new is an lvalue-ref...
      // thus, if an existing translation is passed, the helper will contain a reference to this translation, otherwise the helper has its own translation
      using Helper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, OldToNewTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo, TargetPhylo>(std::forward<Args>(args)...));
      return EdgeEmplacer<Helper, Extracter>(
          Helper(N, std::forward<OldToNewTranslation>(old_to_new)),
          make_data_extracter<SourcePhylo, TargetPhylo>(std::forward<Args>(args)...));
    }

    template<StrictPhylogenyType TargetPhylo, class T, class... Args> requires (!NodeTranslationType<T>)
    static auto make_emplacer(TargetPhylo& N, T&& t, Args&&... args) {
      using Helper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, NodeTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo, TargetPhylo>(std::forward<T>(t), std::forward<Args>(args)...));
      return EdgeEmplacer<Helper, Extracter>(
          Helper(N),
          make_data_extracter<SourcePhylo, TargetPhylo>(std::forward<T>(t), std::forward<Args>(args)...));
    }
    template<StrictPhylogenyType TargetPhylo>
    static auto make_emplacer(TargetPhylo& N) {
      using Helper = EdgeEmplacementHelper<track_roots, TargetPhylo, SourcePhylo, NodeTranslation>;
      using Extracter = decltype(make_data_extracter<SourcePhylo, TargetPhylo>());
      return EdgeEmplacer<Helper, Extracter>(
          Helper(N),
          Extracter());
    }
  };


}

