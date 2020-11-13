

#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"
#include "network.hpp"

namespace PT {

  struct LeafLabelsOnlyT {};

  template<class _LabelTag>
  static constexpr bool _single_label_v = std::is_same_v<_LabelTag, single_label_tag>;

  // the LabelNodeStorage for a single_label_tag is a singleton_set, but a Set for all other _LabelTags
  template<class _LabelTag, template<class> class Set = HashSet>
  using LabelNodeStorage = std::conditional_t<_single_label_v<_LabelTag>, NodeSingleton, Set<Node>>;

  // a label matching maps labels (strings) to pairs of node-storages (Node for single-labeled networks, NodeSet for multi-labeled networks)
  // NOTE: N and T must have compatible LabelTypes (f.ex. both std::string with/without wchar etc)
  template<class _LabelTagA, class _LabelTagB, template<class> class Set = HashSet, class _LabelType = std::string>
  class LabelMatching: public HashMap<_LabelType, std::pair<LabelNodeStorage<_LabelTagA, Set>, LabelNodeStorage<_LabelTagB, Set>>>
  {
  public:
    using StorageA = LabelNodeStorage<_LabelTagA, Set>;
    using StorageB = LabelNodeStorage<_LabelTagB, Set>;
    using LabelType  = _LabelType;
  protected:
    using Parent = HashMap<_LabelType, std::pair<StorageA, StorageB>>;
  public:

    // allow empty label matchings
    LabelMatching() {}

    // build a label matching from 2 iterator factories (one for each network) yielding (node, label) pairs
    template<class NodeLabelContainerA, class NodeLabelContainerB, // enable only if the labelcontainers have pairs as value types
      class = std::enable_if_t<std::is_pair<typename NodeLabelContainerA::value_type> && std::is_pair<typename NodeLabelContainerB::value_type>>>
    LabelMatching(const NodeLabelContainerA& Nfac, const NodeLabelContainerB& Tfac):
      Parent()
    {
      std::cout << "mark!\n";
      std::cout << "Nfac is "<<Nfac<<"\n";
      std::cout << "Tfac is "<<Tfac<<"\n";
      // step 1: create a mapping of labels to nodes in N
      for(const auto& p: Nfac) if(!p.second.empty()){
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
      for(const auto& p: Tfac) if(!p.second.empty()){
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
      class Enabler = typename NetworkA::NetworkTag, // enable only if NetworkA has a NetworkTag (that is, it's a network or a tree)
      class Enabler2 = typename NetworkB::NetworkTag>  // same for NetworkB
    LabelMatching(const NetworkA& A, const NetworkB& B):
      LabelMatching(A.nodes_labeled(), B.nodes_labeled())
    {}

    // construct label-maps only for the leaf-labels
    template<class NetworkA, class NetworkB,
      class Enabler = typename NetworkA::NetworkTag, // enable only if NetworkA has a NetworkTag (that is, it's a network or a tree)
      class Enabler2 = typename NetworkB::NetworkTag>  // same for NetworkB
    LabelMatching(const LeafLabelsOnlyT, const NetworkA& A, const NetworkB& B):
      LabelMatching(A.leaves_labeled(), B.leaves_labeled())
    {}
  };

  template<class NetworkA, class NetworkB, template<class> class Set = HashSet,
    class = std::enable_if_t<std::is_same_v<typename NetworkA::LabelType, typename NetworkB::LabelType>>>
  using LabelMatchingFromNets = LabelMatching<typename NetworkA::LabelTag, typename NetworkB::LabelTag, Set, typename NetworkA::LabelType>;

  template<class NetworkA, class NetworkB, template<class> class Set = HashSet,
    class = std::enable_if_t<std::is_same_v<typename NetworkA::LabelType, typename NetworkB::LabelType>>>
  LabelMatchingFromNets<NetworkA, NetworkB, Set> get_label_matching(const NetworkA& A, const NetworkB& B)
  { return {A, B}; }

  template<class NetworkA, class NetworkB, template<class> class Set = HashSet,
    class = std::enable_if_t<std::is_same_v<typename NetworkA::LabelType, typename NetworkB::LabelType>>>
  LabelMatchingFromNets<NetworkA, NetworkB, Set> get_leaf_label_matching(const NetworkA& A, const NetworkB& B)
  { return {LeafLabelsOnlyT(), A, B}; }


}
