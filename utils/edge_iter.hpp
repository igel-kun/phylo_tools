
#pragma once

#include "iter_factory.hpp"
#include "edge.hpp"

namespace PT{

  struct EdgeMaker {
    template<class Adj>
    static EdgeFromAdjacency<Adj> make_edge(const Node u, Adj&& adj) { return {u, std::forward<Adj>(adj)}; }
  };
  struct ReverseEdgeMaker {
    template<class Adj>
    static EdgeFromAdjacency<Adj> make_edge(const Node u, Adj&& adj) { return {reverse_edge, u, std::forward<Adj>(adj)}; }
  };

  // make an edge container from a node and a container of nodes; makes edges with the function object EdgeMaker
  template<class _AdjContainer, class _EdgeMaker = EdgeMaker>
  class InOutEdgeIterator
  {
  public:
    using iterator = std::iterator_of_t<_AdjContainer>;
    using iterator_category = typename std::my_iterator_traits<iterator>::iterator_category;
    using Adjacency         = typename std::my_iterator_traits<iterator>::value_type;
    using AdjacencyRef      = typename std::my_iterator_traits<iterator>::reference;
    // NOTE: instead of copying the edge data around, we'd rather keep a reference!
    using Edge      = PT::Edge<DataReference<DataFromAdjacency<Adjacency>>>;
    using ConstEdge = PT::Edge<DataReference<DataFromAdjacency<const Adjacency>>>;
    
    using value_type      = Edge;
    using difference_type = ptrdiff_t;
    using reference       = Edge;
    using const_reference = ConstEdge;
    using pointer         = std::self_deref<Edge>;
    using const_pointer   = std::self_deref<ConstEdge>;

  protected:
    iterator node_it;
    Node u;
  public:
    InOutEdgeIterator() {}
    InOutEdgeIterator(const iterator& _node_it, const Node _u): node_it(_node_it), u(_u) {}
    InOutEdgeIterator(_AdjContainer& _node_c, const Node _u): InOutEdgeIterator(_node_c.begin(), _u) {}

    //! increment operator
    InOutEdgeIterator& operator++() { ++node_it; return *this; }
    InOutEdgeIterator operator++(int) { InOutEdgeIterator tmp(*this); ++(*this); return tmp; }
    InOutEdgeIterator& operator--() { --node_it; return *this; }
    InOutEdgeIterator operator--(int) { InOutEdgeIterator tmp(*this); --(*this); return tmp; }

    InOutEdgeIterator& operator+=(const difference_type diff) { std::advance(node_it, diff); return *this; }
    InOutEdgeIterator& operator-=(const difference_type diff) { std::advance(node_it, -diff); return *this; }
    InOutEdgeIterator operator+(const difference_type diff) { return {u, std::next(node_it, diff)}; }
    InOutEdgeIterator operator-(const difference_type diff) { return {u, std::next(node_it, diff)}; }

    //InOutEdgeIterator& operator=(const InOutEdgeIterator& op) = default; // { u = op.u; node_it = op.node_it; return *this; }

    bool operator==(const iterator& _node_it) const { return node_it == _node_it;}
    bool operator!=(const iterator& _node_it) const { return node_it != _node_it;}
    bool operator==(const InOutEdgeIterator& _it) const { return node_it == _it.node_it;}
    bool operator!=(const InOutEdgeIterator& _it) const { return node_it != _it.node_it;}

    const_reference operator*() const { return *(operator->()); }
    reference operator*()             { return *(operator->()); }
    const_pointer operator->() const { return _EdgeMaker::make_edge(u, *node_it); }
    pointer operator->()             { return _EdgeMaker::make_edge(u, *node_it); }

  };

  // make an edge container from a head and a container of tails
  template<class _AdjContainer>
  using InEdgeIterator = InOutEdgeIterator<_AdjContainer, ReverseEdgeMaker>;
  // make an edge container from a tail and a container of heads
  template<class _AdjContainer>
  using OutEdgeIterator = InOutEdgeIterator<_AdjContainer>;

  // factories
  template<class _AdjContainer, template<class> class _Iterator>
  using EdgeIterFactory = std::IterTplIterFactory<_AdjContainer, Node, std::BeginEndIters<_AdjContainer, false, _Iterator>, _Iterator>;
  template<class _AdjContainer>
  using InEdgeFactory = EdgeIterFactory<_AdjContainer, InEdgeIterator>;
  template<class _AdjContainer>
  using OutEdgeFactory = EdgeIterFactory<_AdjContainer, OutEdgeIterator>;

  // go one step further and enumerate edges from a mapping of nodes to adjacencies (nodes with edge-data)

  //! an iterator over a mapping Node->Successors enumerating Edges
  template<class _Map, template<class> class _EdgeIterator>
  class EdgeMapIterator //: public std::my_iterator_traits<_EdgeIterator<std::copy_cv_t<_Map, typename _Map::mapped_type>>>
  {
    using Map = _Map;
    using AdjContainer = std::copy_cv_t<_Map, typename _Map::mapped_type>;
    using MapIterator  = std::iterator_of_t<_Map>;
    using NodeIterator = std::iterator_of_t<AdjContainer>;
  public:

    using EdgeIterator = _EdgeIterator<AdjContainer>;
    using Edge = typename EdgeIterator::Edge;

    using value_type      = typename EdgeIterator::value_type;
    using pointer         = typename EdgeIterator::pointer;
    using difference_type = typename EdgeIterator::difference_type;
    using reference       = typename EdgeIterator::reference;
    using const_reference = typename EdgeIterator::const_reference;
    //using iterator_category = typename EdgeIterator::iterator_category;
    // NOTE: make more efficient depending on the iterator_categories of _Map and EdgeIterator
    using iterator_category = typename std::my_iterator_traits<MapIterator>::iterator_category;

  protected:
    Map& nc_map;
    MapIterator node_it;
    EdgeIterator out_it;

    // skip over map-items for which the container is empty
    void skip_empty() { while(node_it != nc_map.end() ? node_it->second.empty() : false) ++node_it; }
    void skip_empty_rev() {
      while(node_it != nc_map.begin() ? node_it->second.empty() : false) --node_it;
      if(node_it->second.empty()) --node_it;
    }

  public:
    EdgeMapIterator(_Map& _nc_map, const MapIterator& _node_it):
      nc_map(_nc_map),
      node_it(_node_it)
    {
      if(node_it != nc_map.end()){
        skip_empty();
        if(node_it != nc_map.end())
          out_it = EdgeIterator(node_it->second, node_it->first);
      }
    }

    EdgeMapIterator(_Map& _nc_map): EdgeMapIterator(_nc_map, _nc_map.begin()) {}


    const_reference operator*() const { return *out_it; }
    pointer operator->() const { return out_it.operator->(); }

    //! increment operator
    //NOTE: incrementing the end-iterator will crash
    EdgeMapIterator& operator++()
    {
      ++out_it;
      if(out_it == node_it->second.end()){
        ++node_it;
        skip_empty();
        if(node_it != nc_map.end())
          out_it = EdgeIterator(node_it->second, node_it->first);
      }
      return *this;
    }

    //! decrement operator
    //NOTE: this does little to no error checking, so be sure there is an item to go to!
    EdgeMapIterator& operator--()
    {
      if(node_it != nc_map.end()) {
        if(out_it == node_it->second.begin()){
          --node_it;
          skip_empty_rev();
          out_it = EdgeIterator(std::prev(node_it->second.end()), node_it->first);
        } else --out_it;
      } else {
        node_it = std::prev(nc_map.end());
        skip_empty_rev();
        out_it = std::prev(node_it->second.end());
      }
      return *this;
    }

    //! post in-/decrement
    EdgeMapIterator operator++(int) { EdgeMapIterator tmp(*this); ++(*this); return tmp; }
    EdgeMapIterator operator--(int) { EdgeMapIterator tmp(*this); --(*this); return tmp; }


    //! random-access increment operator
    //NOTE: incrementing the end-iterator will crash
    EdgeMapIterator& operator+=(difference_type x)
    {
      if(x < 0) return operator-=(-x);

      while(x){
        const difference_type items_left = std::distance(out_it, node_it->second.end());
        if(x >= items_left){
          x -= items_left;
          ++node_it;
          skip_empty();
          if(node_it != nc_map.end())
            out_it = EdgeIterator(node_it->second, node_it->first);
          else
            return *this;
        } else {
          std::advance(out_it, x);
          return *this;
        }
      }
      return *this;
    }

    //! random-access decrement operator
    EdgeMapIterator& operator-=(difference_type x)
    {
      if(x < 0) return operator+=(-x);
      if(x == 0) return *this;
      if(node_it == std::end(nc_map)){
        --(*this);
        --x;
      }
      // at this point, we either crashed (if nc_map.empty()) or out_it points to a valid item... let's keep it that way
      while(x) {
        const difference_type items_before = std::distance(std::begin(node_it->second), out_it);
        if(x > items_before) {
          x -= items_before + 1;
          --node_it;
          skip_empty_rev();
          out_it = std::prev(node_it->second.end());
        } else {
          std::advance(out_it, -x);
          return *this;
        }
      }
      return *this;
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


  // for EdgeMapIterators, we need to slightly modify how the factory constructs them...
  template<class _Map, template<class> class _Iterator>
  struct BeginEndEdgeMapIters
  {
    using iterator = EdgeMapIterator<_Map, _Iterator>;
    using const_iterator = EdgeMapIterator<const _Map, _Iterator>;
    static iterator begin(std::remove_cv_t<_Map>& c) { return {c, std::begin(c) }; }
    static iterator end(std::remove_cv_t<_Map>& c)   { return {c, std::end(c) }; }
    static const_iterator begin(const _Map& c)       { return {c, std::begin(c) }; }
    static const_iterator end(const _Map& c)         { return {c, std::end(c) }; }
  };


  // successor-maps cannot know à priori how many edges are stored within them, so we take a reference to an external size_t
  template<class _Map, template<class> class _Iterator>
  class EdgeMapIterFactory: public std::IterFactory<_Map, void, BeginEndEdgeMapIters<_Map, _Iterator>>
  {
    using Parent = std::IterFactory<_Map, void, BeginEndEdgeMapIters<_Map, _Iterator>>;
    const size_t& _size;
  public:

    // construct with a size_t reference pointing to the size of the data structure (number edges)
    template<class... Args>
    EdgeMapIterFactory(const size_t& __size, Args&&... args):
      Parent(std::forward<Args>(args)...), _size(__size)
    {}

    size_t size() const { return _size; }
    bool  empty() const { return _size == 0; }
  };


  template<class _Map>
  using OutEdgeMapIterFactory = EdgeMapIterFactory<_Map, OutEdgeIterator>;
  template<class _Map>
  using InEdgeMapIterFactory = EdgeMapIterFactory<_Map, InEdgeIterator>;
}
