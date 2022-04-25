

#pragma once

#include <unordered_map>
#include <string>
#include <string_view>

#include "utils.hpp"
#include "types.hpp"
#include "node.hpp"
#include "tags.hpp"

namespace PT {

  template<class T>
  using LabelTypeOf = std::remove_cvref_t<T>::LabelType;

  template<class T> struct _AsMapKey { using type = std::decay_t<T>; };
  template<class T> requires std::is_same_v<std::decay_t<T>, std::string> struct _AsMapKey<T> { using type = std::string_view;};
  template<class T> using AsMapKey = typename _AsMapKey<T>::type;

  template<class A, class B>
  concept CompatibleLabels = !std::is_void_v<LabelTypeOf<A>> &&
                             !std::is_void_v<LabelTypeOf<B>> &&
                             std::is_same_v<LabelTypeOf<A>, LabelTypeOf<B>>;

  template<class T>
  concept StrictLabelMatchingType = requires {
    typename T::StorageA;
    typename T::StorageB;
  };
  template<class T> concept LabelMatchingType = StrictLabelMatchingType<std::remove_cvref_t<T>>;

  template<StorageEnum _LabelStorageA = singleS, StorageEnum _LabelStorageB = singleS>
  using LabelStoragePair = std::pair<NodeStorage<_LabelStorageA>, NodeStorage<_LabelStorageB>>;



  // a label matching maps labels (strings) to pairs of node-containers (singleton_set for single-labeled networks)
  template<StrictPhylogenyType NetA,
           StrictPhylogenyType NetB,
           StorageEnum _LabelStorageA = singleS,
           StorageEnum _LabelStorageB = singleS>
            requires CompatibleLabels<NetA, NetB>
  class LabelMatching:
    public std::unordered_map<AsMapKey<typename NetA::LabelType>, LabelStoragePair<_LabelStorageA, _LabelStorageB>> {
  public:
    using LabelType = AsMapKey<typename NetA::LabelType>;
    using StorageA = NodeStorage<_LabelStorageA>;
    using StorageB = NodeStorage<_LabelStorageB>;
  protected:
    using Parent = std::unordered_map<LabelType, std::pair<StorageA, StorageB>>;
  public:

    // allow empty label matchings
    LabelMatching() {}

    // build a label matching from 2 iterator factories (one for each network) yielding (node, label) pairs
    // enable only if the labelcontainers have pairs as value types
    template<NodeIterableType NodeContainerA, NodeIterableType NodeContainerB>
      requires (!PhylogenyType<NodeContainerA> && !PhylogenyType<NodeContainerB>)
    LabelMatching(const NodeContainerA& Nfac, const NodeContainerB& Tfac): Parent()
    {
      // step 1: create a mapping of labels to nodes in N
      for(const NodeDesc p: Nfac) {
        const auto& p_label = node_of<NetA>(p).label();
        if(!p_label.empty()){
          std::cout << "treating node "<<p<<" with label "<<p_label<<"\n";
          // the factory Nfac gives us pairs of (node, label)
          // find entry for p's label or construct it by matching p to the default constructed (empty LabelNodeStorage)
          // (if the entry was already there, then we have 2 nodes of the same label, so throw an exception)
          const auto [iter, success] = Parent::try_emplace(p_label);
          // if A is single-label and the label was already there, we have to bail...
          if constexpr (_LabelStorageA == singleS)
            if(!success)
              throw std::logic_error("single-label map for multi-labeled tree/network (first argument of the label matching)");
          // otherwise, just add the node to (the first part of) the entry
          append(iter->second.first, p);
        }
      }
      // step 2: for each node u with label l in T, add u to the set of T-nodes mapped to l
      for(const NodeDesc p: Tfac) {
        const auto& p_label = node_of<NetB>(p).label();
        if(!p_label.empty()){
          // the factory Tfac gives us pairs of (node, label)
          auto& matched_pair = Parent::try_emplace(p_label).first->second;
          // if B is single-label and the second part of the pair already contains a node, then we have to bail...
          if constexpr (_LabelStorageB == singleS)
            if(!matched_pair.second.empty())
              throw std::logic_error("single-label map for multi-labeled network/tree (second argument of the label matching)");
          // otherwise, just add p to (the second part of) the entry
          append(matched_pair.second, p);
        }
      }
    }

    LabelMatching(const NetA& A, const NetB& B): LabelMatching(A.nodes(), B.nodes()) {}
    LabelMatching(const leaf_labels_only_tag, const NetA& A, const NetB& B): LabelMatching(A.leaves(), B.leaves()) {}

    // construct a label matching from another label matching by, for each pair (A,B) of label sets corresponding to label L,
    // appending {L, other_pair_to_our_pair((A,B))} to this
    // NOTE: if the other label matching is not const, we WILL move out of it! If you don't want that, pass a const reference to other
    template<CompatibleLabels<LabelMatching> OtherLabelMatching> requires (LabelMatchingType<OtherLabelMatching>)
    LabelMatching(OtherLabelMatching&& other, auto&& other_pair_to_our_pair): Parent()
    {
      for(auto&& [label, node_sets]: other)
        Parent::try_emplace(std::move(label), other_pair_to_our_pair(node_sets));
    }
  };

  template<StrictPhylogenyType NetA,
           StrictPhylogenyType NetB,
           StorageEnum LabelStorageA = singleS,
           StorageEnum LabelStorageB = singleS>
             requires CompatibleLabels<NetA, NetB>
  auto get_label_matching(const NetA& A, const NetB& B)
  { return LabelMatching<NetA, NetB, LabelStorageA, LabelStorageB>(A, B); }


  template<StrictPhylogenyType NetA,
           StrictPhylogenyType NetB,
           StorageEnum LabelStorageA = singleS,
           StorageEnum LabelStorageB = singleS>
             requires CompatibleLabels<NetA, NetB>
  auto get_leaf_label_matching(const NetA& A, const NetB& B)
  { return LabelMatching<NetA, NetB, LabelStorageA, LabelStorageB>(leaf_labels_only_tag(), A, B); }

}
