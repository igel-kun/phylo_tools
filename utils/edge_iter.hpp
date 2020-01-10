
#pragma once

#include "edge.hpp"

namespace PT{


  // make an edge container from a tail and a list of heads
  template<class _Edge = Edge<>,
           class _NodeContainer = std::unordered_set<typename _Edge::Node>,
           class _Iterator = typename _NodeContainer::iterator>
  class InOutEdgeIterator
  {
  public:
    using AdjRef = typename std::iterator_traits<_Iterator>::reference;
    using iterator = _Iterator;
    using Edge = _Edge;
    using Node = typename _Edge::Node;
  protected:
    Node u;
    _Iterator node_it;
  public:

     InOutEdgeIterator(const Node _u, const _Iterator& _node_it):
      u(_u),
      node_it(_node_it)
    {}
    InOutEdgeIterator(const Node _u, const _NodeContainer& _node_c):
      InOutEdgeIterator(_u, _node_c.begin())
    {}

    //! increment operator
    InOutEdgeIterator& operator++()
    {
      ++node_it;
      return *this;
    }

    //! post-increment
    InOutEdgeIterator operator++(int)
    {
      InOutEdgeIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    // I don't know why C++ forces me to declare this, it should be declared implicitly...
    InOutEdgeIterator& operator=(const InOutEdgeIterator& op)
    {
      u = op.u;
      node_it = op.node_it;
      return *this;
    }

    bool operator==(const _Iterator& _node_it) const { return node_it == _node_it;}
    bool operator!=(const _Iterator& _node_it) const { return node_it != _node_it;}

    bool operator==(const InOutEdgeIterator& _it) const { return node_it == _it.node_it;}
    bool operator!=(const InOutEdgeIterator& _it) const { return node_it != _it.node_it;}
  };

  // make an edge container from a head and a list of tails
  template<class _Edge = Edge<>,
           class _NodeContainer = std::unordered_set<typename _Edge::Node>,
           class _Iterator = typename _NodeContainer::iterator>
  class InEdgeIterator : public InOutEdgeIterator<_Edge, _NodeContainer, _Iterator>
  {
    using Parent = InOutEdgeIterator<_Edge, _NodeContainer, _Iterator>;
    using Parent::u;
    using Parent::node_it;
  public:
    using Parent::Parent;

    _Edge operator*() const { return {*node_it, u}; }
  };

  // make an edge container from a head and a list of tails
  template<class _Edge = Edge<>,
           class _NodeContainer = std::unordered_set<typename _Edge::Node>,
           class _Iterator = typename _NodeContainer::iterator>
  class OutEdgeIterator : public InOutEdgeIterator<_Edge, _NodeContainer, _Iterator>
  {
    using Parent = InOutEdgeIterator<_Edge, _NodeContainer, _Iterator>;
    using Parent::u;
    using Parent::node_it;
  public:
    using Parent::Parent;

    _Edge operator*() const { return {u, *node_it}; }
  };

  template<class _Edge = Edge<>,
           class _NodeContainer = std::unordered_set<typename _Edge::Node>,
           class _Iterator = typename _NodeContainer::const_iterator>
  using InEdgeConstIterator = InEdgeIterator<_Edge, const _NodeContainer, _Iterator>;
  
  template<class _Edge = Edge<>,
           class _NodeContainer = std::unordered_set<typename _Edge::Node>,
           class _Iterator = typename _NodeContainer::const_iterator>
  using OutEdgeConstIterator = OutEdgeIterator<_Edge, const _NodeContainer, _Iterator>;

  template<class _Container,
           class _Iterator,
           class _ConstIterator = _Iterator>
  class EdgeIterFactory
  {
  public:
    using Node = typename _Container::value_type;
    using iterator = _Iterator;
    using const_iterator = _ConstIterator;
  protected:
    const bool copy_container;
    const Node u;
    _Container* c;

  public:

    EdgeIterFactory(const Node _u, _Container& _c, const bool _copy_container = false):
      copy_container(_copy_container),
      u(_u),
      // if _copy_container is true, then make a copy of the given container (to use with temporary containers)
      c(_copy_container ? new _Container(_c) : &_c)
    {}
    
    // construct empty factory
    EdgeIterFactory():
      copy_container(true),
      u(),
      c(new _Container())
    {}

    ~EdgeIterFactory()
    {
      // if we allocated our own copy of the container, then delete it when we're done
      if(copy_container) delete c;
    }

    iterator begin() { return {u, c->begin()}; }
    iterator end() { return {u, c->end()}; }
    const_iterator begin() const { return {u, c->begin()}; }
    const_iterator end() const { return {u, c->end()}; }
  };



  template<class _Edge = Edge<>, class _NodeContainer = std::unordered_set<typename _Edge::Node>>
  using InEdgeFactory = EdgeIterFactory<_NodeContainer, InEdgeIterator<_Edge, _NodeContainer>, InEdgeConstIterator<_Edge, _NodeContainer>>;
  template<class _Edge = Edge<>, class _NodeContainer = std::unordered_set<typename _Edge::Node>>
  using OutEdgeFactory = EdgeIterFactory<_NodeContainer, OutEdgeIterator<_Edge, _NodeContainer>, OutEdgeConstIterator<_Edge, _NodeContainer>>;
  template<class _Edge = Edge<>, class _NodeContainer = std::unordered_set<typename _Edge::Node>>
  using InEdgeConstFactory = EdgeIterFactory<const _NodeContainer, InEdgeConstIterator<_Edge, _NodeContainer>>;
  template<class _Edge = Edge<>, class _NodeContainer = std::unordered_set<typename _Edge::Node>>
  using OutEdgeConstFactory = EdgeIterFactory<const _NodeContainer, OutEdgeConstIterator<_Edge, _NodeContainer>>;

  template<class _Container,
           class _Iterator,
           class _ConstIterator> 
  std::ostream& operator<<(std::ostream& os, const EdgeIterFactory<_Container, _Iterator, _ConstIterator>& fac)
  {
    for(const auto& i: fac) os << i << ' ';
    return os;
  }

}
