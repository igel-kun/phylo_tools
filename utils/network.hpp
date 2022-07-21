
#pragma once

#include "phylogeny.hpp"

namespace PT{

  template<StorageEnum _PredStorage,
           StorageEnum _SuccStorage,
           class _NodeData = void,
           class _EdgeData = void,
           class _LabelType = void,
           template<StorageEnum, StorageEnum, class, class, class> class _Node = PT::DefaultNode>
  using Network = Phylogeny<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType, singleS, _Node>;
  
  // as opposed to a Network, a DAG may have multiple roots, so we take a 'RootStorage' template argument
  template<StorageEnum _PredStorage,
           StorageEnum _SuccStorage,
           StorageEnum _RootStorage,
           class _NodeData = void,
           class _EdgeData = void,
           class _LabelType = void,
           template<StorageEnum, StorageEnum, class, class, class> class _Node = PT::DefaultNode>
  using DAG = Phylogeny<_PredStorage, _SuccStorage, _NodeData, _EdgeData, _LabelType, _RootStorage, _Node>;

  // if you have a tree or network and want a network that uses the same Child-storage, you can use CompatibleNetwork to declare it easily
  // NOTE: if you pass a tree, this will change the parent-storage to be a vector
  template<StrictPhylogenyType _Phylo,
    class _NodeData = typename _Phylo::NodeData,
    class _EdgeData = typename _Phylo::EdgeData,
    class _LabelType = typename _Phylo::LabelType,
    StorageEnum _PredStorage = TreeType<_Phylo> ? vecS : _Phylo::PredStorage>
  using CompatibleNetwork = Network<_PredStorage, _Phylo::SuccStorage, _NodeData, _EdgeData, _LabelType>;

  // if you have a network or tree and want a DAG that uses the same NodeType, you can use CompatibleDAG to declare it easily
  template<StrictPhylogenyType _Phylo,
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
  template<class _NodeData = void, class _EdgeData = void, class _LabelType = std::string>
  using DefaultLabeledNetwork = Network<vecS, vecS, _NodeData, _EdgeData, _LabelType>;
  template<class _NodeData = void, class _EdgeData = void, class _LabelType = std::string>
  using DefaultLabeledDAG = DAG<vecS, vecS, vecS, _NodeData, _EdgeData, _LabelType>;

}
