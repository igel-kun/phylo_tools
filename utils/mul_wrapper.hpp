
#pragma once


namespace PT{

  // the fake mul tree keeps a network and a root (any vertex inside the network) around and minicks a MUL tree
  template<PhylogenyType Network_>
  class FakeMULTree {
    using Network = Network_;
    using Node = typename Network::Node;

    Network* N;
    Node root;
    size_t num_vertices;
    
  public:
    FakeMULTree(const Network& N_, const Node _root): N(&N_), root(_root) {}

  };
}
