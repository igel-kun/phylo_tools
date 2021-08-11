
#pragma once

#include "phylogeny.hpp"

namespace PT{
#warning TODO: if T is binary and its depth is less than 64, we can encode each path in vertex indices, allowing lightning fast LCA queries!

  template<StorageEnum _PredStorage,
           StorageEnum _SuccStorage,
           class _NodeData = void,
           class _EdgeData = void,
           class _LabelType = void,
           template<StorageEnum, StorageEnum, class, class, class> class _Node = PT::DefaultNode>
  using Network = Phylogeny<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType, singleS, _Node>;
  
  template<StorageEnum _PredStorage,
           StorageEnum _SuccStorage,
           StorageEnum _RootStorage,
           class _NodeData = void,
           class _EdgeData = void,
           class _LabelType = void,
           template<StorageEnum, StorageEnum, class, class, class> class _Node = PT::DefaultNode>
  using DAG = Phylogeny<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType, _RootStorage, _Node>;

  // if you have a tree and want a forest that uses the same NodeType, you can use these to declare the forest (or vice versa)
  template<PhylogenyType _Phylo,
    class _NodeData = typename _Phylo::NodeData,
    class _EdgeData = typename _Phylo::EdgeData,
    class _LabelType = typename _Phylo::LabelType>
  using CompatibleNetwork = Network<_Phylo::PredStorage, _Phylo::SuccStorage, _NodeData, _EdgeData, _LabelType>;

  template<PhylogenyType _Phylo,
           StorageEnum _RootStorage = _Phylo::RootStorage,
           class _NodeData = typename _Phylo::NodeData,
           class _EdgeData = typename _Phylo::EdgeData,
           class _LabelType = typename _Phylo::LabelType>
  using CompatibleDAG = DAG<_Phylo::PredStorage, _Phylo::SuccStorage, _RootStorage, _NodeData, _EdgeData, _LabelType>;

  // for convenience, provide defaults for predecessor and successor containers
  template<class _NodeData = void, class _EdgeData = void, class _LabelType = void>
  using DefaultNetwork = Network<vecS, vecS, _NodeData, _EdgeData, _LabelType>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelType = void>
  using DefaultDAG = DAG<vecS, vecS, vecS, _NodeData, _EdgeData, _LabelType>;

  // for convenience, provide defaults for predecessor and successor containers
  template<class _NodeData = void, class _EdgeData = void, class _LabelType = std::string>
  using DefaultLabeledNetwork = Network<vecS, vecS, _NodeData, _EdgeData, _LabelType>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelType = std::string>
  using DefaultLabeledDAG = DAG<vecS, vecS, vecS, _NodeData, _EdgeData, _LabelType>;

}
