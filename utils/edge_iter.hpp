
#pragma once

#include "trans_iter.hpp"
#include "iter_factory.hpp"
#include "types.hpp"
#include "tags.hpp"

namespace PT{

  template<std::IterableType AdjContainer, bool reverse = false>
  struct ProtoEdgeMaker {
    NodeDesc u;
    using Adj  = typename std::value_type_of_t<AdjContainer>;
    using Edge = typename Adj::Edge;
   
    template<class T> requires std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<Adj>>
    auto operator()(T&& adj) const {
      if constexpr(reverse)
        return Edge{reverse_edge_t(), u, std::forward<T>(adj)};
      else return Edge{u, std::forward<T>(adj)};
    }
  };
  template<class Adj> using EdgeMaker = ProtoEdgeMaker<Adj, false>;
  template<class Adj> using ReverseEdgeMaker = ProtoEdgeMaker<Adj, true>;


  template<std::IterableType AdjContainer, class _EdgeMaker = EdgeMaker<AdjContainer>>
  using InOutEdgeIterator = std::transforming_iterator<std::iterator_of_t<AdjContainer>, _EdgeMaker>;

  // make an edge container from a head and a container of tails
  template<std::IterableType _AdjContainer>
  using InEdgeIterator = InOutEdgeIterator<_AdjContainer, ReverseEdgeMaker<_AdjContainer>>;
  // make an edge container from a tail and a container of heads
  template<std::IterableType _AdjContainer>
  using OutEdgeIterator = InOutEdgeIterator<_AdjContainer>;


  // factories
  template<std::IterableType _AdjContainer, class BeginEndTransformation = void>
  using InEdgeFactory = std::IterFactory<InEdgeIterator<_AdjContainer>, BeginEndTransformation, std::iterator_of_t<_AdjContainer>>;
  template<std::IterableType _AdjContainer, class BeginEndTransformation = void>
  using OutEdgeFactory = std::IterFactory<OutEdgeIterator<_AdjContainer>, BeginEndTransformation, std::iterator_of_t<_AdjContainer>>;

  // convenience functions
  template<std::IterableType _AdjContainer, class BeginEndTransformation = void>
  auto make_inedge_factory(const NodeDesc u, _AdjContainer& c) {
    return InEdgeFactory<_AdjContainer, BeginEndTransformation>{std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u), // make first part of the auto_iter
      std::forward_as_tuple(std::end(c)) // make end-iterator of the auto_iter
    };
  }
  template<std::IterableType _AdjContainer, class BeginEndTransformation>
  auto make_inedge_factory(const NodeDesc u, _AdjContainer& c, BeginEndTransformation&& trans) {
    return InEdgeFactory<_AdjContainer, std::remove_cvref_t<BeginEndTransformation>>{
      std::forward<BeginEndTransformation>(trans),
      std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u),
      std::forward_as_tuple(std::end(c))
    };
  }

  template<std::IterableType _AdjContainer, class BeginEndTransformation = void>
  auto make_outedge_factory(const NodeDesc u, _AdjContainer& c) {
    return OutEdgeFactory<_AdjContainer, BeginEndTransformation>{std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u), // make first part of the auto_iter
      std::forward_as_tuple(std::end(c))  // make end-iterator of the auto_iter
    };
  }
  template<std::IterableType _AdjContainer, class BeginEndTransformation>
  auto make_outedge_factory(const NodeDesc u, _AdjContainer& c, BeginEndTransformation&& trans) {
    return OutEdgeFactory<_AdjContainer, std::remove_cvref_t<BeginEndTransformation>>{
      std::forward<BeginEndTransformation>(trans),
      std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u), // make first part of the auto_iter
      std::forward_as_tuple(std::end(c))  // make end-iterator of the auto_iter
    };
  }


}
