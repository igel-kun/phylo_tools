
// classes for handling tree extensions of networks

#pragma once

#include "utils/union_find.hpp"
#include "utils/set_interface.hpp"
#include "utils/extension.hpp"
#include "utils/network.hpp"

namespace PT{

  // construct an extension tree from a network & an extension
  // NOTE: make_node_data() will receive the description of the node in the network
  // NOTE: this will work also for partial extensions (covering only parts of V(N)), but your ExtTree may have to support multiple roots
  // NOTE: if the extension is over the whole network, then construct T without root-tracking, and just translate the last node of the extension
  template<StrictPhylogenyType Network, TreeType ExtTree, bool track_roots = true>
  class ExtToTree {

    template<NodeTranslationType NetToTree = NodeTranslation,
             DataExtracterType   Extracter = DataExtracter<Network>>
    static void _ext_to_tree(const Extension& ex, ExtTree& T,
                            NetToTree&& net_to_tree = NetToTree(),
                            Extracter&& data_extracter = Extracter())
    {
      auto emplacer = EdgeEmplacers<track_roots>::make_emplacer(T, net_to_tree, std::forward<Extracter>(data_extracter));
      // we can use a disjoint set forest with no-rank union to find the current highest node of the weakly-connected component of a node
      mstd::DisjointSetForest<NodeDesc> highest;

      DEBUG3(std::cout << "constructing extension tree from "<<ex<<std::endl);
      for(const auto& u: ex){
        // step 1: add a new set to the DSF with only u
        highest.add_new_set({u});
        DEBUG3(std::cout << "highest ancestors of node "<<u<<": "<<highest<<std::endl);
        // step 2: establish u as the parent in Gamma of all highest nodes in all weakly connected components (in G[ex[1..u]]) of its children in N
        NodeSet new_children; // NOTE: many children of u in the network may have the same "highest current ancestor" in the tree; use a set to avoid dupes
        for(const NodeDesc v: Network::children(u)){
          try{
            mstd::append(new_children, highest.set_of(v).get_representative());
          } catch(std::out_of_range& e) {
            throw(std::logic_error("trying to compute extension tree on a non-extension"));
          }
        }
        DEBUG3(std::cout << "new children of "<<u<<": "<<new_children<<"\n");
        // step 3: register u as the new hightest node in the weakly connected components of its new children 
        // step 4: add edges u->v to the edgelist
        for(const NodeDesc v: new_children){
          // NOTE: make sure the merge is not done by size but v is always plugged below u!
          highest.merge_sets_keep_order(u, v);
          emplacer.emplace_edge(u, v);
        }
      }
      emplacer.mark_root(ex.back());
    }

    // allow calling without a node translation
    template<class First, class... Args> requires (!NodeTranslationType<First>)
    static void _ext_to_tree(const Extension& ex, ExtTree& T, First&& first, Args&&... args) {
      _ext_to_tree(ex, T, NodeTranslation(), make_data_extracter<Network, ExtTree>(std::forward<First>(first), std::forward<Args>(args)...));
    }

  public:


    template<class... Args>
    static ExtTree ext_to_tree(const Extension& ex, Args&&... args) {
      ExtTree T; // yay RVO
      std::cout << "building ext-tree from "<<ex<<"\n";
      _ext_to_tree(ex,T, std::forward<Args>(args)...);
      assert(T.empty() == ex.empty());
      assert(T.edgeless() == (ex.size() <= 1));
      return T;
    }
  };


  // compute the scanwidth of all nodes in a given extension tree
  // NOTE: NetworkDegree is used to return the degrees (pair of indegree and outdegree) of a node of the tree (!!!) in the network
  //       to this end, the tree node has to be converted to a corresponding network node within the function
  //       this can be done by, for example, inverting the net_to_tree translation map filled by ext_to_tree()
  //       or by storing the network NodeDesc's inside the nodes themselves using the make_node_data function passed to ext_to_tree
  // NOTE: this can be used to construct the actual edge- or node- set corresponding to the scanwidth entry by passing a suitable function network_degrees
  template<class _Container = NodeMap<Degree>> requires (!std::is_const_v<_Container>)
  _Container ext_tree_sw_map(const TreeType auto& ext, auto&& network_degrees, _Container&& out = _Container()) {
    for(const NodeDesc u: ext.nodes_postorder()){
      auto [indeg, outdeg] = network_degrees(u);
      auto& sw_u = indeg;
      for(const NodeDesc v: ext.children(u))
        sw_u += out.at(v);
      sw_u -= outdeg;
      mstd::append(out, u, sw_u);
    }
    return std::forward<_Container>(out);
  }


  template<StrictPhylogenyType Network, mstd::MapType NodeDescToNodes>
    requires mstd::SetType<mstd::mapped_type_of_t<NodeDescToNodes>>
  NodeDescToNodes ext_tree_sw_nodes_map(const TreeType auto& ext, auto&& tree_node_to_network_node) {
    using Nodes = mstd::mapped_type_of_t<NodeDescToNodes>;
    using NodesAndNode = std::pair<Nodes, NodeDesc>;
    return ext_tree_sw_map<NodeDescToNodes>(ext, [&](const NodeDesc tree_u){
        const NodeDesc net_u = tree_node_to_network_node(tree_u);
        const auto& u_parents = Network::parents(net_u);
        return NodesAndNode(std::piecewise_construct, std::tuple{u_parents.begin(), u_parents.end()}, std::tuple{net_u});
      }
    );
  }

  template<StrictPhylogenyType Network, class NodeDescToEdges>
    requires (mstd::SetType<mstd::mapped_type_of_t<NodeDescToEdges>>)
  NodeDescToEdges ext_tree_sw_edges_map(const TreeType auto& ext, auto&& tree_node_to_network_node) {
    using Edges = mstd::mapped_type_of_t<NodeDescToEdges>;
    using EdgesPair = std::pair<Edges, Edges>;
    auto extract_edges = [&](const NodeDesc tree_u) {
        const NodeDesc net_u = tree_node_to_network_node(tree_u);
        const auto& u_in_edges = Network::in_edges(net_u);
        const auto& u_out_edges = Network::out_edges(net_u);
        return EdgesPair(u_in_edges.template to_container<Edges>(), u_out_edges.template to_container<Edges>());
      };
    return ext_tree_sw_map<NodeDescToEdges>(ext, extract_edges);
  }


}// namespace
