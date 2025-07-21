
#pragma once

/* These are 3 implementations of LSA-Tree calculation:
 * 1. straightforward (idom(v) = LCA[in the domtree] of idom(u) for all parents u of v in N
 *    (see https://cs.stackexchange.com/questions/43105/dominator-tree-for-dag)
 *    with LCA queries resolved using LinkCutTrees (Sleator & Tarjan '85); running time O(m log n)
 * 2. Lengauer Tarjan [LT'79] (this is for general digraphs); running time: O(m \alpha(m))
 */

#include "raw_vector_map.hpp"

#include "network.hpp"
#include "link_cut_tree.hpp"

namespace PT{
  // naïve computation of the LSA-tree by DLS through the network and node-by-node construction of the LSA as LinkCutTree
  // see also: https://cs.stackexchange.com/questions/43105/dominator-tree-for-dag
  template<class Network> 
  struct NaiveDominatorOracle {
    // ------- static stuff --------
    // ------- members --------
    mstd::LinkCutTree<NodeDesc> lct;
    NodeMap<NodeDesc> dominator;

    // ------- construction & desctruction ---------
    NaiveDominatorOracle(const Network& N) requires (Network::has_unique_root) {
      init(N.root());
    }
    NaiveDominatorOracle(const NodeDesc root) {
      init(root);
    }

    // ------- operators --------
    NodeDesc operator[](const NodeDesc v) const { return dominator.at(v); }

    // ------- methods: initialization --------
    void init(const NodeDesc root) {
      // NOTE: the depth-last traversal guarantees that all parents of x have been treated before x
      for(const NodeDesc x: Traversal<preorder | depth_last_traversal, Network>{root}) {
        DEBUG5(std::cout << "getting dominator of "<<x<<" (parents: "<<Network::parents(x)<<")\n");
        DEBUG5(std::cout << "===== Current LCT =====\n"; print_link_cut_tree(std::cout, lct); );
        const NodeDesc idom = domLCA(Network::parents(x));
        DEBUG4(std::cout << "dominator of "<<x<<" (parents: "<<Network::parents(x)<<") is "<<idom<<", now adding to the LCT tree\n");
        if(idom != NoNode) {
          lct.emplace_leaf(idom, x);
        } else lct.emplace_tree(x);
        dominator.emplace(x, idom);
      }
    }
    // ------- methods: modification --------
    // ------- methods: query --------
    const auto& get_dominator_map() const { return dominator; }

    template<NodeIterableType Nodes>
    NodeDesc domLCA(const Nodes& nodes) const {
      if(not nodes.empty()) {
        auto it = nodes.begin();
        auto lct_node = lct.key_to_node.at(*it).get();
        while(++it != nodes.end())
          lct_node = lct.LCA(lct_node, lct.key_to_node.at(*it).get());
        return lct_node->key;
      } else return NoNode;
    }

    // to make the actual dominator-tree, we can just use the immediate dominator relation as edges
    template<class Tree, class... EmplacerArgs>
    Tree make_dominator_tree(EmplacerArgs&&... args) const {
      // NOTE: we have to reverse all domination pairs; (x, idom(x)) should incur the edge idom(x) --> x
      return Tree{
        dominator 
          | std::ranges::views::filter([](const auto& xy){ return xy.second != NoNode; }) 
          | std::ranges::views::transform([](const auto& xy){ return std::pair{xy.second, xy.first};}),
        std::forward<EmplacerArgs>(args)...
      };
    }

  };


  
  // computation of the LSA tree following [Lengauer & Tarjan, 1979]
  // it runs in O((n+m)*alpha(n,m)) which is as good as linear time for all intends and purposes
  // NOTE: LT'79 is actually for general digraphs
  template<class Network>
  struct LTDominatorOracle {
    NodeMap<NodeDesc> dominator;
#warning "TODO: write me"
    LTDominatorOracle(const Network& _N) {}

    NodeDesc operator[](const NodeDesc v) const { return dominator.at(v); }
    const auto& get_dominator_map() const { return dominator; }

  };
}

