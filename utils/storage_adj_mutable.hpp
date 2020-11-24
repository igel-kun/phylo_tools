

#pragma once

#include "predicates.hpp"
#include "storage_adj_common.hpp"


/*
 * in mutable edge storages, we DO NOT assume consecutivity.
 * Thus, most stuff is stored in hash-maps
 */

namespace PT{

#warning TODO: make a class that selects a tree/network type using properties wanted by the user
#warning TODO: unordered_map<Node, vector> with non-growing vectors also allows for certain modifications: subdivision, leaf-addition, etc
#warning TODO: make a mutable network class whose adjacencies are stored in a vector instead of a hashset
#warning TODO: rewrite alot: instead of removing front(children) or front(parents), do remove back(...), so vector<Node> has an easier time
  // by default, save the edge data in the successor map and provide a reference in each "reverse adjacency" of the predecessor map
  // note: the default maps map Nodes to sets of adjacencies in which 2 adjacencies are considered equal if their nodes are equal (ignoring data)
  template<class _EdgeData>
  using DefaultMutableSuccessorMap = HashMap<Node, HashSet<typename Edge<_EdgeData>::Adjacency, std::hash<Node>, std::equal_to<Node>>>;
  template<class _EdgeData>
  using DefaultMutablePredecessorMap = DefaultMutableSuccessorMap<DataReference<_EdgeData>>;
  template<class _EdgeData>
  using DefaultMutableTreePredecessorMap = HashMap<Node, std::singleton_set<ReverseAdjacencyFromData<_EdgeData>, std::default_invalid_t<Node>>>;


  template<class EdgeContainer, class LeafContainer, class NodeTranslation>
  void compute_translate_and_leaves(const EdgeContainer& edges, NodeTranslation* old_to_new, LeafContainer* leaves = nullptr)
  {
    if(leaves)
      for(const auto& uV: edges.successor_map())
        if(uV.second.empty())
          append(*leaves, uV.first);
    // translate maps are kind of silly for mutable storages, but if the user insists to want one, we'll give her/him one
    if(old_to_new)
      for(const auto& uV: edges.successor_map())
        append(*old_to_new, uV.first, uV.first);
  }




  template<class _EdgeData, class _SuccessorMap, class _PredecessorMap>
  class MutableAdjacencyStorage: public RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>
  {
    using Parent = RootedAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;
  protected:
    using Parent::_successors;
    using Parent::_predecessors;
    using Parent::_root;
    using Parent::_size;

    Node next_node_index = 0;
  public:
    using MutabilityTag = mutable_tag;
    using Parent::Parent;
    using typename Parent::Edge;
    using typename Parent::Adjacency;
    using Parent::compute_root;
    using Parent::in_degree;
    using Parent::out_degree;
    using Parent::parent;
    using Parent::children;
    using Parent::any_child;
    using Parent::has_node;

    template<class T>
    using NodeMap = HashMap<Node, T>;

    using Translation   = NodeMap<Node>;
    using InOutDegreeMap = NodeMap<InOutDegree>;
    using DegreeMap = NodeMap<Degree>;
    using NodeSet = HashSet<Node>;

#warning TODO: re-design such that it is always in a consistent state (single root, no cycles) --> disallow arbitrary deletion


    Node add_node()
    {
      assert(!test(_successors, next_node_index) && !test(_predecessors, next_node_index));
      _successors.try_emplace(next_node_index);
      _predecessors.try_emplace(next_node_index);

      return next_node_index++;
    }

    // the user can suggest(!) a node index (if this suggestion is invalid (already exists), we'll ignore it)
    Node add_node(const Node index)
    {
      if(!test(_successors, index)){
        _successors.try_emplace(index);
        _predecessors.try_emplace(index);
        next_node_index = std::max(index + 1, next_node_index);
        return index;
      } else return add_node();
    }

    // add a new child to u and return its index
    Node add_child(const Node u, const Node index = NoNode)
    {
      const Node v = index == NoNode ? add_node() : add_node(index);
      append(_predecessors, v, u);
      append(_successors, u, v);
      return v;
    }

    // the non-secure versions allow any form of edge addition
    // ATTENTION: this will NOT update the root! use set_root() afterwards!
    template<class _Edge> // for universal reference
    bool add_edge(_Edge&& uv) { return add_edge(uv.tail(), uv.get_adjacency()); }

    template<class _Adjacency> // for universal reference
    bool add_edge(const Node u, _Adjacency&& v)
    {
      const Node v_idx = static_cast<Node>(v);
      const auto [it, success] = append(_successors[u], std::move(v));
      if(success){
        if(append(_predecessors[v_idx], get_reverse_adjacency(u, *it)).second){
          ++_size;
          return true;
        } else {
          assert(false);
          exit(-1);
        }
      } else return false;
    }

    // replace the given parent y of z with x, that is, swap the arc yz with xz; return whether the replace took place (that is xz was not already there)
    bool replace_parent(const Node z, const Node old_parent, const Node new_parent)
    {
      if(old_parent == new_parent) return true;

      auto& y_children = _successors.at(old_parent);
      const auto z_iter = y_children.find(z);
      assert(z_iter != y_children.end());
      // move the z-adjacency into x's children
      auto& z_parents = _predecessors.at(z);
      if((new_parent != z) && append(_successors.at(new_parent), std::move(*z_iter)).second) {
        // replace y by x in z's parents
        y_children.erase(z_iter);
        z_parents.erase(old_parent);
        append(z_parents, new_parent);
        return true;
      } else { // if xz was already present, we've lost an edge
        y_children.erase(z_iter);
        z_parents.erase(old_parent);
        --_size;
        return false;
      }
    }

    // replace the (unique) parent of z with x, that is, hang the subtree rooted at z below x (z will become a child of x)
    bool replace_parent_of(const Node z, const Node x)
    {
      assert(in_degree(z) == 1);
      return replace_parent(z, front(_predecessors[z]), x);
    }

    // replace the given child z of y with x, that is, swap the arc yz with yx; return whether the swap was made (that is, yx is not already in the graph)
    //NOTE: if old_child is a leaf, then this leaf will be removed from the graph!
    bool replace_child(const Node y, const Node old_child, const Node new_child)
    {
      if(old_child == new_child) return true;

      auto& y_children = _successors.at(y);
      const auto z_iter = y_children.find(old_child);
      assert(z_iter != y_children.end());

      // in order to avoid invalidation of z_iter (by rehash) we'll have to move the adjacency into a temporary and then back into y_children
      Adjacency tmp_adj(std::move(*z_iter)); // move the adjacency out of y's children
      y_children.erase(z_iter);
      _predecessors.at(old_child).erase(y);

      if(in_degree(old_child) == 0){
        if(out_degree(old_child) == 0)
          remove_node(old_child);
        else
          throw std::logic_error("trying to create network with 2 roots");
      }

      static_cast<Node&>(tmp_adj) = new_child; // install x as new child of the temporary adjacency
      if((new_child != y) && append(y_children, std::move(tmp_adj)).second) { // put the modified adjacency back in
        append(_predecessors.at(new_child), y);
        // if old_child was a leaf, then just remove it, if it was a tree node, then throw an exception
        return true;
      } else { // if yx was already present, we've just lost an edge
        --_size;
        return false;
      }
    }

    // replace the (unique) child of y with x
    bool replace_child_of(const Node y, const Node x)
    {
      assert(out_degree(y) == 1);
      return replace_parent(y, front(_successors.at(y)), x);
    }

#warning: TODO: write a network class with builtin branch-length that can contract edges in context of their branch-lengths

    // contract the edge uv, leaving u in tact and removing v
    void contract_upwards(const Node v, const Node u)
    {
      assert(auto_find(_predecessors.at(v), u));
      assert((in_degree(u) + in_degree(v) <= 2) || (out_degree(u) + out_degree(v) <= 2)); // let's not create forbidden nodes (in- and out-degree > 1)
      
      auto& v_pred = _predecessors.at(v);
      v_pred.erase(u);
      while(!v_pred.empty()) replace_child(front(v_pred), v, u);

      auto& v_succ = _successors.at(v);
      while(!v_succ.empty()) replace_parent(front(v_succ), v, u);
      
      --_size; // we lost the edge uv
      _successors.at(u).erase(v);
      _predecessors.erase(v);
      _successors.erase(v);
    }

    void contract_upwards(const Node u)
    {
      assert(in_degree(u) == 1);
      contract_upwards(u, parent(u));
    }

    // contract the edge uv, leaving v in tact and removing u
    void contract_downwards(const Node u, const Node v)
    {
      assert(auto_find(_predecessors.at(v), u));
      assert((in_degree(u) + in_degree(v) <= 2) || (out_degree(u) + out_degree(v) <= 2)); // let's not create forbidden nodes (in- and out-degree > 1)

      std::cout << "contracting "<<u<<" onto its child "<<v<<"\n";
      while(out_degree(u)) replace_parent(any_child(u), u, v);
      while(has_node(u)) replace_child(parent(u), u, v);
    }

    void contract_downwards(const Node u)
    {
      assert(out_degree(u) == 1);
      contract_downwards(u, Parent::any_child(u));
    }

    // suppress (shortcut) a node with in-degree == 1 or out-degree <= 1 (if outdegree == 0, then just remove the node)
    void suppress_node(const Node y)
    {
      assert(in_degree(y) != 0); // please don't suppress the root
      switch(out_degree(y)){
        case 0:
          remove_node(y);
          break;
        case 1:
          contract_downwards(y);
          break;
        default:
          assert(in_degree(y) == 1);
          contract_upwards(y);
      }
    }

#warning TODO: have 2 global bools telling us whether to automatically suppress suppressible nodes or remove dangling leaves

#warning TODO: reformulate all bla_except()-functions using predicates like so:
    // remove a node and all out-deg-1 nodes directly above it
    template<class Predicate>
    void remove_upwards_except(const Node x, const Predicate& except)
    {
      if(!except.value(x)) {
        const auto& x_preds = _predecessors.at(x);
        const NodeVec x_parents(x_preds.begin(), x_preds.end());
        remove_node(x);
        for(const Node px: x_parents) {
          switch(out_degree(px)){
            case 0:
              remove_upwards_except(px, except);
              break;
            case 1:
              if(!except.value(px)) suppress_node(px);
            default: break;
          }
        }
      }
    }
    void remove_upwards(const Node x)
    {
      return remove_upwards_except(x, std::FalsePredicate());
    }


    //! subdivide uv: remove uv, add w, add uw and wv
    //NOTE: no checks are made, so if uv did not exist before, this will still create uw and wv, possibly making v a reticulation!
    template<class _Edge>
    Node subdivide(const _Edge& uv) { return subdivide(uv.tail(), uv.head()); }
    Node subdivide(const Node u, const Node v)
    {
      const Node w = add_node();
      DEBUG3(std::cout << "subvididing "<<u<<"-->"<<v<<" with new node "<<w<<"\n");
      remove_edge(u, v);
      add_edge(u, w);
      add_edge(w, v);
      return w;
    }

    // the secure versions allow only adding edges uv for which u is in the tree, but v is not, or v is the root and u is not in the tree
    bool add_edge_secure(const Edge& uv) { return add_edge(uv.tail(), uv.get_adjacency()); }
    bool add_edge_secure(const Node u, Adjacency&& v)
    {
      auto emp_succ = append(_successors, u);
      if(emp_succ.second){
        // oh, u did not exist before, so we added a new root
        if((v == _root) || _predecessors.empty()){
          _root = u;
          append(*emp_succ.first, v);
          append(_predecessors, v, u);
          ++_size;
          return true;
        } else {
          if(_predecessors.contains(v))
            throw std::logic_error("cannot create reticulation ("+std::to_string(v)+") in a tree");
          else
            throw std::logic_error("cannot create isolated edge ("+std::to_string(u)+","+std::to_string(v)+") with add_edge_secure() - if you are adding a bunch of edges resulting in a valid tree, use add_edge() + set_root()");
        }
      } else {
        auto emp_pred = append(_predecessors, v);
        if(emp_pred.second){
          append(*emp_pred.first, u);
          append(*emp_succ.first, v);
          ++_size;
        } else throw std::logic_error("cannot create reticulation ("+std::to_string(v)+") in a tree");
      }
    }

    bool remove_edge(const Edge& uv) { return remove_edge(uv.tail(), uv.head()); }
    bool remove_edge(const Node u, const Node v)
    {
      DEBUG5(std::cout << "removing edge "<<u<<"->"<<v<<"\n");
      const auto v_in = _predecessors.find(v);
      if(v_in != _predecessors.end()){
        // the data structure better be consistent
        assert(test(v_in->second, u));
        // remove uv from both containers
        _successors.at(u).erase(v);
        v_in->second.erase(u);
        --_size;
        return true;
      } else return false; // edge is not in here, so nothing to delete
    }

    bool remove_node(const Node v)
    {
      DEBUG5(std::cout << "removing node "<<v<<"\n");
      if((v == _root) && (out_degree(_root) > 1)) throw(std::logic_error("cannot remove the root unless it has out-degree one"));
      const auto v_pre = _predecessors.find(v);
      if(v_pre != _predecessors.end()){
        for(const Node u: v_pre->second)
          _successors.at(u).erase(v);
        _size -= v_pre->second.size();
        _predecessors.erase(v_pre);

        const auto v_succ = _successors.find(v);
        assert(v_succ != _successors.end());
        if((v == _root) && (out_degree(_root) != 0)) _root = front(v_succ->second);
        for(const Node u: v_succ->second)
          _predecessors.at(u).erase(v);
        _size -= v_succ->second.size();
        _successors.erase(v_succ);

        return true;
      } else return false;
    }

    //! initialization from edgelist without consecutive nodes
    template<class GivenEdgeContainer, class LeafContainer = NodeVec, class NodeTranslation = Translation>
    MutableAdjacencyStorage(const consecutivity_tag,
                            GivenEdgeContainer&& given_edges,
                            NodeTranslation* old_to_new = nullptr,
                            LeafContainer* leaves = nullptr):
      Parent()
    {
      if(!given_edges.empty()){
        std::cout << "adding edges to the tree/network...\n";
        for(auto&& uv: given_edges) {
          _predecessors.try_emplace(uv.tail());
          _successors.try_emplace(uv.head());
          add_edge(uv);
        }
        next_node_index = _successors.size();
        std::cout << "computing the root...\n";
        compute_root();
        std::cout << "computing node translation and leaves...\n";
        compute_translate_and_leaves(*this, old_to_new, leaves);
      } else _root = add_node();
    }

  };

  template<class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutablePredecessorMap<_EdgeData>>
  using MutableNetworkAdjacencyStorage = MutableAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;


  template<class _EdgeData = void,
           class _SuccessorMap = DefaultMutableSuccessorMap<_EdgeData>,
           class _PredecessorMap = DefaultMutableTreePredecessorMap<_EdgeData>>
  using MutableTreeAdjacencyStorage = MutableAdjacencyStorage<_EdgeData, _SuccessorMap, _PredecessorMap>;

}

