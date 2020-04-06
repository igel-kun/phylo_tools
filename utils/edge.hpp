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

    // this is the closest we can get to inheriting the parent class' constructors
    //template <class... T> __Adjacency(const Node u, T... t) : Parent(u, Data(t...))
    __Adjacency(const Node u, Data& d) : Parent(u, d)
    {
      std::cout << "\n((ADJ construct made adjacency ("<<Parent::first<<","<<Parent::second<<")) from ("<<u<<", ...))\n";
    }

    // access the adjacency's data
    Data& data() { return Parent::second; }
    const Data& data() const { return Parent::second; }

    // an Adjacencies can be cast to their Node
    operator Node&() { return Parent::first; }
    operator const Node&() const { return Parent::first; }
  };

  template<class _Data>
  struct _Adjacency { using type = __Adjacency<_Data>; };
  template<>
  struct _Adjacency<void> { using type = Node; };
  template<class _Data>
  using Adjacency_t = typename _Adjacency<_Data>::type;


  // an edge is a node (head) with another node (->tail)
  template<class _Adjacency>
  struct AbstractEdge: public std::pair<Node, _Adjacency>
  {
    using Parent = std::pair<Node, _Adjacency>;
    using Adjacency = _Adjacency;
    using Parent::Parent;
    
    // construct an edge from a node and whatever is needed to construct an adjacency (for example, an adjacency)
    template <class... T> AbstractEdge(const Node u, T... t) : Parent(u, Adjacency(t...)) {}
    // construct the reverse of an edge in a similar manner as above
    template <class... T> AbstractEdge(const reverse_edge_tag tag, const Node u, T... t) : Parent(u, Adjacency(t...))
      { reverse(); }
    // construct from edge with different data; for example, from a ReverseEdge (whose data is a reference_wrapper)
    template<class _Data>
    AbstractEdge(const AbstractEdge<_Data>& other):
      Parent(static_cast<typename Parent::first>(other.first), static_cast<typename Parent::second>(other.second)) {}

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
    using Adjacency = Adjacency_t<Data>;
 
    Adjacency& get_adjacency() { return Parent::second; }
    const Adjacency& get_adjacency() const { return Parent::second; }
    Data& data() { return get_adjacency().data(); }
    const Data& data() const { return get_adjacency().data(); }

    std::ostream& print(std::ostream& os) const { return os << "("<<head()<<","<<tail()<<"):"<<data(); }
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

  template<class Adjacency>
  struct DataFromAdjacency { using type = typename Adjacency::Data; };
  template<>
  struct DataFromAdjacency<Node> { using type = void; };
  template<>
  struct DataFromAdjacency<const Node> { using type = void; };
  template<class Adjacency>
  using DataFromAdjacency_t = typename DataFromAdjacency<Adjacency>::type;

  template<class Adjacency>
  using EdgeFromAdjacency = Edge<DataFromAdjacency_t<Adjacency>>;
  template<class EdgeData>
  using AdjacencyFromData = typename Edge<EdgeData>::Adjacency;

  template<class Data>
  using DataReference = std::conditional_t<std::is_same_v<Data, void>, void, std::reference_wrapper<Data>>;

  template<class Data>
  using ReverseEdgeFromData = Edge<DataReference<Data>>;
  template<class __Edge>
  using ReverseEdge = ReverseEdgeFromData<DataFromAdjacency_t<typename __Edge::Adjacency>>;

  template<class Adjacency>
  using ReverseAdjacency = typename ReverseEdge<EdgeFromAdjacency<Adjacency>>::Adjacency;
  template<class EdgeData>
  using ReverseAdjacencyFromData = typename ReverseEdgeFromData<EdgeData>::Adjacency;

  template<class Adjacency>
  ReverseAdjacency<Adjacency> get_reverse_adjacency(const Node u, Adjacency& adj) {
    ReverseAdjacency<Adjacency> result(u, adj.data());
    std::cout << " [[constructed reverse adj "<<result<<" from "<<u<<" & adj "<<adj<<"]]\n";
    return result; //{u, adj.data()};
  }
  template<>
  ReverseAdjacency<Node> get_reverse_adjacency(const Node u, const Node& v) { return u; }

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


