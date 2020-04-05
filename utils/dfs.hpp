
#pragma once

#include "set_interface.hpp"

namespace PT{
  enum TraversalType {preorder, inorder, postorder};


  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  class Traversal
  {
  public:
    using Edge = typename _Network::Edge;
  protected:
    const _Network& N;
    _Container& out;

    NodeSet seen;

    virtual void emit_node(const Node u) const = 0;
    virtual void emit_edge(const Node u, const Node v) const = 0;

    inline void remove_node(const Node u) { N.remove_node(u); }
  public:

    Traversal(_Network& _N, _Container& _out):
      N(_N), out(_out), seen()
    {}

    void do_preorder() {do_preorder(N.root());}
    bool do_preorder(const Node u)
    {
      if(!track_seen || !contains(seen, u)){
        if(track_seen) append(seen, u);
        emit_node(u);
        for(const auto& v: N.children(u)){
          emit_edge(u, v);
          do_preorder(v);
        }
        return true;
      } return false;
    }

    void do_postorder() {do_postorder(N.root());}
    bool do_postorder(const Node u)
    {
      if(!track_seen || !contains(seen, u)){
        if(track_seen) append(seen, u);
        for(const auto& v: N.children(u)){
          do_postorder(v);
          emit_edge(u, v);
        }
        emit_node(u);
        return true;
      } return false;
    }

    void do_inorder() {do_inorder(N.root());}
    bool do_inorder(const Node u)
    {
      if(!track_seen || !contains(seen, u)){
        if(track_seen) append(seen, u);
        
        bool emitted = false;
        for(const auto& v: N.children(u)){
          do_inorder(v);
          if(!emitted) {
            emit_node(u);
            emit_edge(u, v);
            emitted = true;
          }
        }
        if(!emitted) emit_node(u);
        return true;
      } return false;
    }


    _Container& do_traversal() {return do_traversal(N.root());}
    _Container& do_traversal(const Node u)
    {
      switch(o){
        case preorder: do_preorder(u); break;
        case inorder: do_inorder(u); break;
        case postorder: do_postorder(u); break;
        default: std::cerr<<"this is a bug!"<<std::endl; exit(1);
      };
      return out;
    }
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  class NodeTraversal: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;
  protected:
    using Parent::out;

    void emit_node(const Node u) const { append(out, u); }
    void emit_edge(const Node u, const Node v) const {}
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  class EdgeTraversal: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;

  protected:
    using Parent::out;

    void emit_node(const Node u) const {}
    void emit_edge(const Node u, const Node v) const { append(out, u, v); }
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _ExceptContainer,
           class _Network>
  class NodeTraversalExcept: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;
  protected:
    using Parent::out;
    using Parent::seen;

    void emit_node(const Node u) const { append(out, u); }
    void emit_edge(const Node u, const Node v) const {}
  public:
    NodeTraversalExcept(_Network& _N, _Container& _out, const _ExceptContainer& _except):
      Parent(_N, _out)
    {
      seen.insert(_except.begin(), _except.end());
    }
  };

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _ExceptContainer,
           class _Network>
  class EdgeTraversalExcept: public Traversal<o, track_seen, _Container, _Network>
  {
    using Parent = Traversal<o, track_seen, _Container, _Network>;
  public:
    using typename Parent::Edge;
    using Parent::Parent;

  protected:
    using Parent::out;
    using Parent::seen;
    const _ExceptContainer& except;

    void emit_node(const Node u) const {}
    void emit_edge(const Node u, const Node v) const { if(!contains(except, v)) append(out, u, v); }
  public:
    EdgeTraversalExcept(_Network& _N, _Container& _out, const _ExceptContainer& _except):
      Parent(_N, _out),
      except(_except)
    {
      seen.insert(_except.begin(), _except.end());
    }
  };


  // convenience functions
  template<TraversalType o,
           class _Container,
           class _ExceptContainer,
           class _Network>
  _Container node_traversal(_Network& N, const _ExceptContainer& except, const Node u)
  {
    _Container out;
    return NodeTraversalExcept<o, true, _Container, _ExceptContainer, _Network>(N, out, except).do_traversal(u);
  }

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  _Container node_traversal(_Network& N, const Node u)
  {
    _Container out;
    return NodeTraversal<o, track_seen, _Container, _Network>(N, out).do_traversal(u);
  }

  template<TraversalType o,
           class _Container,
           class _ExceptContainer,
           class _Network>
  _Container edge_traversal(_Network& N, const _ExceptContainer& except, const Node u)
  {
    _Container out;
    return EdgeTraversalExcept<o, true, _Container, _ExceptContainer, _Network>(N, out, except).do_traversal(u);
  }

  template<TraversalType o,
           bool track_seen,
           class _Container,
           class _Network>
  _Container edge_traversal(_Network& N, const Node u)
  {
    _Container out;
    return EdgeTraversal<o, track_seen, _Container, _Network>(N, out).do_traversal(u);
  }




}// namespace
