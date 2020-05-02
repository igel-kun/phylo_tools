
#pragma once


namespace PT{

  // the fake mul tree keeps a network and a root (any vertex inside the network) around and minicks a MUL tree
  template<class _Network>
  class FakeMULTree{
    using Network = _Network;

    const Network& N;
    const Node root;
    const size_t num_vertices;
    
  public:
    FakeMULTree(const Network& _N, const Node _root): N(_N), root(_root) {}

  };
}
