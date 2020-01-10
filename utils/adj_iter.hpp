
#pragma once


namespace PT{

  //! an iterator over a mapping Node->Successors enumerating Edges
  template<class _Map,
           class _MapIterator = typename _Map::iterator,
           class _NodeContainer = typename _Map::mapped_type,
           class _Iterator = typename _NodeContainer::iterator>
  class AdjacencyIterator
  {
    _Map& nc_map;
    _MapIterator node_it;
    OutEdgeIterator<_NodeContainer> out_it;
  public:
    using Node = typename std::iterator_traits<_Iterator>::reference;
    using Edge = std::pair<Node, Node>;

    AdjacencyIterator(_Map& _nc_map, const _MapIterator& _node_it):
      nc_map(_nc_map),
      node_it(_node_it)
    {
      if(node_it != nc_map.end())
        out_it = OutEdgeIterator<_NodeContainer>(node_it->first, node_it->second);
    }

    AdjacencyIterator(_Map& _nc_map):
      AdjacencyIterator(_nc_map, _nc_map.begin())
    {}

    AdjacencyIterator(const AdjacencyIterator& _it):
      nc_map(_it.nc_map),
      node_it(_it.node_it),
      out_it(_it.out_it)
    {}

    const Edge operator*() const { return {node_it->first, *out_it}; }

    //! increment operator
    AdjacencyIterator& operator++()
    {
      ++out_it;
      if(out_it == node_it->end()){
        ++node_it;
        if(node_it != nc_map.end())
          out_it = node_it->begin();
      }
      return *this;
    }

    //! post-increment
    AdjacencyIterator operator++(int)
    {
      AdjacencyIterator tmp(*this);
      ++(*this);
      return tmp;
    }

    bool operator==(const AdjacencyIterator& it) const 
    {
      if(!valid() && !it.valid()) return true;
      if(node_it != it.node_it) return false;
      return (out_it == it.out_it);
    }
    bool operator!=(const AdjacencyIterator& it) const { return !operator==(it); }

    bool valid() const { return node_it != nc_map.end(); }
    operator bool() const { return valid(); }
  };

  template<class _Map,
           class _MapIterator = typename _Map::const_iterator,
           class _NodeContainer = typename _Map::mapped_type,
           class _Iterator = typename _NodeContainer::const_iterator>
  using AdjacencyConstIterator = AdjacencyIterator<_Map, _MapIterator, _NodeContainer, _Iterator>;


  template<class _Map,
           class _Iterator>
  class AdjacencyIterFactory
  {
  protected:
    _Map& node_to_succ;
  public:
    AdjacencyIterFactory(_Map& _node_to_succ):
      node_to_succ(_node_to_succ)
    {}
    _Iterator begin() { return _Iterator(node_to_succ, node_to_succ.begin()); }
    _Iterator end() { return _Iterator(node_to_succ, node_to_succ.end()); }
  };



}
