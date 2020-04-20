

#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"
#include "network.hpp"

namespace PT {

  struct LeafLabelsOnlyT {};
  LeafLabelsOnlyT leaf_labels_only;

  template<class _LabelTag>
  static constexpr bool _single_label_v = std::is_same_v<_LabelTag, single_label_tag>;

  template<class _LabelTag>
  using _LabelNodeStorage = std::conditional_t<_single_label_v<_LabelTag>, std::SingletonSet<Node>, HashSet<Node>>;


  // a label matching maps labels (strings) to pairs of node-storages (Node for single-labeled networks, NodeSet for multi-labeled networks)
  // NOTE: N and T must have compatible LabelTypes (f.ex. both std::string with/without wchar etc)
  template<class _LabelTagA, class _LabelTagB, class _LabelType = std::string>
  class LabelMatching: public HashMap<_LabelType, std::pair<_LabelNodeStorage<_LabelTagA>, _LabelNodeStorage<_LabelTagB>>>
  {
    using StorageA = _LabelNodeStorage<_LabelTagA>;
    using StorageB = _LabelNodeStorage<_LabelTagB>;
    using Parent = HashMap<_LabelType, std::pair<StorageA, StorageB>>;

  public:
    using LabelType  = _LabelType;

    // build a label matching from 2 iterator factories (one for each network) yielding (node, label) pairs
    template<class NodeIteratorN, class PropertyGetterN, class NodeIteratorT, class PropertyGetterT>
    LabelMatching(const LabeledNodeIterFactory<NodeIteratorN, PropertyGetterN>& Nfac,
              const LabeledNodeIterFactory<NodeIteratorT, PropertyGetterT>& Tfac):
      Parent()
    {
      // step 1: create a mapping of labels to nodes in N
      for(const auto& p: Nfac){
        // the factory Nfac gives us pairs of (node, label)
        // find entry for p's label or construct it by matching p to the default constructed (empty LabelNodeStorage)
        // (if the entry was already there, then we have 2 nodes of the same label, so throw an exception)
        const auto emp_res = Parent::try_emplace(p.second);
        // if A is single-label and the label was already there, we have to bail...
        if(_single_label_v<_LabelTagA> && !emp_res.second)
          throw std::logic_error("single-label map for multi-labeled tree/network");
        // otherwise, just add the node to the (first part of the) entry
        append(emp_res.first->second.first, p.first);
      }
      // step 2: for each node u with label l in T, add u to the set of T-nodes mapped to l
      for(const auto& p: Tfac){
        // the factory Tfac gives us pairs of (node, label)
        auto& matched_pair = Parent::try_emplace(p.second).first->second;
        // if B is single-label and the second part of the pair already contains a node, then we have to bail...
        if(_single_label_v<_LabelTagB> && !matched_pair.second.empty())
          throw std::logic_error("single-label map for multi-labeled network/tree");
        // otherwise, just add p to the (second part of) the entry
        append(matched_pair.second, p.first);
      }
    }


    template<class NetworkA, class NetworkB,
      class Enabler = typename NetworkA::NetworkTypeTag, // enable only if NetworkA has a NetworkTypeTag (that is, it's a network or a tree)
      class Enabler2 = typename NetworkB::NetworkTypeTag>  // same for NetworkB
    LabelMatching(const NetworkA& A, const NetworkB& B):
      LabelMatching(A.nodes_labeled(), B.nodes_labeled())
    {}

    // prepend 'leaf_labels_only' to the argument list to construct label-maps only for the leaf-labels
    template<class NetworkA, class NetworkB,
      class Enabler = typename NetworkA::NetworkTypeTag, // enable only if NetworkA has a NetworkTypeTag (that is, it's a network or a tree)
      class Enabler2 = typename NetworkB::NetworkTypeTag>  // same for NetworkB
    LabelMatching(const LeafLabelsOnlyT& sentinel, const NetworkA& A, const NetworkB& B):
      LabelMatching(A.leaves_labeled(), B.leaves_labeled())
    {}
  };

  template<class NetworkA, class NetworkB,
    class = std::enable_if_t<std::is_same_v<typename NetworkA::LabelType, typename NetworkB::LabelType>>>
  using LabelMatchingFromNets = LabelMatching<typename NetworkA::LabelTag, typename NetworkB::LabelTag, typename NetworkA::LabelType>;

  template<class NetworkA, class NetworkB,
    class = std::enable_if_t<std::is_same_v<typename NetworkA::LabelType, typename NetworkB::LabelType>>>
  LabelMatchingFromNets<NetworkA, NetworkB> get_label_matching(const NetworkA& A, const NetworkB& B)
  { return LabelMatchingFromNets<NetworkA, NetworkB>(A, B); }

  template<class NetworkA, class NetworkB,
    class = std::enable_if_t<std::is_same_v<typename NetworkA::LabelType, typename NetworkB::LabelType>>>
  LabelMatchingFromNets<NetworkA, NetworkB> get_leaf_label_matching(const NetworkA& A, const NetworkB& B)
  { return {leaf_labels_only, A, B}; }


}
