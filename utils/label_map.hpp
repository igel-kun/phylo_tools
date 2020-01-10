

#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"
#include "network.hpp"

namespace PT {

#define NO_LABEL UINT32_MAX

  // the last parameter is only to differentiate the different build_labelmap functions
  template<class IterType = IndexVec::const_iterator>
  MULabelMap* build_labelmap(const LabeledNodeIterFactory<IterType>& Nfac,
                             const LabeledNodeIterFactory<IterType>& Tfac,
                             const MULabelMap* sentinel = nullptr)
  {
    MULabelMap* result = new MULabelMap();

    for(const LabeledNode& p: Nfac){
      auto& leaf_correspondance = (*result)[p.second];
      leaf_correspondance.first.push_back(p.first);
      leaf_correspondance.second = NO_LABEL;
    }
    for(const LabeledNode& p: Tfac)
      (*result)[p.second].second = p.first;

    return result;
  }

  //! build a mapping between the labeled nodes produced by the LabeledNodeIterFactories
  template<class IterType = IndexVec::const_iterator>
  LabelMap* build_labelmap(const LabeledNodeIterFactory<IterType>& Nfac,
                           const LabeledNodeIterFactory<IterType>& Tfac,
                           const LabelMap* sentinel = nullptr)
  {
    LabelMap* result = new LabelMap();

    for(const LabeledNode& p: Nfac)
      if(!p.second.empty()){
        if(!result->DEEP_EMPLACE_TWO(p.second, p.first, NO_LABEL).second)
          throw std::logic_error("single-label map, but first tree/network is multi-labelled");
      }
      
    for(const LabeledNode& p: Tfac)
      if(!p.second.empty()){
        auto& _tmp = (*result)[p.second].second;
        if(_tmp != NO_LABEL) throw std::logic_error("single-label map, but second tree/network is multi-labelled");
        _tmp = p.first;
      }

    return result;
  }

  template<class _Network = Network<>, class _Tree = Tree<>, class _LMap>
  _LMap* build_labelmap(const _Network& N, const _Tree& T, const _LMap* sentinel = nullptr)
  {
    return build_labelmap(N.get_nodes_labeled(), T.get_nodes_labeled(), sentinel);
  }

  template<class _Network = Network<>, class _Tree = Tree<>, class _LMap>
  _LMap* build_leaf_labelmap(const _Network& N, const _Tree& T, const _LMap* sentinel = nullptr)
  {
    return build_labelmap(N.get_leaves_labeled(), T.get_leaves_labeled(), sentinel);
  }

}
