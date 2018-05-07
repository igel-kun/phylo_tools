

#pragma once

#include "utils/utils.hpp"
#include "utils/types.hpp"


namespace PT{

  template<class _Network, class _LabelMap>
  class Mapper
  {
  protected:
    const _Network& N;
    const Tree& T;
    const _LabelMap* const my_own_labelmap;
    const _LabelMap& labelmap;
    // a node in T can be displayed by many nodes of N
    DisplayMap display_map;

    typedef typename _LabelMap::mapped_type::first_type LabelType;
  public:

    Mapper(const _Network& _N, const Tree& _T, const _LabelMap& _labelmap):
      N(_N), T(_T), my_own_labelmap(nullptr), labelmap(_labelmap) 
    {
      assert(T.is_preordered());
      initialize();
    }

    // build our own labelmap instead of receiving it
    Mapper(const _Network& _N, const Tree& _T):
      N(_N), T(_T), my_own_labelmap(build_labelmap(_N, _T, my_own_labelmap)), labelmap(*my_own_labelmap)
    {
      assert(T.is_preordered());
      DEBUG3(std::cout << "label map:\n");
      for(auto x : labelmap) std::cout << x.first << "->("<<x.second.first<<","<<x.second.second<<")"<<std::endl;
      initialize();
    }

    ~Mapper()
    {
      if(my_own_labelmap) delete my_own_labelmap;
    }


    // compute the vector of minimal nodes of N displaying the node u of T
    //NOTE: the returned vector is sorted!
    const IndexVec& who_displays(const uint32_t v_idx)
    {
      auto display_iter = display_map.find(v_idx);
      if(display_iter == display_map.end()){
        const Tree::Node& v = T[v_idx];
        switch(v.out.size()){
          case 0:
            std::cout << "leaf "<<v_idx<<" is displayed by " << labelmap.at(T.get_name(v_idx)).first << std::endl;
            return emplace_leaf_entry_hint(display_iter, v_idx, labelmap.at(T.get_name(v_idx)).first);
          case 1:
            return who_displays(v.out[0].head());
          case 2: {
              // save the result for future reference
              std::cout << "finding vertex displaying the cherry ("<<v.out[0]<<","<<v.out[1]<<")\n";
              IndexVec& result = display_map.emplace_hint(display_iter, PWC, std::tuple<uint32_t>(v_idx), std::tuple<uint32_t>())->second;
              get_displaying_vertices_binary(v.out[0].head(), v.out[1].head(), result);
              std::cout << "found that "<<v_idx<<" is displayed by "<<result<<std::endl;
              return result;
            }
          default: {
              std::cout << "vertex "<<v_idx<<" has "<<v.out.size()<<" outessors"<<std::endl;
              assert(false);
              // save the result for future reference
              IndexVec& result = display_map.emplace_hint(display_iter, PWC, std::tuple<uint32_t>(v_idx), std::tuple<uint32_t>())->second;
              for(uint32_t i = 0; i < v.out.size(); ++i){
#warning continue here
              }
              return result;
            }
        }
      } return display_iter->second;
    }

    bool verify_display()
    {
      return !who_displays(T.get_root()).empty();
    }


    virtual const IndexVec& emplace_leaf_entry_hint(const DisplayMap::iterator& hint, const uint32_t v_idx, const LabelType& displaying) = 0;
    virtual void get_displaying_vertices_binary(const uint32_t child1, const uint32_t child2, IndexVec& result) = 0;
    
    virtual void initialize(){
    }

  };
}

