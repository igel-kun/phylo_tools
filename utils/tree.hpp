
#pragma once

#include "phylogeny.hpp"

namespace PT{
#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!

  template<StorageEnum SuccStorage_,
           class NodeData_ = void,
           class EdgeData_ = void,
           class LabelType_ = void,
           template<StorageEnum, StorageEnum, class, class, class> class Node_ = PT::DefaultNode>
  using Tree = Phylogeny<singleS, SuccStorage_, NodeData_, EdgeData_, LabelType_, singleS, Node_>;

  template<StorageEnum SuccStorage_,
           StorageEnum RootStorage_,
           class NodeData_ = void,
           class EdgeData_ = void,
           class LabelType_ = void,
           template<StorageEnum, StorageEnum, class, class, class> class Node_ = PT::DefaultNode>
  using Forest = Phylogeny<singleS, SuccStorage_, NodeData_, EdgeData_, LabelType_, RootStorage_, Node_>;

  // if you have a tree and want a forest that uses the same NodeType, you can use these to declare the forest (or vice versa)
  template<StrictPhylogenyType Phylo_,
    class NodeData_ = typename Phylo_::NodeData,
    class EdgeData_ = typename Phylo_::EdgeData,
    class LabelType_ = typename Phylo_::LabelType>
  using CompatibleTree = Tree<Phylo_::SuccStorage, NodeData_, EdgeData_, LabelType_>;

  template<StrictPhylogenyType Phylo_,
           StorageEnum RootStorage_ = Phylo_::RootStorage,
           class NodeData_ = typename Phylo_::NodeData,
           class EdgeData_ = typename Phylo_::EdgeData,
           class LabelType_ = typename Phylo_::LabelType>
  using CompatibleForest = Forest<Phylo_::SuccStorage, RootStorage_, NodeData_, EdgeData_, LabelType_>;

  // for convenience, provide defaults for predecessor and successor containers
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = void>
  using DefaultTree = Tree<vecS, NodeData_, EdgeData_, LabelType_>;
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = void>
  using DefaultForest = Forest<vecS, vecS, NodeData_, EdgeData_, LabelType_>;
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = std::string>
  using DefaultLabeledTree = Tree<vecS, NodeData_, EdgeData_, LabelType_>;
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = std::string>
  using DefaultLabeledForest = Forest<vecS, vecS, NodeData_, EdgeData_, LabelType_>;


}
