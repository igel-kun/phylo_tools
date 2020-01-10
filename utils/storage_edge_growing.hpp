
#pragma once

#include "storage_edge_common.hpp"

namespace PT{
  // =============== growing storages =====================

  template<class _Edge = Edge<>,
           class _EdgeContainer = std::unordered_set<_Edge>,
           class _InEdgeContainer = _EdgeContainer>
  class GrowingRootedEdgeStorage: public RootedEdgeStorage<_EdgeContainer>
  {
    using Parent = RootedEdgeStorage<_EdgeContainer>;

  public:
    using Parent::Parent;
    using Edge = _Edge;
    using EdgeContainer = _EdgeContainer;
    using Node = typename Edge::Node;

    using InEdgeMap = std::unordered_map<Node, _InEdgeContainer>;

  protected:
    using Parent::_out_edges;
    using Parent::_root;
    using Parent::_size;
    
    InEdgeMap _in_edges;

    virtual bool add_edge(const Edge& uv) = 0;

    // compute the leaves from the _in_edges and _out_edges mappings
    template<class LeafContainer>
    void compute_leaves(LeafContainer& leaves) const
    {
      for(const auto& uv: _in_edges){
        const Node v = uv.first;
        if(!contains(_out_edges, v))
          append(leaves, v);
      }
    }
    // compute the root from the _in_edges and _out_edges mappings
    void compute_root()
    {
      char roots = 0;
      for(const auto& u_edges: _out_edges){
        const Node u = u_edges.first;
        if(!contains(_in_edges, u)){
          if(++roots == 0){
            _root = u;
          } else throw std::logic_error("cannot create tree/network with multiple roots ("+std::to_string(_root)+" & "+std::to_string(u)+")");
        }
      }
    }

    template<class NodeContainer&>
    void compute_nodes(NodeContainer& nodes) const
    {
      for(const auto& u: _in_edges.keys())
        append(nodes, u);
      // note that the root is missing from the _in_edges map
      append(nodes, _root);
    }

  public:

    //! initialization from edgelist with consecutive nodes
    // actually, we don't care about consecutiveness for growing storages...
    template<class GivenEdgeContainer, class LeafContainer = std::vector<Node> >
    GrowingRootedEdgeStorage(const consecutive_edgelist_tag& tag,
                           const GivenEdgeContainer& given_edges,
                           const uint32_t _num_nodes, // although we don't really need to know how many nodes there are, we keep this for compatibility
                           LeafContainer* leaves = nullptr):
      Parent::_size(0)
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      compute_root();
      if(leaves) compute_leaves(&leaves);
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class NodeContainer, class LeafContainer = std::vector<Node> >
    GrowingRootedEdgeStorage(const GivenEdgeContainer& given_edges, NodeContainer& nodes, LeafContainer* leaves = nullptr):
      Parent::_size(0)
    {
      for(const auto& uv: given_edges)
        add_edge(uv);
      compute_root();
      compute_nodes(nodes);
      if(leaves) compute_leaves(&leaves);
    }


  };


  // to make a growing tree storage, we use the growing storage with a refernce_wrapper<Edge> as in_edge list making use of append()-overwrites
  template<class _Edge = Edge<>,
           class _EdgeContainer = std::unordered_set<_Edge>>
  class GrowingTreeEdgeStorage:
    public GrowingRootedEdgeStorage<_EdgeContainer, std::reference_wrapper<typename _EdgeContainer::value_type>>
  {
    using Parent = GrowingRootedEdgeStorage<_Edge, _EdgeContainer, std::reference_wrapper<typename _EdgeContainer::value_type>>;

  public:
    using typename Parent::EdgeContainer;
    using typename Parent::Edge;
    using typename Parent::Node;
    using RevEdge = std::reference_wrapper<Edge>;
    using ConstRevEdge = std::reference_wrapper<const Edge>;

    using InEdgeContainer = std::list<RevEdge>;
    using ConstInEdgeContainer = std::list<ConstRevEdge>;
   
    using ConstPredContainer = std::list<const Node>;

  protected:
    using Parent::Parent;
    using Parent::_out_edges;
    using Parent::_in_edges;
    using Parent::_root;
    using Parent::_size;

  public:

    // return the parent of u, or u itself if it is the root
    Node parent(const Node u) const { return (u == _root) ? _root : _in_edges.at(u).head(); }
    uint32_t in_degree(const Node u) const { return (u == _root) ? 0 : 1; }

    ConstPredContainer predecessors(const Node u) const
      { return (u == _root) ? ConstPredContainer({}) : ConstPredContainer({_in_edges.at(u).tail()}); }

    // NOTE: we have to allow write-access to the edges/adjacencies to allow the user to change edge weights
    Edge& in_edge(const Node u) {return _in_edges.at(u); }
    const Edge& in_edge(const Node u) const {return _in_edges.at(u); }

    InEdgeContainer in_edges(const Node u) { return {map_lookup(_in_edges, u)}; }
    ConstInEdgeContainer in_edges(const Node u) const { return {map_lookup(_in_edges, u)}; }

    bool add_edge(const Node u, const typename Edge::Adjacency& v) { return add_edge({u, v}); }    
    bool add_edge(const Edge& uv)
    {
      const Node u = uv.tail();
      // step 1: try to insert uv into _out_edges[u]
      const auto emp_res_out = append(_out_edges[u], uv);
      if(emp_res_out.second){
        const Node v = uv.head();
        // step 2: try to insert uv into _in_edges[v]
        const auto emp_res_in = append(_in_edges, v, *(emp_res_out.first));
        if(!emp_res_in.second)
          throw std::logic_error("cannot create reticulation in tree edge storage");
        return true;
        ++_size;
      } else return false; // return false if the edge is already there
    }
    
    bool remove_edge(const Edge& uv) { return remove_edge(uv.tail(), uv.head()); }
    bool remove_edge(const Node u, const Node v)
    {
      const auto uv_in = _in_edges.find(v);
      if(uv_in != _in_edges.end()){
        // the data structure better be consistent
        assert(uv_in->first == u);
        assert(uv_in->second == v);
        // remove uv from both containers
        _out_edges.at(u).erase(*uv_in);
        _in_edges.erase(uv_in);
        --_size;
        return true;
      } else return false; // edge is not in here, so nothing to delete
    }

    bool remove_node(const Node v)
    {
      if((v == _root) && (_size != 0)) throw(std::logic_error("cannot remove the root from a non-empty rooted storage"));
      const auto uv_it = _in_edges.find(v);
      if(uv_it != _in_edges.end()){
        _out_edges.at(uv_it->tail()).erase(*uv_it);
        _in_edges.erase(v);
        _out_edges.erase(v);
        return true;
      } else return false;
    }


  };


  template<class _Edge = Edge<>,
           class _EdgeContainer = std::unordered_set<_Edge>,
           class _InEdgeContainer = std::unordered_set<typename _Edge::Node>>
  class GrowingNetworkEdgeStorage: public GrowingRootedEdgeStorage<_Edge, _EdgeContainer, _InEdgeContainer>
  {
    using Parent = GrowingRootedEdgeStorage<_Edge, _EdgeContainer, _InEdgeContainer>;

  public:
    using typename Parent::EdgeContainer;
    using typename Parent::InEdgeMap;
    using typename Parent::Edge;
    using typename Parent::Node;
    using InEdgeContainer = typename InEdgeMap::mapped_type&;
    using ConstInEdgeContainer = const InEdgeContainer;
    using ConstPredContainer = ConstFirstFactory<EdgeContainer>;

  protected:
    using Parent::_out_edges;
    using Parent::_in_edges;
    using Parent::_root;
    using Parent::_size;
    using Parent::insert_edges;
    using Parent::compute_root;
    using Parent::compute_leaves;

  public:
    using Parent::Parent;

    bool add_edge(const Node u, typename Edge::Adjacency& v){ return add_edge(u, v); }
    bool add_edge(const Edge& uv){
      const Node u = uv.tail();
      // step 1: try to insert uv into _out_edges[u]
      const auto app_res_out = append(_out_edges[u], uv);
      if(app_res_out.second){
        const Node v = uv.head();
        // step 2: try to insert uv into _in_edges[v]
        const bool uv_app = append(_in_edges[v], *(app_res_out.first)).second;
        assert(uv_app);
        ++_size;
        return true;
      } else return false; // return false if the edge is already there
    }

    bool remove_edge(const Edge& uv) { return remove_edge(uv.tail(), uv.head()); }
    bool remove_edge(const Node u, const Node v)
    {
      const auto v_in = _in_edges.find(v);
      if(v_in != _in_edges.end()){
        // NOTE: make sure two edges with equal head & tail compare true wrt. operator==() (they should if they are derived from std::pair<Node, Node>
        const auto uv_in = v_in.first->find(Edge({u,v}));
        if(uv_in != v_in.first->end()){
          // the data structure better be consistent
          assert(uv_in->first == u);
          assert(uv_in->second == v);
          // remove uv from both containers
          _out_edges.at(u).erase(*uv_in);
          _in_edges.erase(uv_in);
          --_size;
          return true;
        } else return false;
      } else return false; // edge is not in here, so nothing to delete
    }

    void remove_node(const Node v)
    {
      if((v == _root) && (_size != 0)) throw(std::logic_error("cannot remove the root from a non-empty rooted storage"));
      for(const auto& uv: _in_edges.at(v))
        _out_edges.at(uv.tail()).erase(uv);
      _in_edges.erase(v);
      _out_edges.erase(v);
    }
   
    uint32_t in_degree(const uint32_t u) const { return (u == _root) ? 0 : _in_edges.at(u).size(); }

    ConstPredContainer predecessors(const uint32_t u) const { return const_tails(in_edges(u)); }

    // NOTE: we have to allow write-access to the edges/adjacencies to allow the user to change edge weights
    InEdgeContainer in_edges(const uint32_t u) { return map_lookup(_in_edges, u); }
    ConstInEdgeContainer in_edges(const uint32_t u) const { return map_lookup(_in_edges, u); }

  };

  // abbreviations
  template<class _Edge = Edge<>>
  using UnorderedTreeEdgeStorage = GrowingTreeEdgeStorage<std::unordered_set<_Edge>>;
  template<class _Edge = Edge<>>
  using UnorderedNetworkEdgeStorage = GrowingNetworkEdgeStorage<std::unordered_set<_Edge>>;



}// namespace

