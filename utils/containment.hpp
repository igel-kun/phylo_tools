

#pragma once

#include <vector>
#include "utils.hpp"
#include "config.hpp"
#include "label_matching.hpp"
#include "induced_tree.hpp"
#include "matching.hpp"

namespace PT {
  
  // a containment checker, testing if a (possibly multi-labelled) host-tree contains a single-labeled guest tree
  template<class Host, class Guest, bool leaf_labels_only = true,
    class = std::enable_if_t<Guest::is_single_labeled>,
    class = std::enable_if_t<Guest::is_declared_tree>>
  class TreeInTreeContainment
  {
  protected:
    //NOTE: if the host is single-labelled, we'll be happy to have the DPTable point to singleton_sets instead of vectors of nodes
    using NodeList = LabelNodeStorage<typename Host::LabelTag, std::vector>;
    // we'll need some infos on the nodes, in particular, their depth (dist to root) and some order number
    using NodeInfos = InducedSubtreeInfoMap<Host>;
    // our DP table assigns each node in the guest a list of nodes in the host
    using DPTable = typename Guest::template NodeMap<NodeList>;
   
    const Guest& guest;
    const Host& host;

    std::shared_ptr<NodeInfos> node_infos;
    DPTable table;

  public:

    TreeInTreeContainment(const Host& _host, const Guest& _guest, std::shared_ptr<NodeInfos> _node_infos = {}):
      guest(_guest),
      host(_host),
      node_infos(std::move(_node_infos))
    {
      make_node_infos(_host, node_infos);
      for(const auto& x: *node_infos) std::cout << x.first << ":" << x.second.order_number << " "; std::cout << "\n";
      construct_base_cases();
    }


    // lookup where u (guest node) could be hosted; if u is not in the DP table yet, compute the entry
    const NodeList& who_displays(const Node u)
    {
      std::cout << "who displays "<<u<<"? ";
      const auto [iter, success] = table.try_emplace(u);
      if(success){
        std::cout << "\n";
        compute_possibilities(u, iter->second);
      } else std::cout << iter->second << "\n";
      return iter->second;
    }
    
    bool displayed() { return !who_displays(guest.root()).empty(); }

  protected:
    // in the subtree induced by the child possibilities, we'll need to keep track of which child of u can be displayed by one of our own children
    using MatchingPossibilities = HashMap<Node, std::unordered_bitset>;
    struct induced_subtree_infos {
      MatchingPossibilities nodes_for_poss;
      // try to register a new poss type; if the node has not already seen a poss of this type,
      //    then return the new # children of u for whom we have a possible child
      size_t register_child_poss(const Node child, const Node u_child)
      {
        const auto [it, success] = nodes_for_poss.try_emplace(u_child);
        if(success){
          it->second.set(child);
          return nodes_for_poss.size();
        } else return 0;
      }
      // mark the node uninteresting (by clearing node_for_poss), return whether it was already cleared before
      bool mark_uninteresting()
      {
        if(nodes_for_poss.empty()) return true;
        nodes_for_poss.clear();
        return false;
      }
    };

    std::function<bool(const Node,const Node)> sort_by_order = [&](const Node a, const Node b) {
      return node_infos->at(a).order_number > node_infos->at(b).order_number;
    };

    // construct the base-cases of the DP by matching leaf-labels (remember to tell the matching to store host-labels in a NodeVec (so we can move))
    void construct_base_cases()
    {
      auto lmatch = leaf_labels_only ? get_leaf_label_matching<Guest, Host, std::vector>(guest, host)
                                     : get_label_matching<Guest, Host, std::vector>(guest, host);
      for(auto& node_pair: seconds(lmatch)){
        assert(node_pair.first.size() == 1);
        std::flexible_sort(node_pair.second.begin(), node_pair.second.end(), sort_by_order);
        table.try_emplace(front(node_pair.first), std::move(node_pair.second));
      }
    }
    
    void compute_possibilities(const Node u, NodeList& poss)
    {
      assert(!guest.is_leaf(u));
      // step 1: get the subtree of host induced by the nodes that the children map to
      const NodeVec child_poss = merge_child_poss(u);
      std::cout << "building tree induced by "<<child_poss<<"\n";
      CompatibleRWTree<Host, induced_subtree_infos> induced_subhost(policy_noop, host, child_poss, node_infos);

      // step 2: find all nodes v such that each child of u has a possibility that is seen by a distinct leaf of v
      // register the possibilities for all but one child of u
      for(const Node u_child: guest.children(u)){
        for(Node v_child: who_displays(u_child)){
          // move upwards from v_child until we reach a node that's already seen a possibility for u_child
          Node v_par = induced_subhost.parent(v_child);
          while(induced_subhost[v_par].register_child_poss(v_child, u_child) && (v_par != induced_subhost.root())){
            v_child = v_par;
            v_par = induced_subhost.parent(v_par);
          }
        }
      }
      
      // step 3: go through the induced_subhost in postorder(!) and check containment for eligible nodes
      for(Node v: induced_subhost.dfs().postorder()){
        const auto& v_infos = induced_subhost[v];
        if(v_infos.nodes_for_poss.size() == guest.out_degree(u)) // if all children of u can be displayed below (including) v
          if(perfect_child_matching(v_infos.nodes_for_poss)){ // if each child of v can be displayed by a different child of u
            // H_v displays G_u \o/ - register and mark all ancestors uninteresting, so we don't run matching on them in the future
            poss.push_back(v);
            // if someone else already marked v unintersting, all ancestors are already marked as well
            while((v != induced_subhost.root()) && !induced_subhost[v = induced_subhost.parent(v)].mark_uninteresting());
          }
      }
      // make sure the nodes are in the correct order
      std::flexible_sort(poss.begin(), poss.end(), sort_by_order);

      std::cout << "found that "<< u << " is displayed at "<<poss<<"\n";
    }

    // return whether all children of u can be matched using the possibilities given
    bool perfect_child_matching(const MatchingPossibilities& poss)
    {
      return bipartite_matching<Node, MatchingPossibilities>(poss).maximum_matching().size() == poss.size();
    }

    NodeVec merge_child_poss(const Node u)
    {
      NodeVec poss;
      assert(!guest.is_leaf(u));
      // for degree up to x, merge the child possibilities by linear "inplace_merge", otherwise, merge via iterator-queue in O(n log deg)
      if(guest.out_degree(u) > config::vector_queue_merge_threshold){
        const auto sort_iter_by_order = [&](const auto& a, const auto& b) -> bool {  return sort_by_order(*a, *b); };
        using NodeIter = std::auto_iter<typename NodeList::const_iterator>;
        
        std::priority_queue<NodeIter, std::vector<NodeIter>, decltype(sort_iter_by_order)> iter_queue(sort_iter_by_order);
        size_t total_size = 0;
        for(const Node v: guest.children(u)){
          const auto& child_poss = who_displays(v);
          iter_queue.push(child_poss);
          total_size += child_poss.size();
        }
        poss.reserve(total_size);
        while(!iter_queue.empty()) {
          auto next_iter = iter_queue.top(); iter_queue.pop();
          poss.push_back(*next_iter);
          if(++next_iter) iter_queue.push(next_iter);
        }
      } else {
        for(const Node v: guest.children(u)) {
          const NodeList& v_poss = who_displays(v);
          const size_t old_size = poss.size();
          
          std::cout << "merging "; for(const auto& x: poss) std::cout << x <<":"<<node_infos->at(x).order_number<<" ";
          std::cout << "\t&\t";  for(const auto& x: v_poss) std::cout << x <<":"<<node_infos->at(x).order_number<<" "; std::cout << "\n";
          
          poss.insert(poss.end(), v_poss.begin(), v_poss.end());
          std::inplace_merge(poss.begin(), poss.begin() + old_size, poss.end(), sort_by_order);
        }
      }
      return poss;
    }


  };
}
