
#pragma once

#include "types.hpp"

namespace PT {
  // ========== EdgeEmplacement ==========

  // NOTE: if track_roots is false, the user is resposable to mark the root(s) in the new network
  template<bool track_roots, PhylogenyType Phylo>
  struct EdgeEmplacementHelper {
    Phylo& N;

    template<class... Args>
    void add_an_edge(const NodeDesc& u, const NodeDesc& v, Args&&... args) const { N.add_edge(u,v, std::forward<Args>(args)...); }

    // copy/move a node other_x and place it below x; if x == NoNode, the copy will be a new root
    // return the description of the copy of other_x
    template<class Data, class... Args>
    NodeDesc create_node_below(const NodeDesc& u, Data&& data, Args&&... args) {
      NodeDesc v;
      if(u == NoNode) {
        if constexpr (std::remove_reference_t<Phylo>::has_node_data)
          v = N.create_node(std::forward<Data>(data));
        else v = N.create_node();
        N.count_node();
      } else {
        if constexpr (std::remove_reference_t<Phylo>::has_node_data)
          v = N.create_node(std::forward<Data>(data));
        else v = N.create_node();
        const bool success = N.add_child(u, v, std::forward<Args>(args)...).second;
        assert(success);
      }
      return v;
    }

    bool mark_root(const NodeDesc& r) {
      assert(N.in_degree(r) == 0);
      return append(N._roots, r).second;
    }

    static void clear() {}
  };

  template<PhylogenyType Phylo>
  struct EdgeEmplacementHelper<true, Phylo> {
    Phylo& N;
    NodeSet root_candidates = {};
    
    // when tracking roots, we want to remove v from the root-candidate set
    template<class... Args>
    void add_an_edge(const NodeDesc& u, const NodeDesc& v, Args&&... args) {
      erase(root_candidates, v);
      N.add_edge(u,v,std::forward<Args>(args)...);
    }

    // copy/move a node other_x and place it below x; if x == NoNode, the copy will be a new root
    // return the description of the copy of other_x
    template<class Data, class... Args>
    NodeDesc create_node_below(const NodeDesc& u, Data&& data, Args&&... args) {
      NodeDesc v;
      if(u == NoNode) {
        if constexpr (std::remove_reference_t<Phylo>::has_node_data)
          v = N.create_node(std::forward<Data>(data));
        else v = N.create_node();
        N.count_node();
        append(root_candidates, v);
      } else {
        if constexpr (std::remove_reference_t<Phylo>::has_node_data)
          v = N.create_node(std::forward<Data>(data));
        else v = N.create_node();
        const bool success = N.add_child(u, v, std::forward<Args>(args)...).second;
        assert(success);
        erase(root_candidates, v);
      }
      return v;
    }
    
    void commit_roots() {
      for(const NodeDesc& r: root_candidates)
        if(N.in_degree(r) == 0)
          append(N._roots, r);
    }

    void clear() { root_candidates.clear(); }
  };

  template<bool track_roots,
           PhylogenyType Phylo,
           NodeTranslationType OldToNewTranslation = NodeTranslation,
           class NodeDataExtract = typename Phylo::IgnoreNodeDataFunc> // takes a Node (possible rvalue-ref) and returns (steals) its data
  struct EdgeEmplacer {
    EdgeEmplacementHelper<track_roots, Phylo> helper;
    OldToNewTranslation old_to_new;
    NodeDataExtract nde;

    template<class... MoreArgs> // more args to be passed to create_node_below() when dangling v from u
    void emplace_edge(const NodeDesc& other_u, const NodeDesc& other_v, MoreArgs&&... args) {
      DEBUG5(std::cout << "  treating edge "<<other_u<<" --> "<<other_v<<"\n");
      // check if other_u is known to the translation
      const auto [u_iter, u_success] = old_to_new.try_emplace(other_u);
      NodeDesc& u_copy = u_iter->second;
      // if other_u has not been seen before, insert it as new root
      if(u_success) u_copy = helper.create_node_below(NoNode, nde(other_u), std::forward<MoreArgs>(args)...);
      // ckeck if we've already seen other_v before
      const auto [v_iter, success] = old_to_new.try_emplace(other_v);
      NodeDesc& v_copy = v_iter->second;
      if(!success) { // if other_v is already in the translate map, then add a new edge
        DEBUG5(std::cout << "only adding edge "<<u_copy<<" --> "<<v_copy<<"\n");
        helper.add_an_edge(u_copy, v_copy, std::forward<MoreArgs>(args)...);
      } else { // if other_v is not in the translate map yet, then add it
        DEBUG5(std::cout << "adding new child to "<<u_copy<<"\n");
        v_copy = helper.create_node_below(u_copy, nde(other_v), std::forward<MoreArgs>(args)...);
      }
    }

    template<class... MoreArgs>
    void emplace_edge(const NodePair& other_uv, MoreArgs&&... args) { emplace_edge(other_uv.first, other_uv.second, std::forward<MoreArgs>(args)...); }

    bool mark_root(const NodeDesc& r) {
      return helper.mark_root(old_to_new.at(r));
    }
    // translate the roots of N to use as our roots
    template<PhylogenyType _Phylo>
    void mark_root(const _Phylo& Net) {
      for(const NodeDesc& r: Net.roots()) mark_root(r);
    }
    // commit the root-candidates to N
    void commit_roots() { helper.commit_roots(); }

    template<class... Args>
    void finalize(Args&&... args) {
      if constexpr (track_roots)
        commit_roots();
      else
        mark_root(std::forward<Args>(args)...);
    }

    void clear() { helper.clear(); old_to_new.clear(); }
  };

  // the rest of this file are convenience functions to not have to write sooo much all the time

  // this allows you to just write "auto emp = EdgeEmplacers<track_roots>::make_emplacer(...)" without specifying template parameters
  template<bool track_roots>
  struct EdgeEmplacers {
    template<PhylogenyType Phylo, NodeTranslationType OldToNewTranslation, class NodeDataExtract>
    static auto make_emplacer(Phylo& N, OldToNewTranslation& old_to_new, NodeDataExtract&& make_node_data) {
      return EdgeEmplacer<track_roots, Phylo, OldToNewTranslation&, NodeDataExtract>{{N}, old_to_new, std::forward<NodeDataExtract>(make_node_data)};
    }
    template<PhylogenyType Phylo, class NodeDataExtract>
    static auto make_emplacer(Phylo& N, NodeDataExtract&& make_node_data) {
      return EdgeEmplacer<track_roots, Phylo, NodeTranslation, NodeDataExtract>{{N}, NodeTranslation(), std::forward<NodeDataExtract>(make_node_data)};
    }

  };

}

