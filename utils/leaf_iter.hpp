
#pragma once

#include "utils.hpp"
#include "types.hpp"

namespace TC {

  class TreeLeafIter
  {
    const NameVec& names;

    IndexVec::const_iterator leaf_it;
  public:
    TreeLeafIter(const NameVec& _names):
      names(_names)
    {}

    TreeLeafIter(const NameVec& _names, const IndexVec::const_iterator& _it):
      names(_names), leaf_it(_it)
    {}

    // dereference
    LabeledVertex operator*() const
    {
      return {*leaf_it, names.at(*leaf_it)};
    }

    // increment
    TreeLeafIter& operator++()
    {
      ++leaf_it;
      return *this;
    }

    //! post-increment
    TreeLeafIter operator++(int)
    {
      TreeLeafIter result(names, leaf_it);
      ++(*this);
      return result;
    }

    bool operator==(const TreeLeafIter& it)
    {
      return it.leaf_it == leaf_it;
    }

    bool operator!=(const TreeLeafIter& it)
    {
      return it.leaf_it != leaf_it;
    }
    TreeLeafIter& operator=(const TreeLeafIter& it)
    {
      leaf_it = it.leaf_it;
      return *this;
    }
  };

  class TreeLeafIterFactory
  {
    const IndexVec& leaves;
    const NameVec& names;
  public:

    TreeLeafIterFactory(const IndexVec& _leaves, const NameVec& _names):
      leaves(_leaves), names(_names) {}

    TreeLeafIter begin()
    {
      return TreeLeafIter(names, leaves.begin());
    }
    TreeLeafIter end()
    {
      return TreeLeafIter(names, leaves.end());
    }

  };
}
