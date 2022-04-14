
#pragma once

#include <queue>
#include "config.hpp"
#include "auto_iter.hpp"
#include "matching.hpp"
#include "types.hpp"
#include "tree.hpp"
#include "label_matching.hpp"
#include "induced_tree.hpp"

namespace PT {

  //NOTE: if the host is single-labelled, we'll be happy to have the DisplayTable point to singleton_sets instead of vectors of nodes
  template<bool multi_possibility = true>
  using _NodeList = std::conditional_t<multi_possibility, NodeVec, NodeSingleton>;

  // a containment checker, testing if a (possibly multi-labelled) host-tree contains a single-labeled guest tree
  // NOTE: for now, only single-rooted hosts & guests are supported
  // NOTE: while Host & Guest must be trees, they are not necessarily DECLARED to be trees (a network without reticulation is fine)
  // NOTE: if you pass a non-const LabelMatching, we WILL move out of it; if that is not what you want, then pass the LabelMatching as const-reference
  template<StrictPhylogenyType Host,
           StrictPhylogenyType Guest,
           bool leaf_labels_only = true,
           StorageEnum HostLabelStorage = singleS,
           class _NodeInfos = InducedSubtreeInfoMap>
      requires (std::is_same_v<typename Host::LabelType, typename Guest::LabelType> && Host::has_unique_root && Guest::has_unique_root)
  class TreeInTreeContainment {
  public:
    static constexpr bool multi_labeled_host = (HostLabelStorage != singleS);
    static constexpr StorageEnum GuestLabelStorage = singleS; // so far we can only support single-labelled guests
    static constexpr StorageEnum MSTreeLabelStorage = vecS;

    using LabelType = typename Host::LabelType;
    using LabelMatching = PT::LabelMatching<Host, Guest, MSTreeLabelStorage, GuestLabelStorage>;
    
    using NodeList = _NodeList<multi_labeled_host>;
    using DisplayTable = NodeMap<_NodeList<multi_labeled_host>>;
    
    // we'll need some infos on the nodes, in particular, their depth (dist to root) and some order number
    using NodeInfos = _NodeInfos;
    
   
  protected:
    const Guest& guest;
    const Host& host;

    NodeInfos node_infos;
    DisplayTable table;



    static auto compute_label_matching(const Host& host, const Guest& guest) {
      if constexpr (leaf_labels_only)
        return LabelMatching(host.leaves(), guest.leaves());
      else
        return LabelMatching(host.nodes(), guest.nodes());
    }

  public:

    template<LabelMatchingType LabelMatchingInit = LabelMatching, class NodeInfoInit = _NodeInfos>
    TreeInTreeContainment(const Host& _host,
                          const Guest& _guest,
                          LabelMatchingInit&& host_guest_label_match,
                          NodeInfoInit&& _node_infos = {}):
      guest(_guest),
      host(_host),
      node_infos(std::forward<NodeInfoInit>(_node_infos))
    {
      // step 1: setup node_infos
      if(!guest.empty()) {
        if(node_infos.empty()) get_induced_subtree_infos(_host, node_infos);
        std::cout << node_infos.size() << " node infos: " << node_infos<<"\n";
        std::cout << "label matching: " << host_guest_label_match << "\n";
        
        // step 2: construct base cases
        for(auto& HG_pair: seconds(host_guest_label_match)){
          std::flexible_sort(HG_pair.first.begin(), HG_pair.first.end(), sort_by_order);
          std::cout << "base case: "<<HG_pair<<"\n";
          append(table, front(HG_pair.second), std::move(HG_pair.first));
        }
      }
    }

    template<class NodeInfoInit = _NodeInfos> requires (!LabelMatchingType<NodeInfoInit>)
    TreeInTreeContainment(const Host& _host, const Guest& _guest, NodeInfoInit&& _node_infos = {}):
      TreeInTreeContainment(_host, _guest, compute_label_matching(_host, _guest), std::forward<NodeInfoInit>(_node_infos))
    {}

    // lookup where the guest node u could be hosted; if u is not in the DP table yet, compute the entry
    const NodeList& who_displays(const NodeDesc u) {
      std::cout << "who displays "<<u<<"? ";
      const auto [iter, success] = append(table, u);
      if(success) {
        std::cout << "\n";
        compute_possibilities(u, iter->second);
      } else std::cout << iter->second << "\n";
      return iter->second;
    }
    
    bool displayed() { return !who_displays(guest.root()).empty(); }

  protected:
    // in the subtree induced by the child possibilities, we'll need to keep track of which child of u can be displayed by one of our own children
    using MatchingPossibilities = NodeMap<NodeSet>;

    struct matching_infos {
      MatchingPossibilities nodes_for_poss;
      // try to register a new poss type; if the node has not already seen a poss of this type,
      //    then return the new # children of u for whom we have a possible child
      size_t register_child_poss(const NodeDesc child, const NodeDesc u_child) {
        const auto [it, success] = nodes_for_poss.try_emplace(u_child);
        append(it->second, child);
        std::cout << "marking that "<<child<<" displays "<<u_child<<" poss' of "<<u_child<<" currently: "<<it->second<<"\n";
        return success ? nodes_for_poss.size() : 0;
      }
      // mark the node uninteresting (by clearing node_for_poss), return whether it was already cleared before
      bool mark_uninteresting() {
        if(nodes_for_poss.empty()) return true;
        nodes_for_poss.clear();
        return false;
      }
    };

    std::function<bool(const NodeDesc, const NodeDesc)> sort_by_order = [&](const NodeDesc a, const NodeDesc b) {
      return node_infos.at(a).order_number < node_infos.at(b).order_number;
    };
    
    void compute_possibilities(const NodeDesc u, NodeList& poss) {
      // step 1: get the subtree of host induced by the nodes that the children map to
      const auto child_poss = merge_child_poss(u);
      if(!child_poss.empty()) {
        if(guest.out_degree(u) > 1) {
          std::cout << "building tree induced by "<<child_poss<<"\n";
          CompatibleTree<Host, matching_infos> induced_subhost(get_induced_edges(host, child_poss, node_infos));
          std::cout << "induced tree:\n"<<induced_subhost<<"\n";
          if(!induced_subhost.edgeless()) {
            // step 2: find all nodes v such that each child of u has a possibility that is seen by a distinct leaf of v
            // register the possibilities for all but one child of u
            for(const NodeDesc u_child: guest.children(u)) {
              for(NodeDesc v_child: who_displays(u_child)) {
                // move upwards from v_child until we reach a node that's already seen a possibility for u_child
                while(v_child != induced_subhost.root()) {
                  const NodeDesc v_parent = induced_subhost.parent(v_child);
                  if(!induced_subhost[v_parent].data().register_child_poss(v_child, u_child)) break;
                  v_child = v_parent;
                }
              }
            }
          
            // step 3: go through the induced_subhost in postorder(!) and check containment for eligible nodes
            for(NodeDesc v: induced_subhost.nodes_postorder()){
              const auto& v_infos = induced_subhost[v].data();
              if(v_infos.nodes_for_poss.size() == guest.out_degree(u)){ // if all children of u have a child of u that can display them
                std::cout << "making bipartite matching from "<<v_infos.nodes_for_poss<<"\n";
                if(perfect_child_matching(v_infos.nodes_for_poss)){ // if each child of v can be displayed by a different child of u
                  // H_v displays G_u \o/ - register and mark all ancestors uninteresting, so we don't run matching on them in the future
                  poss.push_back(v);
                  // if someone else already marked v unintersting, all ancestors are already marked as well
                  while((v != induced_subhost.root()) && !induced_subhost[v = induced_subhost.parent(v)].data().mark_uninteresting());
                }
              }
            }
            // make sure the nodes are in the correct order
            std::flexible_sort(poss.begin(), poss.end(), sort_by_order);
          } else poss.clear(); // if the induced tree is edgeless but there are at least 2 children of u in guest, then u is not displayed
        } else append(poss, child_poss); // if u has a single child, then u maps where this child maps
      } else poss.clear(); // if no child of u can be mapped, then u cannot be mapped either

      std::cout << "found that "<< u << " is displayed at "<<poss<<"\n";
    }

    // return whether all children of u can be matched using the possibilities given
    bool perfect_child_matching(const MatchingPossibilities& poss) {
      return bipartite_matching<MatchingPossibilities>(poss).maximum_matching().size() == poss.size();
    }

    // merge the mapping possibilities of all childs into one vector; unless one of the children cannot be mapped, in which case return the empty vector
    NodeVec merge_child_poss(const NodeDesc u) {
      using NodeIter = std::auto_iter<typename NodeList::const_iterator>;
      // if u is a leaf, it should be managed by the base case, unless its label is not in the host, in which case it's not displayed
      NodeVec poss;
      if(!guest.is_leaf(u)){
        std::cout << "merging possibilities of "<<guest.children(u)<<"\n";
        // for degree up to x, merge the child possibilities by linear "inplace_merge", otherwise, merge via iterator-queue in O(n log deg)
        if(guest.out_degree(u) > config::vector_queue_merge_threshold){
          // NOTE: priority_queue outputs the LARGEST element first, so we'll have to reverse sort_by_order by swapping its arguments
          const auto sort_iter_by_order = [&](const auto& a, const auto& b) -> bool {  return sort_by_order(*b, *a); };
          
          std::priority_queue<NodeIter, std::vector<NodeIter>, decltype(sort_iter_by_order)> iter_queue(sort_iter_by_order);
          size_t total_size = 0;
          // for each child v of u, add an auto iter to its possibility list
          for(const NodeDesc v: guest.children(u)){
            const auto& child_poss = who_displays(v);
            if(!child_poss.empty()){
              iter_queue.push(child_poss);
              total_size += child_poss.size();
            } else {
              // if v cannot be displayed, then u cannot be displayed, so return the empty possibility vector
              poss.clear();
              return poss;
            }
          }
          poss.reserve(total_size);
          while(!iter_queue.empty()) {
            NodeIter next_iter = value_pop(iter_queue);
            poss.push_back(*next_iter);
            if(++next_iter) iter_queue.push(next_iter);
          }
        } else {
          for(const NodeDesc v: guest.children(u)) {
            const NodeList& v_poss = who_displays(v);
            if(!v_poss.empty()){
              const size_t old_size = poss.size();
              std::cout << "merging "; for(const auto& x: poss) std::cout << x <<":"<<node_infos.at(x).order_number<<" ";
              std::cout << "\t&\t";  for(const auto& x: v_poss) std::cout << x <<":"<<node_infos.at(x).order_number<<" "; std::cout << "\n";
              
              poss.insert(poss.end(), v_poss.begin(), v_poss.end());
              std::inplace_merge(poss.begin(), poss.begin() + old_size, poss.end(), sort_by_order);
            } else {
              poss.clear();
              return poss;
            }
          }
        }
      }
      return poss;
    }


  };



}
