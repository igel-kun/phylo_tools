

#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"
#include "network.hpp"

namespace TC {

  // the last parameter is only to differentiate the different build_labelmap functions
  template<class _Network = Network, class _Tree = Tree>
  MULabelMap* build_labelmap(const _Network& N, const _Tree& T, const MULabelMap* sentinel = nullptr)
  {
    MULabelMap* result = new MULabelMap();

    for(const LabeledVertex& p: N.get_leaves_labeled()){
      auto& leaf_correspondance = (*result)[p.second];
      leaf_correspondance.first.push_back(p.first);
      leaf_correspondance.second = UINT32_MAX;
    }
    for(const LabeledVertex& p: T.get_leaves_labeled())
      (*result)[p.second].second = p.first;

    return result;
  }

  template<class _Network = Network, class _Tree = Tree>
  LabelMap* build_labelmap(const _Network& N, const _Tree& T, const LabelMap* sentinel = nullptr)
  {
    LabelMap* result = new LabelMap();

    for(const LabeledVertex& p: N.get_leaves_labeled()){
      (*result)[p.second] = {p.first, UINT32_MAX};
    }
    for(const LabeledVertex& p: T.get_leaves_labeled())
      (*result)[p.second].second = p.first;

    return result;
  }

}
