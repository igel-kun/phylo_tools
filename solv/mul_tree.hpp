
#pragma once

#include "utils/utils.hpp"
#include "utils/types.hpp"
#include "utils/network.hpp"
#include "utils/label_map.hpp"
#include "mapper.hpp"

#include <algorithm>

namespace PT{



  template<class MULTree>
  class MULTreeMapper: public Mapper<MULTree, MULabelMap>
  {
    using Parent = Mapper<MULTree, MULabelMap>;
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

      for(uint32_t id1 = 0, id2 = 0;;){
        std::cout << "checking indices "<<id1<<" & "<<id2<<std::endl;
        //NOTE: by always advancing the smaller of displaying1[id1] and displaying2[id2],
        //      we know that every set of LCAs that are ancestors of one another will be consecutive.
        //      Thus, we only need to check against the last LCA to maintain minimality of the
        //      items in result (this needs T being monotone and binary though)
        const uint32_t x = displaying1[id1];
        const uint32_t y = displaying2[id2];
        const uint32_t xy_lca = N.LCA(x, y);
        if(x < y) {
          // if the lca is among x and y, then it is x
          if((xy_lca != x) && (result.empty() || (N.get_minimum(result.back(), xy_lca) == UINT32_MAX)))
            result.push_back(xy_lca);
          // next, advance id1 or, if impossible advance id2
          if(id1 == displaying1.size() - 1){
            if(++id2 == displaying2.size()) break;
          } else ++id1;
        } else {
          // if the lca is among x and y, then it is y
          if((xy_lca != y) && (result.empty() && (N.get_minimum(result.back(), xy_lca) == UINT32_MAX)))
            result.push_back(xy_lca);
          // next, advance id2 or, if impossible advance id1
          if(id2 == displaying2.size() - 1){
            if(++id1 == displaying1.size()) break;
          } else ++id2;
        }
      }
      // note that result may not be sorted, so we sort it ourselves
      std::sort(result.begin(), result.end());
    }

    void initialize() {}

    const IndexVec& emplace_leaf_entry_hint(const DisplayMap::iterator& hint, const uint32_t v_idx, const LabelType& displaying)
    {
      return displaying;
    }
  public:

  };
}
