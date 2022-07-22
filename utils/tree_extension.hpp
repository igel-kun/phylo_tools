
// classes for handling tree extensions of networks

#pragma once

#include "utils/union_find.hpp"
#include "utils/set_interface.hpp"
#include "utils/extension.hpp"
#include "utils/network.hpp"

namespace PT{

  template<StrictPhylogenyType Network, class ExtTreeOrNodeData, class EdgeData>
  using TreeChooser = std::conditional_t<PT::StrictTreeType<ExtTreeOrNodeData> && std::is_void_v<EdgeData>,
                  ExtTreeOrNodeData, PT::CompatibleTree<const Network, ExtTreeOrNodeData, EdgeData>>;


  // construct an extension tree from a network & an extension
  // NOTE: make_node_data() will receive the description of the node in the network
  // NOTE: this will work also for partial extensions (covering only parts of V(N)), but your ExtTree may have to support multiple roots
  // NOTE: if the extension is over the whole network, then construct T without root-tracking, and just translate the last node of the extension
  // NOTE: either specify a Network and a new NodeData and EdgeData, in which case we base the TreeExtension on PT::CompatibleTree
  //       or specify a PT::StrictTreeType directly as second argument
  template<StrictPhylogenyType Network, class ExtTreeOrNodeData = void, class EdgeData = void>
  class TreeExtension: public TreeChooser<Network, ExtTreeOrNodeData, EdgeData> {
    using ExtTree = TreeChooser<Network, ExtTreeOrNodeData, EdgeData>;
    using Parent = ExtTree;

    template<bool track_roots = true,
             NodeTranslationType NetToTree = NodeTranslation,
             DataExtracterType   Extracter = DataExtracter<Network>>
    static ExtTree _ext_to_tree(const Extension& ex,
                                NetToTree&& net_to_tree = NetToTree(),
                                Extracter&& data_extracter = Extracter())
    {
      ExtTree T;
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
            append(new_children, highest.set_of(v).get_representative());
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
      return T;
    }

    // allow calling without a node translation
    template<bool track_roots = true, class First, class... Args> requires (!NodeTranslationType<First>)
    static ExtTree _ext_to_tree(const Extension& ex, First&& first, Args&&... args) {
      return _ext_to_tree<track_roots>(ex, NodeTranslation(), make_data_extracter<Network, ExtTree>(std::forward<First>(first), std::forward<Args>(args)...));
    }

    // check if we have been correctly constructed
    void sanity_check(const Extension& ex) const {
      assert(this->empty() == ex.empty());
      assert(this->edgeless() == (ex.size() <= 1));
    }
  public:

    operator ExtTree() { return *this; }
    operator const ExtTree() const { return *this; }

    // for partial extensions, we will have to track the roots
    template<class... Args>
    TreeExtension(const partial_extension_tag, const Extension& ex, Args&&... args):
      Parent{_ext_to_tree<true>(ex, std::forward<Args>(args)...)}
    { sanity_check(ex); }

    template<class... Args>
    TreeExtension(const Extension& ex, Args&&... args):
      Parent{_ext_to_tree<false>(ex, std::forward<Args>(args)...)}
    { sanity_check(ex); }

    // compute the scanwidth of all nodes in a given extension tree
    // NOTE: NetworkDegree is used to return the degrees (pair of indegree and outdegree) of a node of the tree (!!!) in the network
    //       to this end, the tree node has to be converted to a corresponding network node within the function
    //       this can be done by, for example, inverting the net_to_tree translation map filled by ext_to_tree()
    //       or by storing the network NodeDesc's inside the nodes themselves using the make_node_data function passed to ext_to_tree
    // NOTE: this can be used to construct the actual edge- or node- set corresponding to the scanwidth entry by passing a suitable function network_degrees
    void sw_map_meta(auto&& network_degrees, auto&& out) const {
      for(const NodeDesc u: this->nodes_postorder()){
        auto [indeg, outdeg] = network_degrees(u);
        auto& sw_u = indeg;
        for(const NodeDesc v: this->children(u))
          sw_u += out.at(v);
        sw_u -= outdeg;
        append(out, u, std::move(sw_u));
      }
    }

    template<class Out>
    void sw_map(auto&& tree_node_to_network_node, Out&& out) const {
      return sw_map_meta([&](const NodeDesc tree_u){
          const NodeDesc net_u = tree_node_to_network_node(tree_u);
          return Network::degrees(net_u);
        },
        std::forward<Out>(out)
      );
    }
    template<class Out = NodeMap<sw_t>, class NodeConverter>
    Out get_sw_map(NodeConverter&& tree_node_to_network_node) const {
      Out result;
      sw_map(std::forward<NodeConverter>(tree_node_to_network_node), result);
      return result;
    }


    template<NodeMapType Out>
    void sw_nodes_map(auto&& tree_node_to_network_node, Out&& out) const {
      using Nodes = mstd::mapped_type_of_t<Out>;
      static_assert(std::is_same_v<std::remove_cvref_t<mstd::value_type_of_t<Nodes>>, NodeDesc>);
      using NodesAndNode = std::pair<Nodes, NodeDesc>;
      return sw_map_meta([&](const NodeDesc tree_u){
          const NodeDesc net_u = tree_node_to_network_node(tree_u);
          const auto& u_parents = Network::parents(net_u);
          return NodesAndNode(std::piecewise_construct, std::tuple{u_parents.begin(), u_parents.end()}, std::tuple{net_u});
        },
        std::forward<Out>(out)
      );
    }
    template<NodeMapType Out = NodeMap<NodeVec>, class NodeConverter>
    Out get_sw_nodes_map(NodeConverter&& tree_node_to_network_node) const {
      Out result;
      sw_nodes_map(std::forward<NodeConverter>(tree_node_to_network_node), result);
      return result;
    }

    template<NodeMapType Out>
    void sw_edges_map(auto&& tree_node_to_network_node, Out&& out) const {
      using Edges = mstd::mapped_type_of_t<Out>;
      static_assert(EdgeType<mstd::value_type_of_t<Edges>>);
      using EdgesPair = std::pair<Edges, Edges>; 
      return sw_map_meta([&](const NodeDesc tree_u) {
          const NodeDesc net_u = tree_node_to_network_node(tree_u);
          const auto& u_in_edges = Network::in_edges(net_u);
          const auto& u_out_edges = Network::out_edges(net_u);
          return EdgesPair(u_in_edges.template to_container<Edges>(), u_out_edges.template to_container<Edges>());
        },
        std::forward<Out>(out)
      );
    }
    template<NodeMapType Out = NodeMap<typename Network::EdgeVec>, class NodeConverter>
    Out get_sw_edges_map(NodeConverter&& tree_node_to_network_node) const {
      Out result;
      sw_edges_map(std::forward<NodeConverter>(tree_node_to_network_node), result);
      return result;
    }


  };




}// namespace
