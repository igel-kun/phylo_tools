
#pragma once

#include "tree.hpp"

namespace PT{

  // a tree consists of a list of nodes, each having out-neighbors and one in-neighbor
  // the out-neighbor list are represented by a pointer into an out-neighbor list that is handled by the tree
  //NOTE: the first template argument of the TreeNode must be compatible with _Edge because it will point into a list of _Edge's!
  //NOTE: this tree is immutable in the sense that it cannot grow (that is, after construction, no new vertices or edges can be added)
  template<class _Edge = Edge,
           class _Node = TreeNodeT<_Edge>,
           class _NodeList = std::vector<_Node>>
  class ROProtoTreeT: public ProtoTreeT<_Edge, _Node, _NodeList>
  {
    using Parent = ProtoTreeT<_Edge, _Node, _NodeList>;
  public:
    using typename Parent::Node;
    using typename Parent::Edge;
    using typename Parent::SuccList;
    using typename Parent::EdgeIter;
  protected:
    using Parent::names;
    using Parent::nodes;
    using Parent::_num_edges;
    using Parent::root;
    using Parent::leaves;
    using Parent::max_outdeg;
  public:
    using Parent::get_leaves;
    using Parent::get_nodes;
    using Parent::get_node;
    using Parent::get_names;
    using Parent::operator[];
    using Parent::get_name;
    using Parent::begin;
    using Parent::end;
    using Parent::get_root;
    using Parent::num_nodes;
    using Parent::num_edges;
    using Parent::get_leaves_labeled;
    using Parent::get_nodes_labeled;
    using Parent::is_bifurcating;
    using Parent::is_binary;
    using Parent::empty;
    using Parent::update_max_degrees;
    using Parent::LCA;
    using Parent::has_path;
    using Parent::get_minimum;
    using Parent::is_preordered;
    using Parent::is_multi_labeled;
    using Parent::is_edge;
    using Parent::has_cycle;
    using Parent::print_subtree;

  protected:

    _Edge* edges;

  public:
 
    // =============== variable query ======================


    // =================== information query ==============



    // =================== modification ====================



    // ================== construction =====================

    ROProtoTreeT(const NameVec& _names, const uint32_t edgenum):
      Parent::ProtoTreeT(_names, edgenum),
      edges((_Edge*)malloc(edgenum * sizeof(_Edge)))
    {}

    // read all nodes and prepare the edge storage to receive edges; return the root
    template<class EdgeContainer = std::vector<_Edge>>
    uint32_t read_nodes_and_prepare_edge_storage(const EdgeContainer& given_edges, const uint32_t num_nodes)
    {
      nodes.reserve(num_nodes);
      // compute out-degrees and the root
      std::iterable_bitset root_poss(num_nodes);
      root_poss.set_all();
      uint32_t* out_deg = (uint32_t*)calloc(num_nodes, sizeof(uint32_t));
      for(const auto &edge: given_edges){
        assert(edge.tail() < num_nodes);
        assert(edge.head() < num_nodes);
        ++out_deg[edge.tail()];
        root_poss.clear(edge.head());
        DEBUG5(std::cout << "treated edge "<<edge<<", root poss: "<<root_poss<<std::endl);
      }
      // compute start points in the edge list
      EdgeIter e_start = edges;
      for(uint32_t u_idx = 0; u_idx < num_nodes; ++u_idx){
        const uint32_t u_outdeg = out_deg[u_idx];
        // emplace a tree node without parent whose outessors start at e_start
        nodes.emplace_back();
        _Node& u = nodes.back();

        u.out.start = e_start;
        std::advance(e_start, u_outdeg);
        if(u_outdeg > 0){
          max_outdeg = std::max(max_outdeg, u_outdeg);
        } else leaves.push_back(u_idx);
      }
      free(out_deg);

      // finally, set the root
      if(root_poss.size() != 1) throw std::logic_error("cannot create tree with "+std::to_string(root_poss.size())+" roots");
      return front(root_poss);
    }


    // =================== destruction ====================
    //! destructor frees everything
    ~ROProtoTreeT()
    {
      if(edges) free(edges);
    }

    // =================== i/o ======================
    
  };






  template<class _Edge = Edge,
           class _Node = TreeNodeT<_Edge>,
           class _NodeList = std::vector<_Node>>
  class TreeT: public ROProtoTreeT<_Edge, _Node, _NodeList>
  {
  protected:
    using Parent = ROProtoTreeT<_Edge, _Node, _NodeList>;
    using typename Parent::EdgeIter;
    using Parent::nodes;
    using Parent::ROProtoTreeT;
    using Parent::root;
    using Parent::has_cycle;
    using Parent::_num_edges;

    //! add an edge
    void add_edge(const _Edge& e)
    {
      assert(e.head() < nodes.size());
      assert(e.tail() < nodes.size());

      _Node& u = nodes[e.tail()];
      _Node& v = nodes[e.head()];

      const EdgeIter it = u.out.emplace_back(e);
      v.in = it;
    }

  public:

    //! read a tree from a (not neccessarily sorted) list of pairs of uint32_t (aka edges) in which each vertex is less than num_nodes
    // NOTE: make sure that the indices of the leaves are increasing from left to right (that is, leaves are ordered)!
    //       This is the case for inputs in newick format
    template<class EdgeContainer = std::vector<_Edge>>
    TreeT(const EdgeContainer& given_edges, const NameVec& _names, const uint32_t num_nodes):
      Parent(_names, given_edges.size())
    {
      // get memory & initialize class variables
      DEBUG3(std::cout << "constructing tree from "<<_num_edges<<" edges" << std::endl);
      assert(num_nodes == _num_edges + 1);

      root = Parent::read_nodes_and_prepare_edge_storage(given_edges, num_nodes);
      // actually read the edges
      for(const auto &e: given_edges) add_edge(e);
    }

    // for some reason gcc wont let me put _names.size() as default value for the last argument, so I have to duplicate code...
    template<class EdgeContainer = EdgeVec>
    TreeT(const EdgeContainer& given_edges, const NameVec& _names):
      TreeT(given_edges, _names, _names.size())
    {}

  };

  
  typedef TreeT<> Tree;

}
