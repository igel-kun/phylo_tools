
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
  template<TreeType ExtTree>
  class ExtToTree {

    template<bool track_roots>
    struct ExtToTreeHelper {
      template<PhylogenyType _Network,
               NodeTranslationType NetToTree = NodeTranslation,
               DataExtracterType   Extracter = DataExtracter<_Network>>
      static void ext_to_tree(const _Network& N, const Extension& ex, ExtTree& T,
                              NetToTree&& net_to_tree = NetToTree(),
                              Extracter&& data_extracter = Extracter())
      {
        auto emplacer = EdgeEmplacers<track_roots>::make_emplacer(T, net_to_tree, std::forward<Extracter>(data_extracter));
        // we can use a disjoint set forest with no-rank union to find the current highest node of the weakly-connected component of a node
        std::DisjointSetForest<NodeDesc> highest;

        DEBUG3(std::cout << "constructing extension tree from "<<ex<<std::endl);
        for(const auto& u: ex){
          // step 1: add a new set to the DSF with only u
          highest.add_new_set({u});
          DEBUG3(std::cout << "highest ancestors of node "<<u<<": "<<highest<<std::endl);
          // step 2: establish u as the parent in Gamma of all highest nodes in all weakly connected components (in G[ex[1..u]]) of its children in N
          NodeSet new_children; // NOTE: many children of u in the network may have the same "highest current ancestor" in the tree; use a set to avoid dupes
          for(const NodeDesc v: N.children(u)){
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
        emplacer.finalize(N);
      }

      // allow calling without a node translation
      template<PhylogenyType _Network, class First, class... Args> requires (!NodeTranslationType<First>)
      static void ext_to_tree(const _Network& N, const Extension& ex, ExtTree& T, First&& first, Args&&... args) {
        ext_to_tree(N, ex, T, NodeTranslation(), make_data_extracter<_Network, ExtTree>(std::forward<First>(first), std::forward<Args>(args)...));
      }
    };

  public:


    template<PhylogenyType _Network, class... Args>
    static ExtTree ext_to_tree(const _Network& N, const Extension& ex, Args&&... args) {
      ExtTree T; // yay RVO
      if(ex.size() == N.num_nodes()) {
        // if the extension is over the whole network, then construct T without root-tracking, and just translate N's roots
        ExtToTreeHelper<false>::ext_to_tree(N,ex,T, std::forward<Args>(args)...);
      } else {
        // if the extension is only over parts of the network, then activate root tracking
        ExtToTreeHelper<true>::ext_to_tree(N,ex,T, std::forward<Args>(args)...);
      }
      return T;
    }
  };


  // compute the scanwidth of all nodes in a given extension tree
  // NOTE: NetworkDegree is used to return the degrees (pair of indegree and outdegree) of a node of the tree (!!!) in the network
  //       to this end, the tree node has to be converted to a corresponding network node within the function
  //       this can be done by, for example, inverting the net_to_tree translation map filled by ext_to_tree()
  //       or by storing the network NodeDesc's inside the nodes themselves using the make_node_data function passed to ext_to_tree
  template<TreeType _Tree, class NetworkDegrees, std::ContainerType _Container = NodeMap<Degree>>
  _Container ext_tree_sw_map(const _Tree& ext, NetworkDegrees&& network_degrees, _Container&& out = _Container()) {
    for(const NodeDesc u: ext.nodes_postorder()){
      auto [indeg, outdeg] = network_degrees(u);
      Degree sw_u = indeg;
      for(const NodeDesc v: ext.children(u))
        sw_u += out[v];
      sw_u -= outdeg;
      append(out, u, sw_u);
      DEBUG3(std::cout << "found sw("<<u<<") = "<<sw_u<<std::endl);
    }
    return out;
  }

}// namespace
