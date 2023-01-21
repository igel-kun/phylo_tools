
#pragma once

// the bridge- and biconnected-component finders are based on chain decompositions
// (see https://arxiv.org/pdf/1209.0700.pdf
// NOTE: we will assume that the input network is single-rooted in order to make things a bit more efficient
//       for example, this lets us assume that no node has 2 incoming bridges

#include "linear_interval.hpp"
#include "types.hpp"
#include "set_interface.hpp"
#include "traversal_traits.hpp"

namespace PT{

  enum class CutObject { cut_node, bridge, bcc};

  struct ChainInfo {
    // each node knows...
    // ... its DFS-parent...
    NodeDesc DFS_parent = NoNode;
    // ... its DFS-number...
    size_t DFS_number = 0;
    // ... whether it is visited...
    bool visited = false;
    // ... if it's the start of said chain...
    bool start_of_cyclic_chain = false;
    // ... is incident with a bridge ...
    bool incident_with_bridge = false;
    // ... if the node has an incoming bridge (if the network has a unique root, such a bridge is unique)
    bool has_incoming_bridge = false;

    friend std::ostream& operator<<(std::ostream& os, const ChainInfo& info) {
      return os << "(DFS#: "<<info.DFS_number
                << "\tcyc-chain-start: "<< info.start_of_cyclic_chain
                << "\thas bridge: "<< info.incident_with_bridge
                << "\tvisited: "<< info.visited 
                << "\tDFS-parent: "<< info.DFS_parent
                << ")\n";
    }

    bool is_leaf_or_cut_node() const { return start_of_cyclic_chain || incident_with_bridge; }
    bool is_leaf_or_cut_node_or_root() const { return is_leaf_or_cut_node() || (DFS_parent == NoNode); }
  };

  // in order to tell biconnected components apart, we'll also store the root r of each chain (except in r itself)
  // (for any cut node u, we will enumerate the BCCs with root u before the at most one BCC containing u in which u is not the root)
  struct ChainInfoBCC: public ChainInfo {
    bool DFS_parent_is_chain_root = false;
    
    bool is_first_edge_in_nontrivial_bcc_from(const NodeDesc u) const {
      return DFS_parent_is_chain_root && (ChainInfo::DFS_parent == u);
    }
    bool is_first_edge_in_bcc_from(const NodeDesc u) const {
      return (ChainInfo::DFS_parent == u) && (DFS_parent_is_chain_root || ChainInfo::has_incoming_bridge);
    }

  };

  // the main data structure is the chain decomposition
  // it can answer whether a node is a cut node and whether an arc is a bridge
  // if output_root == true, then it will pretend that the root is a cut node
  template<StrictPhylogenyType Network, CutObject cut_object, bool output_root = (cut_object == CutObject::bcc)>
  struct ChainDecomposition {
    using InfoStruct = std::conditional_t<cut_object == CutObject::bcc, ChainInfoBCC, ChainInfo>;
    NodeMap<InfoStruct> chain_info;

    // register the fact that the edge to the DFS-parent is a bridge
    void mark_edge_to_DFS_parent_as_bridge(ChainInfo& u_info) {
      auto& p_info = chain_info.at(u_info.DFS_parent);
      u_info.incident_with_bridge = true;
      p_info.incident_with_bridge = true;
      // NOTE: if the edge to u's DFS-parent p is indeed a bridge, then we know that it is oriented from p to u
      u_info.has_incoming_bridge = true;
    }

    NodeDesc treat_forwardedge(const NodeDesc u, NodeDesc v) {
      InfoStruct* v_info;
      assert(u != v);
      while(v != NoNode) {
        v_info = &(chain_info.at(v));
        if(!v_info->visited) {
          v_info->visited = true;
          v = v_info->DFS_parent;
          if constexpr (cut_object == CutObject::bcc) {
            if(u == v) {
              // NOTE: we're using the v_info of the previous v here since it has not been updated yet
              assert(v_info->DFS_parent == u);
              std::cout << "marking that "<<u<<" is a chain-root\n";
              v_info->DFS_parent_is_chain_root = true;
              return v;
            }
          }
        } else return v;
      }
      return v;
    }

    // treat a newly encountered edge uv (directed either u-->v or v-->u in the network)
    // return whether uv is a backedge
    bool treat_edge(const NodeDesc u, const NodeDesc v, ChainInfo& u_info) {
      if(v != u_info.DFS_parent) {
        std::cout << "treating edge "<<u<<" -> "<<v<<"\n";
        auto& v_info = chain_info.at(v);
        if(u != v_info.DFS_parent) {
          // if none of u and v is the DFS-parent of the other, then uv is a backedge or a forwardedge
          if(u_info.DFS_number < v_info.DFS_number) {
            u_info.visited = true;
            const NodeDesc chain_end = treat_forwardedge(u, v);
            // if the chain is a cycle, then mark u as the start of a cyclic chain
            if(chain_end == u) u_info.start_of_cyclic_chain = true;
          } else return true;
        }
      }
      return false;
    }

    void analyse_network(const NodeVec& dfs_nodes) {
      for(const NodeDesc u: dfs_nodes) {
        auto& u_info = chain_info.at(u);
        const bool u_was_visited = u_info.visited;
        bool u_has_backedge = false;
        for(const NodeDesc v: Network::children(u)) u_has_backedge |= treat_edge(u, v, u_info);
        for(const NodeDesc v: Network::parents(u)) u_has_backedge |= treat_edge(u, v, u_info);
        // if u was unvisited before, then there is no backedge spanning over it
        // further, if u has no backedges incident with it, then the edge between u and its DFS-parent is a bridge
        if(!u_was_visited && !u_has_backedge && (u_info.DFS_parent != NoNode)) {
          std::cout << "marking "<<u<<" & "<<u_info.DFS_parent<<" as incident with bridge\n";
          mark_edge_to_DFS_parent_as_bridge(u_info);
        } else std::cout << "not marking "<<u<<" incident with bridge; reasons: "<< u_was_visited << u_has_backedge << (u_info.DFS_parent == NoNode)<<"\n";
      }
    }
    
    void construct_DFS_tree(const NodeDesc u, const NodeDesc parent, NodeVec& dfs_nodes, bool parent_edge_incoming = true) {
      const auto [iter, success] = append(chain_info, u);
      auto& u_info = iter->second;
      if(success) {
        u_info.DFS_parent = parent;
        u_info.DFS_number = dfs_nodes.size();
        dfs_nodes.push_back(u);

        for(const NodeDesc v: Network::children(u)) construct_DFS_tree(v, u, dfs_nodes, true);
        for(const NodeDesc v: Network::parents(u)) construct_DFS_tree(v, u, dfs_nodes, false);
      }
    }

    void analyse_network(NodeVec& dfs_nodes, const NodeDesc u) {
      construct_DFS_tree(u, NoNode, dfs_nodes);
      analyse_network(dfs_nodes);
      std::cout << "analysis complete, chain info is:\n"<<chain_info<<"\n";
    }

    //ChainDecomposition() = default;
    ChainDecomposition() = delete;
    ChainDecomposition(const Network& N):
      ChainDecomposition(N.root(), N.num_nodes()) {}

    ChainDecomposition(const NodeDesc u, const size_t num_nodes = 0) {
      DEBUG4(std::cout << "making chain decomposition for network with "<<num_nodes<<" nodes\n");
      NodeVec dfs_nodes;
      dfs_nodes.reserve(num_nodes);
      analyse_network(dfs_nodes, u);
    }


    // return whether u is a cut-node
    bool is_cut_node(const NodeDesc x) const {
      assert(!chain_info.empty());
      assert(chain_info.contains(x));
      if(!Network::is_leaf(x)){
        if constexpr (output_root)
          return chain_info.at(x).is_leaf_or_cut_node_or_root();
        else return chain_info.at(x).is_leaf_or_cut_node();
      } else return false;
    }
    bool operator()(const NodeDesc x) const {
      std::cout << "checking if "<<x<<" is a cut-node... "<<is_cut_node(x)<<"\n";
      return is_cut_node(x);
    }
    // return whether xy is a bridge
    bool is_bridge(const NodeDesc x, const NodeDesc y) const {
      static_assert(Network::has_unique_root);
      return chain_info.at(y).has_incoming_bridge;
    }
    bool operator()(const NodeDesc x, const NodeDesc y) const { return is_bridge(x, y); }
    template<EdgeType E>
    bool operator()(const E& e) const { return is_bridge(e.tail(), e.head()); }

    // for a child v of u, return whether v is in a biconnected component rooted at u
    // NOTE: this only works if we're storing information for BCCs
    bool is_first_edge_in_bcc(const NodeDesc u, const NodeDesc v) const {
      static_assert(cut_object == CutObject::bcc);
      return chain_info.at(v).is_first_edge_in_bcc_from(u);
    }
    bool is_first_edge_in_nontrivial_bcc(const NodeDesc u, const NodeDesc v) const {
      static_assert(cut_object == CutObject::bcc);
      return chain_info.at(v).is_first_edge_in_nontrivial_bcc_from(u);
    }

  };

  template<CutObject cut_object>
  static constexpr TraversalType default_tt_for_cut_object = (cut_object == CutObject::bridge) ? all_edge_tail_postorder : postorder;

  template<class Iter, class ChainDecomp = std::remove_cvref_t<typename Iter::Predicate>>
  struct WithChains {
    ChainDecomp chains;

    decltype(auto) operator()(const auto& it) const {
      std::cout << "making new iter of type\n"<<mstd::type_name<Iter>()<<"\nfrom iter of type\n"<<mstd::type_name<decltype(it)>()<<"\n";
      if constexpr (std::is_same_v<std::remove_cvref_t<decltype(it)>, mstd::GenericEndIterator>)
        return it;
      else return Iter{it, chains};
    }
  };

  // NOTE: we'll store the ChainDecomposition in the IteratorFactory, so if the factory runs out of scope, don't access the iterator anymore!
  template<StrictPhylogenyType Network,
           TraversalType tt,
           class ChainDecomp,
           template<StrictPhylogenyType, TraversalType, class> class _CutIt>
  struct _CutIterFactory: public mstd::IterFactoryWithBeginEnd<
                            typename _CutIt<Network, tt, ChainDecomp>::Iterator,
                            WithChains<_CutIt<Network, tt, ChainDecomp>>
                          >
  {
    using Parent = mstd::IterFactoryWithBeginEnd<typename _CutIt<Network, tt, ChainDecomp>::Iterator, WithChains<_CutIt<Network, tt, ChainDecomp>>>;
    _CutIterFactory(const Network& N): // construct the DFSIterator and the WithChains object with the network N
      Parent(std::piecewise_construct, std::forward_as_tuple(N), std::forward_as_tuple(N))
    {
      std::cout << "mark4\n";
    }
    _CutIterFactory(const Network& N, const NodeDesc u): // construct the DFSIterator with the given node u & the WithChains object with the network N
      Parent(std::piecewise_construct, std::forward_as_tuple(u), std::forward_as_tuple(N))
    {}
  };
 

  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::cut_node>,
           class ChainDecomp = ChainDecomposition<Network, CutObject::cut_node>>
  using CutNodeIter = mstd::filtered_iterator<typename NodeTraversal<tt, Network, NodeDesc>::OwningIter, ChainDecomp>;
  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::cut_node>,
           class ChainDecomp = ChainDecomposition<Network, CutObject::cut_node>>
  using CutNodeIterFactory = _CutIterFactory<Network, tt, ChainDecomp, CutNodeIter>;

  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::bridge>,
           class ChainDecomp = ChainDecomposition<Network, CutObject::bridge>>
  using BridgeIter = mstd::filtered_iterator<typename AllEdgesTraversal<tt, Network, NodeDesc>::OwningIter, ChainDecomp>;
  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::bridge>,
           class ChainDecomp = ChainDecomposition<Network, CutObject::bridge>>
  using BridgeIterFactory = _CutIterFactory<Network, tt, ChainDecomp, BridgeIter>;

  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::bcc>,
           class ChainDecomp = ChainDecomposition<Network, CutObject::bcc>>
  using BCCCutIter = mstd::filtered_iterator<typename NodeTraversal<tt, Network, NodeDesc>::OwningIter, ChainDecomp>;
  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::bcc>,
           class ChainDecomp = ChainDecomposition<Network, CutObject::bcc>>
  using BCCCutIterFactory = _CutIterFactory<Network, tt, ChainDecomp, BCCCutIter>;

  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::cut_node>,
           class... Args>
  auto get_cut_nodes(const Network& N, Args&&... args) { return CutNodeIterFactory<Network, tt>(N, std::forward<Args>(args)...); }

  template<StrictPhylogenyType Network,
           TraversalType tt = default_tt_for_cut_object<CutObject::bridge>,
           class... Args>
  auto get_bridges(const Network& N, Args&&... args) { return BridgeIterFactory<Network, tt>(N, std::forward<Args>(args)...); }

}// namespace
