

#pragma once

#include "../utils/utils.hpp"
#include "../utils/types.hpp"
#include "mapper.hpp"

namespace TC{

  class NetworkMapper: public Mapper<Network, LabelMap>
  {
    using Parent = Mapper<Network, LabelMap>;
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
      const IndexVec& displaying1 = who_displays(child1);
      std::cout << "we know that "<<child1<<" is displayed at "<<displaying1<<std::endl;
      if(displaying1.empty()) return;

      const IndexVec& displaying2 = who_displays(child2);
      std::cout << "we know that "<<child2<<" is displayed at "<<displaying2<<std::endl;
      if(displaying2.empty()) return;

      assert(false);
    }

    const IndexVec& emplace_leaf_entry_hint(const DisplayMap::iterator& hint, const uint32_t v_idx, const LabelType& displaying)
    {
      return display_map.emplace_hint(hint, PWC, std::make_tuple(v_idx), std::make_tuple(1, displaying))->second;
    }

  public:

  };


}

