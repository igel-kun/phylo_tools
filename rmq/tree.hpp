#pragma once

#include <vector>

/**
 * Represents an n-ary tree with a node id and a vector of children.
 */
template<typename value_type>
class tree {

public:

  typedef typename std::vector<size_t>::difference_type index_type;

private:
  
  value_type _data;
  unsigned _id;
  std::vector<tree> _children;

public:

  tree(const unsigned id): _id(id) {}

  /**
   * Constructs a leaf node.
   */
  tree(const unsigned id, const value_type &data)
    : _data(data),
      _id(id),
      _children()
  {}

  // No vector copying allowed.
  tree(const value_type& data, const std::vector<tree> &c) = delete;

  //! Construct an internal node.
  tree(const unsigned id, const value_type &data, std::vector<tree> &&c)
    : _data(data),
      _id(id),
      _children(std::move(c))
  {}

  // Move constructor (marked noexcept so that vector will use it).
  tree(tree &&o) noexcept
    : _data(std::move(o._data)),
      _id(std::move(o._id)),
      _children(std::move(o._children))
  {}

  // No copying allowed.
  tree(const tree &o) = delete;

  // allow iterating through children via const_iterators
  typename std::vector<tree>::const_iterator cbegin() const
  {
    return _children.cbegin();
  }
  typename std::vector<tree>::const_iterator cend() const
  {
    return _children.cend();
  }

  // Move assignment.
  tree& operator=(tree &&o) {
    _data = std::move(o._data);
    _children = std::move(o._children);
    return *this;
  }

  // No copying assignment allowed.
  tree& operator=(const tree &o) = delete;

  // This node's data.
  const value_type &get_data() const { return _data; }
  
  // This node's id.
  unsigned get_id() const { return _id; }

  // A list of this node's children.
  std::vector<const tree*> children()  const {
    std::vector<const tree*> result;
    result.reserve(_children.size());
    for(const auto& c : _children) result.push_back(&c);
    return result;
  }
};

