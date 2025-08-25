
#pragma once

#include "phylogeny.hpp"

namespace PT{

  template<StorageEnum PredStorage_,
           StorageEnum SuccStorage_,
           class NodeData_ = void,
           class EdgeData_ = void,
           class LabelType_ = void,
           template<StorageEnum, StorageEnum, class, class, class> class Node_ = PT::DefaultNode>
  using Network = Phylogeny<PredStorage_, SuccStorage_, NodeData_, EdgeData_, LabelType_, singleS, Node_>;
  
  // as opposed to a Network, a DAG may have multiple roots, so we take a 'RootStorage' template argument
  template<StorageEnum PredStorage_,
           StorageEnum SuccStorage_,
           StorageEnum RootStorage_,
           class NodeData_ = void,
           class EdgeData_ = void,
           class LabelType_ = void,
           template<StorageEnum, StorageEnum, class, class, class> class Node_ = PT::DefaultNode>
  using DAG = Phylogeny<PredStorage_, SuccStorage_, NodeData_, EdgeData_, LabelType_, RootStorage_, Node_>;

  // if you have a tree or network and want a network that uses the same Child-storage, you can use CompatibleNetwork to declare it easily
  // NOTE: if you pass a tree, this will change the parent-storage to be a vector
  template<StrictPhylogenyType Phylo_,
    class NodeData_ = typename Phylo_::NodeData,
    class EdgeData_ = typename Phylo_::EdgeData,
    class LabelType_ = typename Phylo_::LabelType,
    StorageEnum PredStorage_ = TreeType<Phylo_> ? vecS : Phylo_::PredStorage>
  using CompatibleNetwork = Network<PredStorage_, Phylo_::SuccStorage, NodeData_, EdgeData_, LabelType_>;

  // if you have a network or tree and want a DAG that uses the same NodeType, you can use CompatibleDAG to declare it easily
  template<StrictPhylogenyType Phylo_,
           StorageEnum RootStorage_ = Phylo_::RootStorage,
           class NodeData_ = typename Phylo_::NodeData,
           class EdgeData_ = typename Phylo_::EdgeData,
           class LabelType_ = typename Phylo_::LabelType>
  using CompatibleDAG = DAG<Phylo_::PredStorage, Phylo_::SuccStorage, RootStorage_, NodeData_, EdgeData_, LabelType_>;


  // for convenience, provide defaults for predecessor and successor containers
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = void>
  using DefaultNetwork = Network<vecS, vecS, NodeData_, EdgeData_, LabelType_>;
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = void>
  using DefaultDAG = DAG<vecS, vecS, vecS, NodeData_, EdgeData_, LabelType_>;
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = std::string>
  using DefaultLabeledNetwork = Network<vecS, vecS, NodeData_, EdgeData_, LabelType_>;
  template<class NodeData_ = void, class EdgeData_ = void, class LabelType_ = std::string>
  using DefaultLabeledDAG = DAG<vecS, vecS, vecS, NodeData_, EdgeData_, LabelType_>;

}
