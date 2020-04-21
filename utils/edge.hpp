#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "pair_iter.hpp"
#include <queue>

namespace PT{

  struct reverse_edge_tag {};
  constexpr reverse_edge_tag reverse_edge = reverse_edge_tag();

  template<class _Data>
  struct __Adjacency: public std::pair<Node, _Data>
  {
    using Parent = std::pair<Node, _Data>;
    using Parent::Parent;
    using Data = _Data;

    __Adjacency(const __Adjacency& adj) : Parent(adj.first, adj.second) {}
    __Adjacency(const Node u, Data& d) : Parent(u, d) {}

    // construct an adjacency whose data is a reference to X from an adjacency whose data is X
    // NOTE: std::enable_if may only depend on immediate template parameters of the function; depending on the class template parameter is not SFINAE
    template<class __Data, class = std::enable_if_t<std::is_same_v<Data, const __Data&>>>
    __Adjacency(const __Adjacency<__Data>& adj) : Parent(adj.first, adj.second) {}
    template<class __Data, class = std::enable_if_t<std::is_same_v<Data, __Data&>>>
    __Adjacency(__Adjacency<__Data>& adj) : Parent(adj.first, adj.second) {}

    // access the adjacency's data
    Data& data() { return Parent::second; }
    const Data& data() const { return Parent::second; }

    // Adjacencies can be cast to their Node
    operator Node&() { return Parent::first; }
    operator const Node&() const { return Parent::first; }
  };

  // an Adjacency with void data is a Node
  template<class _Data>
  using Adjacency_t = std::conditional_t<std::is_same_v<_Data, void>, Node, __Adjacency<_Data>>;


  // an edge is a node (head) with another node (->tail)
  template<class _Adjacency>
  struct AbstractEdge: public std::pair<Node, _Adjacency>
  {
    using Parent = std::pair<Node, _Adjacency>;
    using Adjacency = _Adjacency;
    using Parent::Parent;
    
    // construct an edge from a node and whatever is needed to construct an adjacency (for example, an adjacency)
    template <class... Args> AbstractEdge(const Node u, Args&&... args):
      Parent(std::piecewise_construct, std::make_tuple(u), std::forward_as_tuple<Args...>(args...))
    {}
    // construct the reverse of an edge in a similar manner as above
    template <class... Args> AbstractEdge(const reverse_edge_tag tag, const Node u, Args&&... args):
      AbstractEdge(u, std::forward<Args>(args)...)
    { reverse(); }
    // construct from edge with different data; for example, from a ReverseEdge (whose data is a reference)
    template<class _Data>
    AbstractEdge(const AbstractEdge<_Data>& other):
      Parent(static_cast<typename Parent::first_type>(other.first), static_cast<typename Parent::second_type>(other.second))
    {}

    Node& head() { return this->second; }
    Node& tail() { return this->first; }
    const Node& head() const { return this->second; }
    const Node& tail() const { return this->first; }

    // reverse the edge - to reverse, cast the Adjacency to Node&
    void reverse() { std::swap(this->first, static_cast<Node&>(this->second)); }

    bool operator<(const AbstractEdge& e) const
    {
      if(this->second == e.second)
        return this->first < e.first;
      else 
        return this->second < e.second;
    }

    Adjacency& get_adjacency() { return Parent::second; }
    const Adjacency& get_adjacency() const { return Parent::second; }

    std::ostream& print(std::ostream& os) const { return os << Parent(*this); }
  };

  template<class _Data = void>
  struct Edge: public AbstractEdge<Adjacency_t<_Data>>
  {
    using Parent = AbstractEdge<Adjacency_t<_Data>>;
    using Parent::Parent;
    using Parent::head;
    using Parent::tail;
    using Data = _Data;
    using Adjacency = typename Parent::Adjacency;
 
    Data& data() { return Parent::get_adjacency().data(); }
    const Data& data() const { return Parent::get_adjacency().data(); }

    std::ostream& print(std::ostream& os) const
    { return os << "("<<tail()<<","<<head()<<"):"<<data(); }
  };
 
  template<>
  class Edge<void>: public AbstractEdge<Node>
  { public: using AbstractEdge<Node>::AbstractEdge; };
/*
  // enable default template argument
  template<class T = void>
  using Edge = _Edge<T>;
*/
  template<class _Data>
  std::ostream& operator<<(std::ostream& os, const Edge<_Data>& e) {
    return e.print(os);
  }

  template<class Data>
  Node head(const Edge<Data>& e) { return e.head(); }
  template<class Data>
  Node tail(const Edge<Data>& e) { return e.tail(); }
  template<class Data>
  Node& head(Edge<Data>& e) { return e.head(); }
  template<class Data>
  Node& tail(Edge<Data>& e) { return e.tail(); }

  // the data of a "const Adjacency<float>" should be "const float"
  template<class Adjacency> struct __DataFromAdjacency { using type = std::copy_cv_t<Adjacency, typename Adjacency::Data>; };
  template<> struct __DataFromAdjacency<Node> { using type = void; };
  template<> struct __DataFromAdjacency<const Node> { using type = void; };
  template<class Adjacency>
  using DataFromAdjacency = typename __DataFromAdjacency<std::remove_reference_t<Adjacency>>::type;

  template<class Adjacency>
  using EdgeFromAdjacency = Edge<std::copy_cvref_t<Adjacency, DataFromAdjacency<Adjacency>>>;
  template<class EdgeData>
  using AdjacencyFromData = typename Edge<EdgeData>::Adjacency;

  template<class Data>
  using DataReference = std::add_lvalue_reference_t<Data>;

  template<class Data>
  using ReverseEdgeFromData = Edge<DataReference<Data>>;
  template<class __Edge>
  using ReverseEdge = ReverseEdgeFromData<DataFromAdjacency<typename __Edge::Adjacency>>;

  template<class Adjacency>
  using ReverseAdjacency = typename ReverseEdge<EdgeFromAdjacency<Adjacency>>::Adjacency;
  template<class EdgeData>
  using ReverseAdjacencyFromData = typename ReverseEdgeFromData<EdgeData>::Adjacency;

  template<class Adjacency, class = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Adjacency>, Node>>>
  ReverseAdjacency<Adjacency> get_reverse_adjacency(const Node u, Adjacency& adj) { return ReverseAdjacency<Adjacency>(u, adj.data()); }
  template<class Adjacency, class = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Adjacency>, Node>>>
  ReverseAdjacency<const Adjacency> get_reverse_adjacency(const Node u, const Adjacency& adj) { return ReverseAdjacency<Adjacency>(u, adj.data()); }
  ReverseAdjacency<Node>      get_reverse_adjacency(const Node u, const Node v) { return u; }

  // an adjacency for storing weights on edges
  using WEdge = Edge<float>;

  using EdgeVec = std::vector<Edge<>>;
  using WEdgeVec = std::vector<WEdge>;
 
  template<class _Edge = Edge<>>
  using AdjacencySet = HashSet<typename _Edge::Adjacency>;
}

#define heads(x) seconds(x)
#define tails(x) firsts(x)


namespace std{
  template<class _Data>
  struct hash<PT::Edge<_Data> >{
    size_t operator()(const PT::Edge<_Data>& e) const {
      const hash<PT::NodePair> hasher;
      return hasher(e);
    }
  };
}


