
#pragma once

#include "utils/utils.hpp"
#include "utils/types.hpp"
#include "utils/network.hpp"
#include "utils/label_map.hpp"
#include "mapper.hpp"

namespace PT{

  template<class _Network>
  class MULNetworkMapper: public Mapper<_Network, MULabelMap>
  {
    using Parent = Mapper<_Network, MULabelMap>;
    using Parent::N;
    using Parent::T;
    using Parent::labelmap;
    using Parent::display_map;
    using typename Parent::LabelType;

  public:
    using Parent::Mapper;
    using Parent::who_displays;
    using Parent::verify_display;

  protected:
    // get subtrees of N displaying the subtree of T rooted at the binary parent of child1 and child2
    //NOTE: result will be sorted
    void get_displaying_vertices_binary(const uint32_t child1, const uint32_t child2, IndexVec& result)
    {
    }

    void initialize() {}

    const IndexVec& emplace_leaf_entry_hint(const DisplayMap::iterator& hint, const uint32_t v_idx, const LabelType& displaying)
    {
    }
  public:

  };
}
