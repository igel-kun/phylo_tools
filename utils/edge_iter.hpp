
#pragma once

#include "edge.hpp"

namespace PT{


  // make an edge container from a node and a container of nodes
  template<class _AdjContainer>
  class InOutEdgeIterator
  {
  public:
    using iterator = std::iterator_of_t<_AdjContainer>;
    using iterator_category = typename std::my_iterator_traits<iterator>::iterator_category;
    using Adjacency         = typename std::my_iterator_traits<iterator>::value_type;
    using AdjacencyRef      = typename std::my_iterator_traits<iterator>::reference;
    // NOTE: instead of copying the edge data around, we'd rather keep a reference!
    using Edge = PT::Edge<DataReference<DataFromAdjacency<Adjacency>>>;
    
    using value_type      = Edge;
    using pointer         = Edge*;
    using difference_type = ptrdiff_t;
    using reference       = Edge;
    using const_reference = const Edge;
  protected:
    Node u;
    iterator node_it;
  public:
  
    InOutEdgeIterator() {}
    InOutEdgeIterator(const Node _u, const iterator& _node_it):
      u(_u),
      node_it(_node_it)
    {}
    InOutEdgeIterator(const Node _u, _AdjContainer& _node_c):
      InOutEdgeIterator(_u, _node_c.begin())
    {}

    //! increment operator
    InOutEdgeIterator& operator++() { ++node_it; return *this; }
    InOutEdgeIterator operator++(int) { InOutEdgeIterator tmp(*this); ++(*this); return tmp; }
    InOutEdgeIterator& operator--() { --node_it; return *this; }
    InOutEdgeIterator operator--(int) { InOutEdgeIterator tmp(*this); --(*this); return tmp; }

    InOutEdgeIterator& operator+=(const difference_type diff) { node_it += diff; return *this; }
    InOutEdgeIterator& operator-=(const difference_type diff) { node_it -= diff; return *this; }
    InOutEdgeIterator operator+(const difference_type diff) { return {u, node_it + diff}; }
    InOutEdgeIterator operator-(const difference_type diff) { return {u, node_it - diff}; }

    // I don't know why C++ forces me to declare this, it should be declared implicitly...
    InOutEdgeIterator& operator=(const InOutEdgeIterator& op) = default; // { u = op.u; node_it = op.node_it; return *this; }

    bool operator==(const iterator& _node_it) const { return node_it == _node_it;}
    bool operator!=(const iterator& _node_it) const { return node_it != _node_it;}

    bool operator==(const InOutEdgeIterator& _it) const { return node_it == _it.node_it;}
    bool operator!=(const InOutEdgeIterator& _it) const { return node_it != _it.node_it;}
  };

  // make an edge container from a head and a container of tails
  template<class _AdjContainer>
  class InEdgeIterator : public InOutEdgeIterator<_AdjContainer>
  {
    using Parent = InOutEdgeIterator<_AdjContainer>;
    using Parent::u;
    using Parent::node_it;
  public:
    using Parent::Parent;
    using typename Parent::value_type;

    value_type operator*() const { return value_type(reverse_edge, u, node_it->first, node_it->second); }
  };

  // make an edge container from a head and a container of tails
  template<class _AdjContainer>
  class OutEdgeIterator : public InOutEdgeIterator<_AdjContainer>
  {
    using Parent = InOutEdgeIterator<_AdjContainer>;
    using Parent::u;
    using Parent::node_it;
  public:
    using Parent::Parent;
    using typename Parent::value_type;

    value_type operator*() const {
      return value_type(u, node_it->first, node_it->second);
    }
  };

  template<class _AdjContainer,
           template<class> class _Iterator>
  class EdgeIterFactory: public std::my_iterator_traits<_Iterator<_AdjContainer>>
  {
  public:
    using Adjacency       = typename std::my_iterator_traits<std::iterator_of_t<_AdjContainer>>::value_type;
    using iterator        = _Iterator<_AdjContainer>;
    using const_iterator  = _Iterator<const _AdjContainer>;
    using const_reference = typename iterator::const_reference;
    using Edge            = typename iterator::Edge;
  protected:
    const Node u;
    std::shared_ptr<_AdjContainer> c;

  public:
    // if constructed via a reference, do not destruct the object, if constructed via a pointer (à la "new _AdjContainer()"), do destruct after use
    EdgeIterFactory(const Node _u, _AdjContainer& _c): u(_u), c(&_c, std::NoDeleter()) {}
    EdgeIterFactory(const Node _u, _AdjContainer* const _c): u(_u), c(_c) {}
    EdgeIterFactory(const Node _u, _AdjContainer&& _c): u(new _AdjContainer(std::forward<_AdjContainer>(_u))), c(_c) {}
    
    iterator begin() { return {u, c->begin()}; }
    iterator end() { return {u, c->end()}; }
    const_iterator begin() const { return {u, c->begin()}; }
    const_iterator end() const { return {u, c->end()}; }

    bool empty() const { return c->empty(); }
    size_t size() const { return c->size(); }
  };

  template<class _AdjContainer = NodeSet>
  using InEdgeFactory = EdgeIterFactory<_AdjContainer, InEdgeIterator>;
  template<class _AdjContainer = NodeSet>
  using OutEdgeFactory = EdgeIterFactory<_AdjContainer, OutEdgeIterator>;



  // go one step further and enumerate edges from a mapping of nodes to nodes


  //! an iterator over a mapping Node->Successors enumerating Edges
  template<class _Map,
           template<class> class _EdgeIterator>
  class EdgeMapIterator //: public std::my_iterator_traits<_EdgeIterator<std::copy_cv_t<_Map, typename _Map::mapped_type>>>
  {
    using Map = _Map;
    using AdjContainer = std::copy_cv_t<_Map, typename _Map::mapped_type>;
    using MapIterator = std::iterator_of_t<_Map>;
    using NodeIterator = std::iterator_of_t<AdjContainer>;
  public:

    using EdgeIterator = _EdgeIterator<AdjContainer>;
    using Edge = typename EdgeIterator::Edge;

    using value_type      = typename EdgeIterator::value_type;
    using pointer         = typename EdgeIterator::pointer;
    using difference_type = typename EdgeIterator::value_type;
    using reference       = typename EdgeIterator::reference;
    using const_reference = typename EdgeIterator::const_reference;
    using iterator_category = typename EdgeIterator::iterator_category;

  protected:
    Map& nc_map;
    MapIterator node_it;
    EdgeIterator out_it;

    // skip over map-items for which the container is empty
    void skip_empty()
    {
      while(node_it != nc_map.end() ? node_it->second.empty() : false) ++node_it;
    }

  public:
    EdgeMapIterator(_Map& _nc_map, const MapIterator& _node_it):
      nc_map(_nc_map),
      node_it(_node_it)
    {
      skip_empty();
      if(node_it != nc_map.end())
        out_it = EdgeIterator(node_it->first, node_it->second);
    }

    EdgeMapIterator(_Map& _nc_map): EdgeMapIterator(_nc_map, _nc_map.begin()) {}

    EdgeMapIterator(const EdgeMapIterator& _it):
      nc_map(_it.nc_map),
      node_it(_it.node_it),
      out_it(_it.out_it)
    {}

    const_reference operator*() const { return *out_it; }
    pointer operator->() const { return EdgeIterator::operator->(out_it); }

    //! increment operator
    EdgeMapIterator& operator++()
    {
      ++out_it;
      if(out_it == node_it->second.end()){
        ++node_it;
        skip_empty();
        if(node_it != nc_map.end())
          out_it = EdgeIterator(node_it->first, node_it->second);
      }
      return *this;
    }

    //! post-increment
    EdgeMapIterator operator++(int)
    {
      EdgeMapIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    bool operator==(const EdgeMapIterator& it) const 
    {
      if(!valid() && !it.valid()) return true;
      if(node_it != it.node_it) return false;
      return (out_it == it.out_it);
    }
    bool operator!=(const EdgeMapIterator& it) const { return !operator==(it); }

    bool valid() const { return node_it != nc_map.end(); }
    operator bool() const { return valid(); }
  };

  template<class _Map, template<class> class _Iterator>
  class EdgeMapIterFactory: public std::my_iterator_traits<EdgeMapIterator<_Map, _Iterator>>
  {
  protected:
    std::shared_ptr<_Map> map;
  public:
    using iterator        = EdgeMapIterator<_Map, _Iterator>;
    using const_iterator  = iterator;
    using const_reference = typename iterator::const_reference;

    // if constructed via a reference, do not destruct the object, if constructed via a pointer (à la "new _Map()"), do destruct after use
    EdgeMapIterFactory(_Map& _map): map(&_map, std::NoDeleter()) {}
    EdgeMapIterFactory(_Map* const _map): map(_map) {}
    EdgeMapIterFactory(_Map&& _map): map(new _Map(std::forward<_Map>(_map))) {}

    iterator begin() { return {*map, map->begin()}; }
    iterator end() { return {*map, map->end()}; }
    const_iterator begin() const { return {*map, map->begin()}; }
    const_iterator end() const { return {*map, map->end()}; }

    bool empty() const { return map->empty(); }
    bool size() const { return map->size(); }
  };

  template<class _Map>
  using OutEdgeMapIterFactory = EdgeMapIterFactory<_Map, OutEdgeIterator>;
  template<class _Map>
  using InEdgeMapIterFactory = EdgeMapIterFactory<_Map, InEdgeIterator>;


  
  template<class Q, template<class> class T>
  std::ostream& operator<<(std::ostream& os, const EdgeIterFactory<Q,T>& x) { return std::_print(os, x); }
  template<class Q, template<class> class T>
  std::ostream& operator<<(std::ostream& os, const EdgeMapIterFactory<Q,T>& x) { return std::_print(os, x); }


}
