
#pragma once

#include "utils.hpp"
#include "types.hpp"

namespace PT {

  std::ostream& operator<<(std::ostream& os, const IndexVec::const_iterator& it){
    return os << "<iter>"<<std::endl;
  }

  template<typename IterType = IndexVec::const_iterator>
  class LabeledNodeIter
  {
    const NameVec& names;

    IterType leaf_it;

    LabeledNodeIter();
  public:

    LabeledNodeIter(const NameVec& _names, const IterType& _it):
      names(_names), leaf_it(_it)
    {}

    // using function overload resolution as type check is a bit dirty, but this allows using an int as well as an vector::iterator as IterType
    uint32_t deref_iter(const uint32_t x) const { return x;}
    uint32_t deref_iter(const IndexVec::const_iterator& x) const {return *x; }
    uint32_t deref_iter(const IndexVec::iterator& x) const {return *x; }
    
    // dereference
    LabeledNode operator*() const
    {
      const uint32_t x = deref_iter(leaf_it);
      return {x, names.at(x)};
    }

    // increment
    LabeledNodeIter& operator++()
    {
      ++leaf_it;
      return *this;
    }

    //! post-increment
    LabeledNodeIter operator++(int)
    {
      LabeledNodeIter result(names, leaf_it);
      ++(*this);
      return result;
    }

    bool operator==(const LabeledNodeIter& it)
    {
      return it.leaf_it == leaf_it;
    }

    bool operator!=(const LabeledNodeIter& it)
    {
      return it.leaf_it != leaf_it;
    }
    LabeledNodeIter& operator=(const LabeledNodeIter& it)
    {
      leaf_it = it.leaf_it;
      return *this;
    }
  };

  template<typename IterType = IndexVec::const_iterator>
  class LabeledNodeIterFactory
  {
    const NameVec& names;
    const IterType it_begin;
    const IterType it_end;
  public:

    LabeledNodeIterFactory(const NameVec& _names, const IterType& _begin, const IterType& _end):
      names(_names), it_begin(_begin), it_end(_end) 
    {
//      std::cout << "constructed labeled-node-iter-fac with begin = "<<_begin<<" & end = "<<_end<<std::endl;
//      std::cout << "constructed labeled-node-iter-factory with names:"<<std::endl;
//      for(const auto& i: names) std::cout << i << " ";
//      std::cout << std::endl;
    }

    LabeledNodeIter<IterType> begin() const
    {
      return LabeledNodeIter<IterType>(names, it_begin);
    }
    LabeledNodeIter<IterType> end() const
    {
      return LabeledNodeIter<IterType>(names, it_end);
    }

  };
}
