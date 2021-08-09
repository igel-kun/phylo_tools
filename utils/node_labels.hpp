
#pragma once

#include "node.hpp"

namespace PT {

  // some special data types
  struct NodeDataLabel {
    using LabelType = std::string;
    LabelType label;
  };


  // a LabeledNodeType requires having a LabelType and a label attribute
  template<NodeType N>
  concept LabeledNodeType = requires(N n) {
    { n.label } -> typename N::LabelType;
  }

  // LabelType<N> is a string if Node is labeled and void, otherwise
  template<NodeType Node>
  using LabelType = std::conditional_t<LabeledNodeType<Node>, std::string, void>;
  
}
