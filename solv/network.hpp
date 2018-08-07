

#pragma once

#include "utils/utils.hpp"
#include "utils/types.hpp"
#include "utils/dominators.hpp"
#include "utils/tree_comps.hpp"
#include "utils/tc_preprocess.hpp"
#include "mapper.hpp"

namespace PT{

  template<class _Network>
  class NetworkMapper: public Mapper<_Network, LabelMap>
  {
    using Parent = Mapper<_Network, LabelMap>;
    using Parent::N;
    using Parent::T;
    using Parent::labelmap;
    using Parent::display_map;
    using Parent::Mapper;
    using typename Parent::LabelType;

    LSATree lsa;
    ComponentRoots croots;

  public:
    using Parent::who_displays;
    using Parent::verify_display;

  protected:
    // get subtrees of N displaying the subtree of T rooted at v
    //NOTE: result will be sorted
    void get_displaying_vertices_binary(const Tree::Node& v, IndexVec& result)
    {
      const uint32_t child0 = v.out[0].head();
      const uint32_t child1 = v.out[1].head();

      const IndexVec& displaying1 = who_displays(child0);
      std::cout << "we know that "<<child0<<" is displayed at "<<displaying1<<std::endl;
      if(displaying1.empty()) return;

      const IndexVec& displaying2 = who_displays(child1);
      std::cout << "we know that "<<child1<<" is displayed at "<<displaying2<<std::endl;
      if(displaying2.empty()) return;

      assert(false);
    }

    const IndexVec& emplace_leaf_entry_hint(const DisplayMap::iterator& hint, const uint32_t v_idx, const LabelType& displaying)
    {
      return display_map.emplace_hint(hint, PWC, std::make_tuple(v_idx), std::make_tuple(1, displaying))->second;
    }

    // run the reduction rules and simple (1,1) branching of stable reticulations
    void preprocess()
    {
      assert(false);
    }

  public:

    NetworkMapper(const _Network& _N, const Tree& _T):
      Parent(_N, _T),
      lsa(_N),
      croots(_N)
    {
      preprocess();
    }

  };


}

