

#pragma once

#include <vector>
#include "label_matching.hpp"
#include "bimap.hpp"

namespace PT {
  
  // a containment checker, testing if a (possibly multi-labelled) host-tree contains a single-labeled guest tree
  template<class Host, class Guest, bool leaf_labels_only = true, class = enable_if_t<is_single_labeled<Guest>>
  class TreeInTreeContainment
  {
  protected:
    using HostTranslation = typename Host::Translation;
    // sometimes we need nodes in order, so this is a bidirectional translation map
    using NodeOrder = std::IntegralBimap<RawConsecutiveMap<Node, Node>, HostTranslation>;
    // our DP table assigns each node in the guest a list of nodes in the host
    using DPTable = typename Guest::template NodeMap<NodeVec>;
   
    const Guest& guest;
    const Host& host;

    std::shared_ptr<NodeOrder> order2node;

    // if we're not given a node-order we will construct one by dfs
    std::shared_ptr<NodeOrder> construct_node_order()
    {
      std::shared_ptr<NodeOrder> result = make_shared<NodeOrder>();
      Node counter = 0;
      for(const Node u: host.dfs().preorder())
        result->try_emplace(counter, u);
      return result;
    }

    void init_DP_leaves()
    {
      const auto label_matching = leaf_labels_only ? get_leaf_label_matching(guest, host) : get_label_matching(guest, host);
      for(const auto& lab_pair: seconds(label_matching)){
        const Node gu = lab_pair.first.front();
        for(const Node hu: lab_pair.second)
#error continue here!
      }
    }

  public:

    TreeInTreeContainment(const Guest& _guest, const Host& _host, const NodeOrder& _order2node):
      guest(_guest),
      host(_host),
      order2node(&_order2node, std::NoDeleter())
    {}
    TreeInTreeContainment(const Guest& _guest, const Host& _host, const std::shared_ptr<NodeOrder>& _order2node):
      guest(_guest),
      host(_host),
      order2node(_order2node)
    {}
    TreeInTreeContainment(const Guest& _guest, const Host& _host):
      guest(_guest),
      host(_host),
      order2node(construct_node_order())
    {}

  };
}
