
#pragma once

#include "utils.hpp"
#include "types.hpp"

namespace PT {

  std::ostream& operator<<(std::ostream& os, const IndexVec::const_iterator& it){
    return os << "<iter>"<<std::endl;
  }

  template<typename IterType = IndexVec::const_iterator, class NameContainer = NameVec>
  class LabeledNodeIter: public std::iterator<std::random_access_iterator_tag, LabeledNode>
  {
    const NameContainer& names;

    IterType name_it;

    LabeledNodeIter();
  public:

    LabeledNodeIter(const NameContainer& _names, const IterType& _it):
      names(_names), name_it(_it)
    {}

    // using function overload resolution as type check is a bit dirty, but this allows using an int as well as an vector::iterator as IterType
    uint32_t deref_iter(const uint32_t x) const { return x;}
    uint32_t deref_iter(const IndexVec::const_iterator& x) const {return *x; }
    uint32_t deref_iter(const IndexVec::iterator& x) const {return *x; }
    
    // dereference
    LabeledNode operator*() const
    {
      const uint32_t x = deref_iter(name_it);
      return {x, names.at(x)};
    }

    // increment
    LabeledNodeIter& operator++()
    {
      ++name_it;
      return *this;
    }

    //! post-increment
    LabeledNodeIter operator++(int)
    {
      LabeledNodeIter result(names, name_it);
      ++(*this);
      return result;
    }

    bool operator==(const LabeledNodeIter& it)
    {
      return it.name_it == name_it;
    }

    bool operator!=(const LabeledNodeIter& it)
    {
      return it.name_it != name_it;
    }
    LabeledNodeIter& operator=(const LabeledNodeIter& it)
    {
      name_it = it.name_it;
      return *this;
    }
  };

  template<typename _IterType = IndexVec::const_iterator, class NameContainer = NameVec>
  class LabeledNodeIterFactory
  {
    const NameContainer& names;
    const _IterType it_begin;
    const _IterType it_end;
  public:
    using IterType = _IterType;

    LabeledNodeIterFactory(const NameContainer& _names, const IterType& _begin, const IterType& _end):
      names(_names), it_begin(_begin), it_end(_end) 
    {
      //std::cout << "constructed labeled-node-iter-fac with begin = "<<_begin<<" & end = "<<_end<<std::endl;
      //std::cout << "constructed labeled-node-iter-factory with names:"<<std::endl;
      //for(const auto& i: names) std::cout << i << " ";
      //std::cout << std::endl;
      //std::cout << "from "<<it_begin<<" to "<<it_end<<"("<<it_end-it_begin<<" items)"<<std::endl;
    }
    template<class _NodeContainer>
    LabeledNodeIterFactory(const NameContainer& _names, const _NodeContainer& c):
      names(_names), it_begin(c.begin()), it_end(c.end())
    {}

    LabeledNodeIter<IterType> begin() const
    {
      return LabeledNodeIter<IterType>(names, it_begin);
    }
    LabeledNodeIter<IterType> end() const
    {
      return LabeledNodeIter<IterType>(names, it_end);
    }
    
    size_t size() const
    {
      return it_end - it_begin;
    }

  };

  template<class _Names, class _NodeContainer, typename _IterType = IndexVec::const_iterator>
  LabeledNodeIterFactory<_IterType> get_labeled_nodes(const _Names& names, const _NodeContainer& nodes)
  {
    return {names, nodes};
  }
}
