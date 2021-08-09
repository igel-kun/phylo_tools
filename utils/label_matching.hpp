

#pragma once

#include <unordered_map>
#include <string>

#include "utils.hpp"
#include "types.hpp"
#include "node.hpp"

namespace PT {

  struct leaf_labels_only_tag {};

  // a label matching maps labels (strings) to pairs of node-containers (singleton_set for single-labeled networks)
  template<StorageEnum _LabelStorageA = singleS, StorageEnum _LabelStorageB = singleS, class _LabelType = std::string>
  class LabelMatching: public std::unordered_map<_LabelType, std::pair<StorageType<_LabelStorageA, NodeDesc>, StorageType<_LabelStorageB, NodeDesc>>> {
  public:
    using StorageA = StorageType<_LabelStorageA, NodeDesc>;
    using StorageB = StorageType<_LabelStorageB, NodeDesc>;
  protected:
    using Parent = std::unordered_map<_LabelType, std::pair<StorageA, StorageB>>;
  public:

    // allow empty label matchings
    LabelMatching() {}

    // build a label matching from 2 iterator factories (one for each network) yielding (node, label) pairs
    // enable only if the labelcontainers have pairs as value types
    template<class NodeLabelContainerA, class NodeLabelContainerB> requires std::is_pair<typename NodeLabelContainerA::value_type> && std::is_pair<typename NodeLabelContainerB::value_type>>
    LabelMatching(const NodeLabelContainerA& Nfac, const NodeLabelContainerB& Tfac):
      Parent()
    {
      // step 1: create a mapping of labels to nodes in N
      for(const auto& p: Nfac) if(!p.second.empty()){
        std::cout << "treating label "<<p<<"\n";
        // the factory Nfac gives us pairs of (node, label)
        // find entry for p's label or construct it by matching p to the default constructed (empty LabelNodeStorage)
        // (if the entry was already there, then we have 2 nodes of the same label, so throw an exception)
        const auto [iter, success] = Parent::try_emplace(p.second);
        // if A is single-label and the label was already there, we have to bail...
        if constexpr (_LabelStorageA == singleS)
          if(!success)
            throw std::logic_error("single-label map for multi-labeled tree/network");
        // otherwise, just add the node to the (first part of the) entry
        append(iter->second.first, p.first);
      }
      // step 2: for each node u with label l in T, add u to the set of T-nodes mapped to l
      for(const auto& p: Tfac) if(!p.second.empty()){
        // the factory Tfac gives us pairs of (node, label)
        auto& matched_pair = Parent::try_emplace(p.second).first->second;
        // if B is single-label and the second part of the pair already contains a node, then we have to bail...
        if constexpr (_LabelStorageB == singleS)
          if(!matched_pair.second.empty())
            throw std::logic_error("single-label map for multi-labeled network/tree");
        // otherwise, just add p to the (second part of) the entry
        append(matched_pair.second, p.first);
      }
    }

    // enable only if NetworkA & NetworkB has a NetworkTag (that is, it's a network or a tree)
    template<class NetworkA, class NetworkB> requires() { typename NetworkA::NetworkTag; typename NetworkB::NetworkTag; }
    LabelMatching(const NetworkA& A, const NetworkB& B):
      LabelMatching(A.nodes_labeled(), B.nodes_labeled())
    {}

    // construct label-maps only for the leaf-labels
    template<class NetworkA, class NetworkB> requires() { typename NetworkA::NetworkTag; typename NetworkB::NetworkTag; }
    LabelMatching(const leaf_labels_only_tag, const NetworkA& A, const NetworkB& B):
      LabelMatching(A.leaves_labeled(), B.leaves_labeled())
    {}
  };

  template<StorageEnum LabelStorageA = singleS, StorageEnum LabelStorageB = singleS, class LabelType = std::string>
  LabelMatching<LabelStorageA, LabelStorageB, LabelType> get_label_matching(const auto& A, const auto& B)
  { return {A, B}; }

  template<StorageEnum LabelStorageA = singleS, StorageEnum LabelStorageB = singleS, class LabelType = std::string>
  LabelMatching<LabelStorageA, LabelStorageB, LabelType> get_leaf_label_matching(const auto& A, const auto& B)
  { return {leaf_labels_only_tag(), A, B}; }

}
