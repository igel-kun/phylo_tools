
#pragma once

#include "trans_iter.hpp"
#include "iter_factory.hpp"
#include "edge.hpp"

namespace PT{

  template<std::IterableType AdjContainer, bool reverse = false>
  struct ProtoEdgeMaker {
    NodeDesc u;
    using Adj  = typename std::value_type_of_t<AdjContainer>;
    using Edge = typename Adj::Edge;
    
    Edge operator()(Adj&& adj) const {
      if constexpr(reverse)
        return {reverse_edge_t(), u, std::move<Adj>(adj)};
      else return {u, std::move<Adj>(adj)};
    }
    Edge operator()(const Adj& adj) const {
      if constexpr(reverse)
        return {reverse_edge_t(), u, adj};
      else return {u, adj};
    }

  };
  template<class Adj> using EdgeMaker = ProtoEdgeMaker<Adj, false>;
  template<class Adj> using ReverseEdgeMaker = ProtoEdgeMaker<Adj, true>;


  template<std::IterableType AdjContainer, class _EdgeMaker = EdgeMaker<AdjContainer>>
  using InOutEdgeIterator = std::transforming_iterator<std::iterator_of_t<AdjContainer>,
                                                       typename _EdgeMaker::Edge,
                                                       _EdgeMaker>;
/*
  // make an edge container from a node and a container of nodes; makes edges with the function object EdgeMaker
  template<std::IterableType _AdjContainer, class _EdgeMaker = EdgeMaker>
  class InOutEdgeIterator
  {
  public:
    using iterator          = std::iterator_of_t<_AdjContainer>;
    using const_iterator    = std::iterator_of_t<const _AdjContainer>;
    using iterator_category = typename std::my_iterator_traits<iterator>::iterator_category;
    using Adjacency         = typename std::my_iterator_traits<iterator>::value_type;
    using AdjacencyRef      = typename std::my_iterator_traits<iterator>::reference;
    using Edge      = typename PT::EdgeFromNode<typename Adjacency::Node>;
    using ConstEdge = const Edge;
    
    using value_type      = Edge;
    using difference_type = ptrdiff_t;
    using reference       = Edge;
    using const_reference = ConstEdge;
    using pointer         = std::self_deref<Edge>;
    using const_pointer   = std::self_deref<ConstEdge>;

  protected:
    NodeDesc u;
    iterator node_it;
  public:
    InOutEdgeIterator() {}
    template<class First, class... Args> requires (!std::ContainerType<First>)
    InOutEdgeIterator(const NodeDesc _u, First&& first, Args&&... args): u(_u), node_it(std::forward<First>(first), std::forward<Args>(args)...) {}
    InOutEdgeIterator(const NodeDesc _u, _AdjContainer& _node_c): InOutEdgeIterator(_u, _node_c.begin()) {}

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
*/
  // make an edge container from a head and a container of tails
  template<std::IterableType _AdjContainer>
  using InEdgeIterator = InOutEdgeIterator<_AdjContainer, ReverseEdgeMaker<_AdjContainer>>;
  // make an edge container from a tail and a container of heads
  template<std::IterableType _AdjContainer>
  using OutEdgeIterator = InOutEdgeIterator<_AdjContainer>;


  // factories
  template<std::IterableType _AdjContainer, class BeginEndTransformation = void>
  using InEdgeFactory = std::IterFactory<InEdgeIterator<_AdjContainer>, BeginEndTransformation>;
  template<std::IterableType _AdjContainer, class BeginEndTransformation = void>
  using OutEdgeFactory = std::IterFactory<OutEdgeIterator<_AdjContainer>, BeginEndTransformation>;

}
