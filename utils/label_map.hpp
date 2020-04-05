

#pragma once

#include "utils.hpp"
#include "types.hpp"
#include "tree.hpp"
#include "network.hpp"

namespace PT {

  struct LeafLabelsOnlyT {};
  LeafLabelsOnlyT leaf_labels_only;

  // a label map maps labels (strings) to pairs of node-storages (Node for single-labeled networks, NodeSet for multi-labeled networks)
  template<class LabelStorageNet, class LabelStorageTree>
  class LabelMatching: public std::unordered_map<std::string, std::pair<LabelStorageNet, LabelStorageTree>>
  {
    using Parent = std::unordered_map<std::string, std::pair<LabelStorageNet, LabelStorageTree>>;

  public:

    template<class NodeIteratorN, class PropertyGetterN, class NodeIteratorT, class PropertyGetterT>
    LabelMatching(const LabeledNodeIterFactory<NodeIteratorN, PropertyGetterN>& Nfac,
              const LabeledNodeIterFactory<NodeIteratorT, PropertyGetterT>& Tfac):
      Parent()
    {
      for(const LabeledNode<>& p: Nfac){
        auto emp_res = Parent::emplace(p.second);
        append(emp_res.first->second.first, p.first);
        if(emp_res.second){
          if(std::is_integral_v<LabelStorageTree>)
            emp_res.first->second.second = (LabelStorageTree)(-1);
        } else {
          if(!std::is_integral_v<LabelStorageNet>)
            append(emp_res.first->second.first, p.first);
          else throw std::logic_error("single-label map for multi-labeled tree/network");
        }
              
      }
      for(const LabeledNode<>& p: Tfac){
        auto emp_res = Parent::emplace(p.second);
        append(emp_res.first->second, p.first);
        
        // if the label p.second has not been seen in N before, initialize the net-node-storage in the label-map
        if(emp_res.second){
          if(std::is_integral<LabelStorageNet>::value)
            emp_res.first->first = (LabelStorageNet)(-1);
        } else {
          if(std::is_integral<LabelStorageTree>::value && (emp_res.first->second == (LabelStorageTree)(-1)))
            throw std::logic_error("single-label map for multi-labeled network/tree");
        }
      }
    }


    template<class _Network, class _Tree>
    LabelMatching(const _Network& N, const _Tree& T):
      LabelMatching(N.get_nodes_labeled(), T.get_nodes_labeled())
    {}

    // prepend 'leaf_labels_only' to the argument list to construct label-maps only for the leaf-labels
    template<class _Network, class _Tree>
    LabelMatching(const LeafLabelsOnlyT& sentinel, const _Network& N, const _Tree& T):
      LabelMatching(N.get_leaves_labeled(), T.get_leaves_labeled())
    {}
  };

  template<class Network = RONetwork<>, class Tree = RONetwork<>>
  LabelMatching<typename Network::LabelStorage, typename Tree::LabelStorage> get_label_matching(const Network& N, const Tree& T)
  { return {N, T}; }

  template<class Network = RONetwork<>, class Tree = RONetwork<>>
  LabelMatching<typename Network::LabelStorage, typename Tree::LabelStorage> get_leaf_label_matching(const Network& N, const Tree& T)
  { return {leaf_labels_only, N, T}; }


}
