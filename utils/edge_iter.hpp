
#pragma once

#include "trans_iter.hpp"
#include "iter_factory.hpp"
#include "types.hpp"
#include "tags.hpp"

namespace PT{

  template<mstd::IterableType AdjContainer, bool reverse = false>
  struct ProtoEdgeMaker {
    NodeDesc u;
    using Adj  = typename mstd::value_type_of_t<AdjContainer>;
    using Edge = typename Adj::Edge;
   
    template<class T> requires std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<Adj>>
    auto operator()(T&& adj) const {
      if constexpr(reverse)
        return Edge{reverse_edge_tag(), u, std::forward<T>(adj)};
      else return Edge{u, std::forward<T>(adj)};
    }
  };
  template<class Adj> using EdgeMaker = ProtoEdgeMaker<Adj, false>;
  template<class Adj> using ReverseEdgeMaker = ProtoEdgeMaker<Adj, true>;


  template<mstd::IterableType AdjContainer, class EdgeMaker_ = EdgeMaker<AdjContainer>>
  using InOutEdgeIterator = mstd::transforming_iterator<mstd::iterator_of_t<AdjContainer>, EdgeMaker_>;

  // make an edge container from a head and a container of tails
  template<mstd::IterableType AdjContainer_>
  using InEdgeIterator = InOutEdgeIterator<AdjContainer_, ReverseEdgeMaker<AdjContainer_>>;
  // make an edge container from a tail and a container of heads
  template<mstd::IterableType AdjContainer_>
  using OutEdgeIterator = InOutEdgeIterator<AdjContainer_>;


  // factories
  template<mstd::IterableType AdjContainer_, class BeginEndTransformation = void>
  using InEdgeFactory = mstd::IterFactory<InEdgeIterator<AdjContainer_>, BeginEndTransformation, mstd::iterator_of_t<AdjContainer_>>;
  template<mstd::IterableType AdjContainer_, class BeginEndTransformation = void>
  using OutEdgeFactory = mstd::IterFactory<OutEdgeIterator<AdjContainer_>, BeginEndTransformation, mstd::iterator_of_t<AdjContainer_>>;

  // convenience functions
  template<mstd::IterableType AdjContainer_, class BeginEndTransformation = void>
  auto make_inedge_factory(const NodeDesc u, AdjContainer_& c) {
    return InEdgeFactory<AdjContainer_, BeginEndTransformation>{std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u), // make first part of the auto_iter
      std::forward_as_tuple(std::end(c)) // make end-iterator of the auto_iter
    };
  }
  template<mstd::IterableType AdjContainer_, class BeginEndTransformation>
  auto make_inedge_factory(const NodeDesc u, AdjContainer_& c, BeginEndTransformation&& trans) {
    return InEdgeFactory<AdjContainer_, std::remove_cvref_t<BeginEndTransformation>>{
      std::forward<BeginEndTransformation>(trans),
      std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u),
      std::forward_as_tuple(std::end(c))
    };
  }

  template<mstd::IterableType AdjContainer_, class BeginEndTransformation = void>
  auto make_outedge_factory(const NodeDesc u, AdjContainer_& c) {
    return OutEdgeFactory<AdjContainer_, BeginEndTransformation>{std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u), // make first part of the auto_iter
      std::forward_as_tuple(std::end(c))  // make end-iterator of the auto_iter
    };
  }
  template<mstd::IterableType AdjContainer_, class BeginEndTransformation>
  auto make_outedge_factory(const NodeDesc u, AdjContainer_& c, BeginEndTransformation&& trans) {
    return OutEdgeFactory<AdjContainer_, std::remove_cvref_t<BeginEndTransformation>>{
      std::forward<BeginEndTransformation>(trans),
      std::piecewise_construct,
      std::forward_as_tuple(std::begin(c), u), // make first part of the auto_iter
      std::forward_as_tuple(std::end(c))  // make end-iterator of the auto_iter
    };
  }


}
