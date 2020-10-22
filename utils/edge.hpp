#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "pair_iter.hpp"
#include <queue>
#include <new>

namespace PT{

  struct reverse_edge_tag {};
  constexpr reverse_edge_tag reverse_edge = reverse_edge_tag();

  // NOTE: Adjacencies store a const Node, which implies that they cannot be assigned. This may make them tricky to handle.
  //       In particular, vector<Adjacency>::push_back() will not compile! Use emplace_back() instead!
  template<class _Data>
  struct __Adjacency: public std::pair<const Node, _Data>
  {
    using Parent = std::pair<const Node, _Data>;
    using Parent::Parent;
    using Data = _Data;
 
    template<class T> static constexpr bool is_compatible_data  = std::is_convertible_v<Data, std::remove_reference_t<Data>>;
    template<class T> static constexpr bool has_compatible_data = is_compatible_data<typename std::remove_reference_t<T>::Data>;

    // construct an adjacency from another adjacency, but replace the node
    template<class __Data>
    __Adjacency(const Node u, __Adjacency<__Data>&& adj) : Parent(u, std::forward<__Data>(adj.second)) {}
    template<class __Data>
    __Adjacency(const Node u, const __Adjacency<__Data>& adj) : Parent(u, adj.second) {}

    // construct an adjacency from a node and data
    template<class __Data, std::enable_if_t<is_compatible_data<__Data, Data>>>
    __Adjacency(const Node u, __Data&& d) : Parent(u, std::forward<__Data>(d)) {}

    // construct an adjacency from any compatible adjacency
    template<class Adj, class = std::enable_if_t<has_compatible_data<Adj>>>
    __Adjacency(Adj&& adj) : Parent(adj.first, std::forward<Data>(adj.second)) {}

    // access the adjacency's data
    Data& data() { return Parent::second; }
    const Data& data() const { return Parent::second; }

    // Adjacencies can be cast to their Node
    operator const Node&() const { return Parent::first; }
  };
  template<class Adjacency>
  constexpr bool has_data = !std::is_same_v<std::remove_cvref_t<Adjacency>, Node>;

  // an Adjacency with void data is a Node
  template<class _Data>
  using AdjacencyFromData = std::conditional_t<std::is_void_v<_Data>, Node, __Adjacency<_Data>>;


  // an edge is a node (head) with another node (->tail)
  template<class _Adjacency>
  struct AbstractEdge: public std::pair<const Node, _Adjacency>
  {
    using Parent = std::pair<const Node, _Adjacency>;
    using Adjacency = _Adjacency;
    using Parent::Parent;
    
    // construct an edge from a node and whatever is needed to construct an adjacency (for example, an adjacency)
    template<class... Args> AbstractEdge(const Node u, Args&&... args):
      Parent(std::piecewise_construct, std::forward_as_tuple(u), std::forward_as_tuple(std::forward<Args>(args)...))
    {}
    // construct from edge with different data; for example, from a ReverseEdge (whose data is a reference)
    template<class _Data>
    AbstractEdge(const AbstractEdge<_Data>& other):
      Parent(static_cast<typename Parent::first_type>(other.first), static_cast<typename Parent::second_type>(other.second))
    {}

    const Node& head() const { return this->second; }
    const Node& tail() const { return this->first; }
    bool incident_with(const Node x) const { return (x == this->first) || (x == this->second); }
    std::pair<const Node&, const Node&> as_pair() const { return { this->first, this->second }; }

    bool operator<(const AbstractEdge& e) const
    {
      if(this->second == e.second)
        return this->first < e.first;
      else 
        return this->second < e.second;
    }
    template<class __Adjacency>
    bool operator==(const AbstractEdge<__Adjacency>& other) const { return (head() == other.head()) && (tail() == other.tail()); }

    Adjacency& get_adjacency() { return Parent::second; }
    const Adjacency& get_adjacency() const { return Parent::second; }

    std::ostream& print(std::ostream& os) const { return os << Parent(*this); }
  };

  template<class _Data = void>
  struct Edge: public AbstractEdge<AdjacencyFromData<_Data>>
  {
    using Parent = AbstractEdge<AdjacencyFromData<_Data>>;
    using Parent::Parent;
    using Parent::head;
    using Parent::tail;
    using Data = _Data;
    using Adjacency = typename Parent::Adjacency;
 
    // construct the reverse of an edge
    template<class Adjacency> Edge(const reverse_edge_tag, const Node u, Adjacency&& adj):
      Parent((Node)adj, u, adj.data())
    {}

    Data& data() { return Parent::get_adjacency().data(); }
    const Data& data() const { return Parent::get_adjacency().data(); }

    std::ostream& print(std::ostream& os) const
    { return os << "("<<tail()<<","<<head()<<"):"<<data(); }
  };
 
  template<> class Edge<void>: public AbstractEdge<Node>
  {
    using Parent = AbstractEdge<Node>;
  public:
    using Parent::Parent;

    // construct an edge from two nodes and ignore whatever else is given
    template<class... Args> Edge(const Node u, const Node v, Args&&...): Parent(u, v) {}

    // construct the reverse of an edge
    Edge(const reverse_edge_tag, const Node u, const Node v): Parent(v, u) {}

  };
  
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
  template<class Adjacency>
  using DataFromAdjacency = std::copy_cv_t<Adjacency, typename __DataFromAdjacency<std::remove_cvref_t<Adjacency>>::type>;

  template<class Adjacency>
  using EdgeFromAdjacency = Edge<std::copy_cvref_t<Adjacency, DataFromAdjacency<Adjacency>>>;

  template<class Data>
  using DataReference = std::add_lvalue_reference_t<Data>;
  template<class Adjacency>
  using ReferencingAdjacency = AdjacencyFromData<DataReference<DataFromAdjacency<Adjacency>>>;

  template<class Data>
  using ReverseEdgeFromData = Edge<DataReference<Data>>;
  template<class __Edge>
  using ReverseEdge = ReverseEdgeFromData<DataFromAdjacency<typename __Edge::Adjacency>>;
  template<class Adjacency>
  using ReverseAdjacency = typename ReverseEdge<EdgeFromAdjacency<Adjacency>>::Adjacency;
  template<class EdgeData>
  using ReverseAdjacencyFromData = typename ReverseEdgeFromData<EdgeData>::Adjacency;

  // creating reverse adjacencies from normal adjacencies
  //NOTE: reversing a reversed adjacency may be counter intuitive for the following reason:
  //      a reverse_adjacency of an Adjacency<Data> has data type Data&, so the reverse of that still has Data& as data type!
  template<class Adjacency, class = std::enable_if_t<has_data<Adjacency>>>
  ReverseAdjacency<Adjacency> get_reverse_adjacency(const Node u, Adjacency& adj)
  { return ReverseAdjacency<Adjacency>(u, adj.data()); }
  
  template<class Adjacency, class = std::enable_if_t<has_data<Adjacency>>>
  ReverseAdjacency<const Adjacency> get_reverse_adjacency(const Node u, const Adjacency& adj)
  { return ReverseAdjacency<Adjacency>(u, adj.data()); }
  ReverseAdjacency<Node>      get_reverse_adjacency(const Node u, const Node v) { return u; }

  // an adjacency for storing weights on edges
  using WEdge = Edge<float>;

  using EdgeVec = std::vector<Edge<>>;
  using WEdgeVec = std::vector<WEdge>;
 
  template<class _Edge = Edge<>>
  using AdjacencySet = HashSet<typename _Edge::Adjacency>;


  // emplace a new adjacency at position possibly replacing the node by v
  template<class Adjacency, class = std::enable_if_t<has_data<Adjacency>>>
  void emplace_new_adjacency(Adjacency* const position, Adjacency&& adj) { new (position) Adjacency(std::move(adj)); }
  void emplace_new_adjacency(Node* const position, const Node u) { new (position) Node(u); }

  template<class Adjacency, class = std::enable_if_t<has_data<Adjacency>>>
  void emplace_new_adjacency(Adjacency* const position, Adjacency&& adj, const Node v) { new (position) Adjacency(v, std::move(adj)); }
  void emplace_new_adjacency(Node* const position, const Node u, const Node v) { new (position) Node(v); }

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


