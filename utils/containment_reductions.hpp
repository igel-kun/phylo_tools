
#pragma once

#include "predicates.hpp"
#include "types.hpp"
#include "tree_tree_containment.hpp"
#include "tree_comp_containment.hpp"

namespace PT {

  template<class T>
  concept Reduction = requires(T t) { t.apply(); };

  template<class Manager>
  struct ContainmentReduction {
    using Host = typename Manager::Host;
    using Guest = typename Manager::Guest;
    using LabelMatching = typename Manager::Containment::LabelMatching;
    using LabelType = LabelTypeOf<Host>;
    
    Manager& manager;

    ContainmentReduction(Manager& c): manager(c) {}

#warning "TODO: not all containment reductions need access to component roots and visible leaves"
    NodeDesc comp_root_of(const NodeDesc u) const { return manager.contain.comp_info.comp_root_of(u); }
    NodeDesc visible_leaf_of(const NodeDesc u) const { return manager.contain.comp_info.visible_leaf_of(u); }
  };

  template<class Manager>
  struct ContainmentReductionWithNodeQueue: public ContainmentReduction<Manager> {
    using Parent = ContainmentReduction<Manager>;
    using Parent::Parent;

    NodeSet node_queue;
    void add(const NodeDesc x) {
      mstd::append(node_queue, x);
      //std::cout << "adding node "<<x<<" to queue, now: "<<node_queue<<"\n";
    }
    
    // prepare first round of reticulations to merge
    void init_queue() {}
  };


  // a class to remove orphaned leaves
  template<class Manager>
  struct OrphanRemover: public ContainmentReductionWithNodeQueue<Manager> {
    using Parent = ContainmentReductionWithNodeQueue<Manager>;
    using typename Parent::Host;
    using Parent::manager;
    using Parent::node_queue;
    using Parent::comp_root_of;
    using Parent::add;

    // NOTE: as this is part of orphan cleaning, we cannot make any assumptions about consistency in the structure!
    void suppress_leaf_in_host(const NodeDesc u) {
      assert(Host::out_degree(u) == 0);

      for(const NodeDesc pu: Host::parents(u))
        if((Host::out_degree(pu) <= 2) && (Host::label(pu).empty()))
          add(pu);

      manager.remove_from_comp_DAG(u);
      manager.remove_from_queues(u);
      manager.contain.host.remove_node(u);
    }

    // NOTE: as this is part of orphan cleaning, we cannot make any assumptions about consistency in the structure!
    void suppress_node_in_host(const NodeDesc u) {
      auto& host = manager.contain.host;
      auto& info = manager.contain.comp_info;
      assert(host.in_degree(u) <= 1);
      assert(host.out_degree(u) == 1);

      const NodeDesc u_child = Host::child(u);
      std::cout << "suppressing node "<<u<<" with child "<<u_child<<"\n";
      if(Host::in_degree(u_child) == 1) {
        if(!Host::label(u_child).empty()) {
          // if u_child is a leaf, we will have to contract u down into it
          // NOTE: however, if u is a component root, we'll need to remove it from the comp-DAG before
          if(info.comp_root_of(u) == u) {
            std::cout << "suppressing node "<<u<<" with leaf-child "<<u_child<<" and comp-root "<<info.comp_root_of(u)<<"\n";
            // if u is a component-root, then it should be a leaf in the comp_DAG, which we can remove now
            assert(info.N_to_comp_DAG.contains(u));
            manager.remove_from_comp_DAG(u);
            info.replace_comp_root(u, u_child);
            std::cout << "changed comp-root of "<<u_child<<" to "<<info.comp_root_of(u_child)<<"\n";
          }
          // then, we can safely contract into u's child
          std::cout << "contracting-down "<<u<<" into "<<u_child<<"\n";
          manager.remove_from_queues(u);
          host.contract_down(u, u_child);
        } else {
          // if u_child is not a leaf, we will contract it into u
          std::cout << "contracting-up "<<u_child<<" into "<<u<<"\n";
          manager.remove_from_queues(u_child);
          host.contract_up(u_child, u);
          // if u is still suppressible, re-run for u
          if((Host::in_degree(u) <= 1) && (Host::out_degree(u) <= 1)) add(u);
        }
      } else {
        manager.remove_from_queues(u);
        // if u's child is a reticulation then contract u upwards
        assert(host.is_reti(u_child));
        manager.triangle_rule.add(u_child);
        // NOTE: if everything is consistent then u's parent is a reti if and only if u is a comp root
        const NodeDesc u_parent = host.parent(u);
        std::cout << u << " has parent "<<u_parent<<" and reticulation child "<<u_child << "\n";
        std::cout << "contracting-up "<<u<<" into "<<u_parent<<"\n";
        if(info.comp_root_of(u) == u) {
          assert(info.N_to_comp_DAG.contains(u));
          manager.remove_from_comp_DAG(u);
          // NOTE: since both u's child and parent are reticulations, we also want to contract u's parent down onto u's child
          host.contract_up(u, u_parent);
          if(Host::in_degree(u_parent) > 1)
            manager.contract_reti_onto_reti_child(u_parent);
          else add(u_parent);
        } else {
          // NOTE: it might be that u_child is already a child of u_parent, so u_parent and u_child might become orphans
          if(host.contract_up_unique(u, u_parent)) {
            // NOTE: if we just removed a "double-edge", we'll have to inform the tree-component infos about this
            //  in particular, the child of u_child may now have a different component-root, so update that
            manager.contain.comp_info.react_to_edge_deletion(u_parent, u_child);
            if(Host::out_degree(u_parent) <= 1)
              add(u_parent);
            if(Host::in_degree(u_child) == 1)
              add(u_child);
          }
        }
      }
    }

    // remove leaves without label from the host
    bool apply() {
      auto& host = manager.contain.host;
      bool result = false;
      while(!node_queue.empty()){
        std::cout << "removing orphans from "<<node_queue<<" in\n";
        std::cout << host <<"\n";
        std::cout << "comp-roots:\n"; for(const NodeDesc z: manager.contain.host.nodes()) std::cout << z <<": " << manager.contain.comp_info.comp_root_of(z) <<'\n';
        const NodeDesc v = mstd::value_pop(node_queue);
        std::cout << "next orphan: "<<v<<" with comp root "<<comp_root_of(v)<<"\n";
        assert(host.label(v).empty());
        assert(host.out_degree(v) <= 1);
        assert((host.out_degree(v) == 0) || (host.in_degree(v) <= 1));
        // if u is unlabeled, queue its parents and remove u itself
        if(host.out_degree(v) == 0) {
          suppress_leaf_in_host(v);
        } else suppress_node_in_host(v);
        result = true;
      }
      return result;
    }

    void init_queue() {
      const auto& host = manager.contain.host;
      for(const NodeDesc x: manager.contain.host.nodes())
        if((host.in_degree(x) <= 1) && (host.out_degree(x) <= 1) && (host.label(x).empty()))
          Parent::add(x);
    }

  };


  // a class to merge adjacent reticulations 
  template<class Manager>
  struct ReticulationMerger: public ContainmentReductionWithNodeQueue<Manager> {
    using Parent = ContainmentReductionWithNodeQueue<Manager>;
    using typename Parent::Host;
    using Parent::manager;
    using Parent::node_queue;
  
    // if the child y of x is a reti, then contract x down to y
    bool contract_reti(const NodeDesc x) {
      assert(manager.contain.host.out_degree(x) == 1);
      const NodeDesc y = manager.contain.host.child(x);
      if(manager.contain.host.out_degree(y) == 1) {
        manager.contract_reti_onto_reti_child(x);
        return true;
      }
      return false;
    }

    // simplify host by contracting adjacent retis
    bool apply() {
      bool result = false;
      while(!node_queue.empty()){
        std::cout << "getting next node from "<<node_queue<<"\n";
        const NodeDesc x = mstd::value_pop(node_queue);
        std::cout << "trying to contract reticulation "<<x<<'\n';
        // NOTE: please be sure that the nodes in the node_queue really exist in the host (have not been removed by other reductions)!
        if((manager.contain.host.out_degree(x) == 1) && contract_reti(x))
          result = true;
      }
      return result;
    }

    // prepare first round of reticulations to merge
    void init_queue() {
      const auto& host = manager.contain.host;
      for(const NodeDesc x: manager.contain.host.nodes()) {
        if((host.out_degree(x) == 1) && (host.out_degree(host.any_child(x)) == 1)){
          Parent::add(x);
        }
      }
    }

  };


  // a class to apply triangle reduction
  template<class Manager>
  struct TriangleReducer: public ContainmentReductionWithNodeQueue<Manager> {
    using Parent = ContainmentReductionWithNodeQueue<Manager>;
    using typename Parent::Host;
    using Parent::manager;
    using Parent::node_queue;

    bool apply() {
      std::cout << "\tTRIANGLE: applying reduction...\n";
      while(!node_queue.empty()){
        if(triangle_rule(mstd::value_pop(node_queue)))
          return true;
      }
      return false;
    }

    // triangle rule:
    // if z has 2 parents x and y s.t. xy is an arc and x has indeg-1 and outdeg-2 & y has outdeg at most 2, then remove the arc xz
    bool triangle_rule(const NodeDesc z, const NodeDesc y) {
      assert(Host::out_degree(z) == 1);
      if(Host::out_degree(y) <= 2){
        std::cout << "applying triangle-rule to "<<z<< " & "<<y<<"\n";
        for(const NodeDesc x: Host::parents(y)){
          if((Host::out_degree(x) == 2) && Host::is_edge(x, z)) {
            manager.remove_edge_in_host(x, z);
            return true;
          }
        }
      } 
      return false;
    }

    bool triangle_rule(const NodeDesc z) {
      std::cout << "applying triangle-rule to "<<z<< "\n";
      assert(Host::in_degree(z) > 1);
      bool result = false;
      if(Host::out_degree(z) == 1){
        bool local_result;
        do {
          local_result = false;
          for(const NodeDesc y: Host::parents(z))
            if(triangle_rule(z, y)) { // if the triangle rule applied, then we messed with z's parents so we have to exit the for-each here
              result = local_result = true;
              break;
            }
        } while(local_result);
      } 
      return result;
    }
  };

  // a class for containment cherry reduction
  template<class Manager>
  struct CherryPicker: public ContainmentReductionWithNodeQueue<Manager> {
    using Parent = ContainmentReductionWithNodeQueue<Manager>;
    using Parent::Parent;
    using typename Parent::Host;
    using typename Parent::Guest;
    using typename Parent::LabelMatching;
    using typename Parent::LabelType;
    using Parent::manager;
    using Parent::node_queue;
    using Parent::comp_root_of;
    using Parent::visible_leaf_of;
    using Parent::add;

    using LabelSet = HashSet<AsMapKey<LabelType>>;

    bool apply() {
      std::cout << "\tCHERRY: applying reduction...\n";
      while(!node_queue.empty()){
        if(simple_cherry_reduction(mstd::value_pop(node_queue)))
          return true;
      }
      return false;
    }

    void init_queue() {
      // put all labeled nodes of the host in the queue
      for(const auto uv: manager.contain.HG_label_match){
        const auto& U = uv.second.first;
        assert(U.size() == 1);
        add(U.front());
      }
    }

    bool simple_cherry_reduction(const NodeDesc u) {
      std::cout << "cherry on node "<<u<<"\n";
      const auto& label_match = manager.find_label_in_host(u);
      std::cout << "using label-match " << *label_match << "\n";
      return simple_cherry_reduction_from(label_match);
    }
#warning "TODO: make all functions static that don't need the node_queue"

    // return the parent of u, the parent of v, and whether all degrees work out
    static std::tuple<NodeDesc, NodeDesc, bool> label_matching_sanity_check(const NodeDesc host_x, const NodeDesc guest_x) {
      std::tuple<NodeDesc, NodeDesc, bool> result{NoNode, NoNode, false};

      if(Host::in_degree(host_x) == 1) {
        const NodeDesc host_px = Host::parent(host_x);
        std::get<0>(result) = host_px;
        if(Host::in_degree(host_px) <= 1) std::get<2>(result) = true;
      }
      
      if(Guest::in_degree(guest_x) == 1) {
        std::get<1>(result) = Guest::parent(guest_x);
      } else std::get<2>(result) = false;

      return result;
    }

#warning "TODO: try to work with NodeDesc's instead of labels here"
    static LabelSet get_labels_of_children(const NodeDesc pv) {
      LabelSet result;
      for(const NodeDesc x: Guest::node_of(pv).children()) {
        const auto& x_node = Guest::node_of(x);
        // if pv is parent of a non-leaf in the guest, then cancel rule application by returning the empty set
        if(x_node.out_degree() != 0) {
          result.clear();
          break;
        }
        mstd::append(result, x_node.label());
      }
      return result;
    }

    bool simple_cherry_reduction_from(const mstd::iterator_of_t<LabelMatching>& uv_label_iter) {
      std::cout << "\tCHERRY: checking label matching "<<*uv_label_iter<<"\n";
      const auto& UV = uv_label_iter->second;
      const auto& [U, V] = UV;
      assert(U.size() == 1);
      assert(V.size() == 1);
      const NodeDesc u = U.front();
      const NodeDesc v = V.front();
      const auto [pu, pv, success] = label_matching_sanity_check(u, v);
     
      if(success) {
        // degrees match, so see if the labels of the children match
        // step 1: compute a set of leaves directly below the parent of v (in guest)
        LabelSet seen = get_labels_of_children(pv);
        if(!seen.empty()) {
          //NOTE: detect reticulated cherries by passing through reticulations
          const auto& puC = Host::children(pu);
          std::cout << "\tCHERRY: considering children "<<puC<<" of "<<pu<<" in the host (leaves in guest: "<<seen<<")\n";
          if(puC.size() >= seen.size()){
            const auto& host = manager.contain.host;

            // step 1: collect all labels directly below x
            NodeVec edge_removals;
            for(const NodeDesc x: puC) {
              NodeDesc y = x;
              // if x is a reti, get the tree-node below x
              while(Host::out_degree(y) == 1) y = Host::any_child(y);
              // if we've arrived at a node that is visible on a leaf that is not below pv, then pu-->x will never be in an embedding of guest in host!
              const NodeDesc vis_leaf = visible_leaf_of(y);
              std::cout << "checking visible leaf "<<vis_leaf<<" of "<<y<<" for child "<<x<<" of "<<pu<<"\n";
              assert((vis_leaf != NoNode) || Host::label(y).empty()); // if x has a label, it should have a visible leaf
              if(vis_leaf != NoNode) {
                const auto& vis_label = Host::label(vis_leaf);
                std::cout << "\tCHERRY: using visible-leaf "<<vis_leaf<<" with label "<<vis_label<<", seen = "<<seen<<"\n";
                const auto seen_iter = seen.find(vis_label);
                if(seen_iter == seen.end()) {
                  mstd::append(edge_removals, x);
                } else seen.erase(seen_iter);
              }
            }
            // step 2: remove edges to nodes that see a label that is not below pv
            if(!edge_removals.empty()) {
              std::cout << "\tCHERRY: want to delete edges from "<<pu<<" to nodes in "<<edge_removals<<"\n";
              for(const NodeDesc x: edge_removals) {
                assert(Host::is_edge(pu,x));
                if(Host::in_degree(x) == 1) {
                  manager.contain.failed = true;
                  std::cout << "\tCHERRY: refusing to delete edge from "<<pu<<" to "<<x<<" since "<<x<<" is visible from "<<visible_leaf_of(x)<<" - containment is impossible\n";
                  return true;
                } else manager.remove_edge_in_host(pu, x);
              }
              if(!edge_removals.empty()) std::cout << "new host is:\n" << host <<'\n';
            }
            // if we found all labels occuring below pv also below pu, then we'll apply the cherry reduction
            if(seen.empty()) {
              std::cout << "\tCHERRY: found (reticulated) cherry at "<<pu<<" (host) and "<<pv<<" (guest)\n";
              // step 1: fix visibility labeling to u, because the leaf that pu's root is visible from will not survive the cherry reduction (but u will)
              manager.contain.comp_info.replace_visible_leaf(comp_root_of(pu), u);
              // step 2: prune host and guest
              manager.HG_match_rule.match_nodes(pu, pv, uv_label_iter);
              std::cout << "\tCHERRY: after reduction:\nhost:\n"<<host<<"guest:\n"<<manager.contain.guest<<"\n";
              std::cout << "comp-roots:\n"; for(const NodeDesc z: manager.contain.host.nodes()) std::cout << z <<": " << manager.contain.comp_info.comp_root_of(z) <<'\n';
            } else return !edge_removals.empty();
          } else manager.HG_match_rule.match_nodes(pu, v, uv_label_iter); // if puC is smaller than seen, then pu cannot display pv, so pu has to display v
          return true;
        } else return false; // if get_labels_of_children returned the empty set, this indicates that pv has a child that is not a leaf, so give up
      } else return false; // if label_matching_sanity_check failed, then give up
    } 

  };

  // a class to match visible components of the host with subtrees of the guest
  template<class Manager, bool leaf_labels_only = true>
  struct VisibleComponentRule: public ContainmentReduction<Manager> {
    using Parent = ContainmentReduction<Manager>;
    using ComponentDAG = typename Manager::ComponentDAG;
    using typename Parent::Host;
    using typename Parent::Guest;
    using typename Parent::LabelMatching;
    using typename Parent::LabelType;
    using Parent::Parent;
    using Parent::manager;
    using Parent::comp_root_of;
    using Parent::visible_leaf_of;

    using MulSubtree = DefaultLabeledTree<>;

    void treat_comp_root(const NodeDesc u) {
      TreeInComponent<MulSubtree, Guest, leaf_labels_only> tree_comp_display(manager.contain.host, u, manager.contain.guest, manager.contain.HG_label_match);
      
      const NodeDesc vis_leaf = visible_leaf_of(u);
      assert(vis_leaf != NoNode);
      const auto& vlabel = manager.contain.host.label(vis_leaf);

      std::cout << "using visible leaf "<<vis_leaf<<" with label "<<vlabel<<"\n";
      std::cout << "label matching: "<<manager.contain.HG_label_match<<"\n";

      const auto uv_label_iter = manager.find_label(vlabel);
      assert(uv_label_iter != manager.contain.HG_label_match.end());

      // step 2: get the highest ancestor v of vis_leaf in T s.t. T_v is still displayed by N_u
      const auto& matched_leaves = uv_label_iter->second.second;
      assert(matched_leaves.size() == 1);
      const NodeDesc visible_leaf_in_guest = mstd::front(matched_leaves);
      std::cout << "finding highest node displaying "<<visible_leaf_in_guest<<"\n";
      const NodeDesc v = tree_comp_display.highest_displayed_ancestor(visible_leaf_in_guest);
      std::cout << v << " is the highest displayed ancestor (or it's the root) - label: "<<vlabel<<"\n";

      std::cout << "successfully treated comp root "<<u<<"\n";

      // step 3: replace both N_u and T_v by a leaf with the label of vis_leaf
      manager.HG_match_rule.match_nodes(u, v, uv_label_iter);
    }


    // a component root x is called "1/2-eligible" if, for each child y of x in the component-DAG, the x-y-path is unique in host and y is 1/2-eligible
    // a component root x is called "eligible" if it is 1/2-eligible and it is visible from a leaf
    // a pair (v, 10) in PathProfile[u] means that there are 10 different u-->v paths in host
    using PathProfile = NodeMap<NodeMap<size_t>>;

    // return whether u is half-eligible and update u's paths
    //NOTE: the update to num_paths[u] are incorrect if u is not half-elighible!
    bool is_half_eligible(const NodeDesc u, PathProfile& num_paths, const NodeSet& half_eligible) const {
      auto& u_paths = num_paths[u];
      std::cout << "checking node "<<u<<" ("<< comp_root_of(u) <<") with current paths "<<u_paths<<"\n";
      const auto& host = manager.contain.host;
      const NodeDesc u_in_cDAG = manager.contain.comp_info.N_to_comp_DAG.at(u);
      for(const NodeDesc cDAG_v: ComponentDAG::children(u_in_cDAG)) {
        if(const NodeDesc v = ComponentDAG::data(cDAG_v); mstd::test(half_eligible, v)) {
          std::cout << "next child: "<<v<<"\n";
          // to test if the u-v-path in host is unique, we'll look at all reticulations above v in host and check if we see u more than once
          assert(host.in_degree(v) == 1);
          NodeDesc r = host.parent(v);
          if(r != u) { // if v is also the child of u in host (!) then the u-v-path is unqiue
            NodeVec retis_above{r};
            while(!retis_above.empty()){
              r = mstd::value_pop(retis_above);
              std::cout << "exploring reti "<<r<<" ("<<manager.contain.comp_info.comp_root_of(r)<<") above "<<v<<"\n";
              if(host.in_degree(r) <= 1) {
                assert(comp_root_of(r) != NoNode);
                num_paths[comp_root_of(r)][v]++;
                std::cout << "registered "<< comp_root_of(r) <<"--"<<v<<" path. Now, "<<u<<" has "<<num_paths[comp_root_of(r)][v]<<" of them\n";
              } else mstd::append(retis_above, host.parents(r));
            }
            assert(u_paths.contains(v));
            if(u_paths.at(v) > 1) return false; // if we ended up counting more than one path, return non-eligibility
          } else u_paths.at(v) = 1;
          // for each root that v has paths to, u also has paths to via v
          for(const auto& [w, num_w_paths]: num_paths[v]) {
            if(const auto iter = u_paths.find(w); iter != u_paths.end())
              if((iter->second += num_w_paths) > 1)
                return false;
          }
          std::cout << "added paths to "<<u<<"'s paths, now: "<<u_paths<<"\n";
        } else return false;
      } // for all children of u in the comp-DAG
      std::cout << u<< " 1/2-eligible with paths: "<<u_paths<<'\n';
      // if we reached here, we know that u is half-eligible... now, if u is also visible from a leaf, its fully eligible and we'll return it
      return true;
    }

    NodeDesc get_eligible_component_root() {
      assert(!manager.contain.comp_info.comp_DAG.empty());
      if(!manager.contain.comp_info.comp_DAG.edgeless()) {
        // cache how many paths there are between a parent (first parameter) and a child (second parameter)
        PathProfile num_paths;
        NodeSet half_eligible;
        for(const NodeDesc cDAG_u: manager.contain.comp_info.comp_DAG.nodes_postorder()) {
          const NodeDesc u = node_of<ComponentDAG>(cDAG_u).data(); 
          if(is_half_eligible(u, num_paths, half_eligible)) {
            if(visible_leaf_of(u) == NoNode) {
              std::cout << u << " is 1/2-eligible\n";
              mstd::append(half_eligible, u);
            } else return u;
          } else std::cout << u<< " not eligible\n";
        }
        // if we never found an eligible node, then return failure
        return NoNode;
      } else return manager.contain.host.root();
    }

    // visible tree-component reduction: find a lowest visible tree component C and reduce it in O(|C|) time; return true if network & tree changed
    bool apply() {
      std::cout << "\tVISIBLE COMPONENTS: applying reduction...\n";
      const NodeDesc rt = get_eligible_component_root();
      if(rt != NoNode){
        std::cout << "tree-component rule with eligible node "<< rt <<" ("<< manager.contain.comp_info.comp_root_of(rt)<<")\n";
        std::cout << "on tree-component DAG:\n";
        manager.contain.comp_info.comp_DAG.print_subtree_with_data();

        std::cout << "host:\n"<<manager.contain.host<<"\n";
        std::cout << "guest:\n"<<manager.contain.guest<<"\n";
        treat_comp_root(rt);
        return true;
      } else return false;
    } 

  };

  // whenever a node in the host is identified that optimally displays a node in the guest, this class updates host and guest accordingly
  template<class Manager>
  struct HostGuestMatch {
    using Host = typename Manager::Containment::Host;
    using Guest = typename Manager::Containment::Guest;
    using LabelMatching = typename Manager::Containment::LabelMatching;

    Manager& manager;
   
    HostGuestMatch(Manager& c): manager(c) {}

    // prune the guest below Node x, 
    // NOTE: return a list of nodes in the host corresponding to removed leaves in the guest
    // ATTENTION: also remove the corresponding entries in HG_label_match
    NodeVec prune_guest_except(const NodeDesc x, const NodeDesc except) {
      std::cout << "pruning "<<x<<" in guest (except "<<except<<")\n";
      NodeVec host_leaves;
      NodeVec to_suppress;
      for(const NodeDesc l: Guest::leaves_below(x)) if(l != except){
        // if x has a label that matches in the host, then remove the corresponding node from the host and remove the matching entry as well
        const auto HG_match_iter = manager.find_label_in_guest(l);
        if(HG_match_iter != manager.contain.HG_label_match.end()) {
          const auto& host_matched = HG_match_iter->second.first;
          assert(host_matched.size() == 1);
          const NodeDesc host_l = mstd::front(host_matched);
          mstd::append(host_leaves, host_l);
          Host::label(host_l).clear();
          manager.contain.comp_info.replace_visible_leaf(host_l, NoNode);
          manager.contain.HG_label_match.erase(HG_match_iter);
        }
        mstd::append(to_suppress, l);
      }
      for(NodeDesc l: to_suppress) {
        std::cout << "SUPPRESSING "<<l<<" in guest...\n";
        while((l != NoNode) && (Guest::out_degree(l) < 2)) {
          const NodeDesc pl = (Guest::in_degree(l) == 1) ? static_cast<NodeDesc>(Guest::parent(l)) : NoNode;
          manager.contain.guest.suppress_node(l);
          l = pl;
        }
      }
      return host_leaves;
    }

    // remove all paths between top and host_leaves except 'except'
    void clear_host_between(const NodeDesc top, const NodeVec host_leaves, const NodeDesc except) {
      assert(except != NoNode);
      assert(Host::is_leaf(except));
      assert(Host::in_degree(except) == 1);
      assert(Host::in_degree(top) == 1);
      auto& host = manager.contain.host;

      // step 0: install except above top
      const NodeDesc ptop = Host::parent(top);
      host.transfer_child(except, top);
      host.transfer_child(ptop, except);

      const NodeDesc top_root = manager.contain.comp_info.comp_root_of(top);
      //step 1: set visible leaf of top's comp root to except
      std::cout << "setting visible leaf of "<<top_root<<" to "<<except<<"\n";
      manager.contain.comp_info.replace_visible_leaf(top_root, except);      

      for(const NodeDesc u: host_leaves) {
        // step 2: clear everything between top and host_leaves
        const NodeDesc u_root = manager.contain.comp_info.comp_root_of(u);
        if(u_root != top_root) {
          assert(u_root != NoNode);
          manager.remove_edges_to_retis_below(u_root);
        }
        manager.clean_orphan_later(u);
      }
      manager.remove_edges_to_retis_below(top);
    }

    // after having identified a node host_u in the host that will be used to display the subtree below guest_v, remove the subtrees below them
    //NOTE: xy_label_iter points to the label-pair that will be spared (u and v will end up receiving this label)
    void match_nodes(const NodeDesc host_u, const NodeDesc guest_v, const mstd::iterator_of_t<LabelMatching>& xy_label_iter) {
      // step 3a: prune guest and remove nodes with label below v (in guest) from both host and guest - also remove their label entries in HG_label_match
      const auto& vlabel = xy_label_iter->first;
      const auto& [host_matched, guest_matched] = xy_label_iter->second;
      assert(host_matched.size() == 1);
      assert(guest_matched.size() == 1);
      const NodeDesc host_x = mstd::front(host_matched);
      const NodeDesc guest_y = mstd::front(guest_matched);
      auto& host = manager.contain.host;

      std::cout << "\tMATCH: marking "<<guest_v<<" (guest) & "<<host_u<<" (host) with label "<<vlabel<<"\n";
      
      std::cout << "\tMATCH: pruning guest at "<<guest_v<<" (except "<<guest_y<<"):\n"<<manager.contain.guest<<"\n";
      // note: prune_guest automatically removes everything it encounters from HG_label_match
      const NodeVec host_leaves = prune_guest_except(guest_v, guest_y);
      std::cout << "\tMATCH: pruned guest:\n"<<manager.contain.guest<<"\n";

      //NOTE: keep track of nodes in the host who have one of their incoming edges removed as component roots may now see them
      std::cout << "\tMATCH: pruning leaves "<<host_leaves<<" below "<<host_u<<" in host (keeping "<<host_x<<"):\n"<<host<<" with comp-DAG\n";
      manager.contain.comp_info.comp_DAG.print_subtree_with_data();

      std::cout << "component roots:\n";
      for(const NodeDesc u: manager.contain.host.nodes()) std::cout << u << ": "<<manager.contain.comp_info.comp_root_of(u) << "\n";
      
      if(!Host::is_root(host_u)) {
        clear_host_between(host_u, host_leaves, host_x);
        manager.contain.comp_info.react_to_leaf_regraft(host_x);
      } else {
        host.transfer_above_root(host_x, host_u);
        manager.contain.comp_info.react_to_leaf_regraft(host_x);
        host.remove_subtree(host_u);
        // clear queues
        manager.clear_queues();
        // clear label matching except for xy_label_iter
        mstd::clear_except(manager.contain.HG_label_match, xy_label_iter);
      }

      std::cout << "\tMATCH: done pruning host; orphan-queue "<<manager.remove_orphans.node_queue<<"\n";
      manager.remove_orphans.apply();
      std::cout << "\tMATCH: pruned host:\n"<<host<<"\n";

      // if not everything below host_x has been removed, then host cannot display guest!
      if(Host::out_degree(host_x) != 0)
        manager.contain.failed = true;
    }
  };

  // a class for extended cherry reduction
  // NOTE: one application of this runs in O(n) time
  // NOTE the reduction works as follows for a cherry abc in guest:
  //      1. let P be a tree path from some x to a such that x is stable on any leaf z != a
  //      2. let uv be an edge such that u is on P but v is not
  //      3. let none of bc be below v (note that a cannot be below v)
  //      ----> delete uv
  //      Further, if
  //      2. b or c has a unique path onto P, then fix this path (remove anything that strays from it)
  template<class Manager>
  struct ExtendedCherryPicker: public CherryPicker<Manager> {
    using Parent = CherryPicker<Manager>;
    using Parent::Parent;
    using typename Parent::Host;
    using typename Parent::Guest;
    using typename Parent::LabelMatching;
    using typename Parent::LabelType;
    using typename Parent::LabelSet;
    using Parent::manager;
    using Parent::node_queue;
    using Parent::comp_root_of;
    using Parent::visible_leaf_of;
    using Parent::add;
    using Parent::label_matching_sanity_check;
    using Parent::get_labels_of_children;

    enum class P_contain: unsigned char { invalid = 0, not_below_P = 1, below_P = 2, in_P = 3 };
    using P_Relation = NodeMap<P_contain>;
    P_Relation p_rel;
    static constexpr std::array<const char*, 4> rel_to_string = {"invalid", "not below P", "below P", "in P"};

    P_contain rel_lookup(const NodeDesc u) const { return_map_lookup(p_rel, u, P_contain::invalid); }
    bool is_in_P(const NodeDesc u) const { return rel_lookup(u) == P_contain::in_P; }
    bool is_below_P(const NodeDesc u) const { return rel_lookup(u) == P_contain::below_P; }
    bool is_in_or_below_P(const NodeDesc u) const { return rel_lookup(u) >= P_contain::below_P; }
    bool is_not_below_P(const NodeDesc u) const { return rel_lookup(u) == P_contain::not_below_P; }

    void mark_one(const NodeDesc u, const P_contain pc) {
      std::cout << "marking "<<u<<" as "<<rel_to_string[static_cast<int>(pc)]<<"\n";
      mstd::append(p_rel, u, pc);
    }
    void mark_all(const NodeVec& v, const P_contain pc) {
      for(const NodeDesc u: v) mark_one(u, pc);
    }

    // if x is in any of the 3 categories, then mark all nodes in 'nodes' as below_P/not_below_P and return x's relation
    // NOTE: if you set construct_P, then this function will compute P instead
    template<bool construct_P = false>
    P_contain mark_relative_to_P(const NodeDesc x, const NodeVec& nodes) {
      const auto iter = p_rel.find(x);
      if(iter != p_rel.end()) {
        switch(iter->second) {
          case P_contain::in_P:
          case P_contain::below_P:
            mark_all(nodes, construct_P ? P_contain::in_P : P_contain::below_P);
            break;
          case P_contain::not_below_P:
            mark_all(nodes, P_contain::not_below_P);
            break;
          default:
            break;
        }
        return iter->second;
      } else return P_contain::invalid;
    }

    friend std::ostream& operator<<(std::ostream& os, ExtendedCherryPicker& ecp) {
      const auto lam = [&](const auto rel) {
        os << rel_to_string[static_cast<int>(rel)] << ": [";
        for(const auto [x, r]: ecp.p_rel) if(r == rel) os<<x<<' ';
        os << "]\n";
      };
      lam(P_contain::in_P);
      lam(P_contain::below_P);
      lam(P_contain::not_below_P);
      return os;
    }

    struct P_Rel_clearer {
      P_Relation& to_clear;
      ~P_Rel_clearer() { to_clear.clear(); }
    };


    bool apply() {
      std::cout << "\texCHERRY: applying reduction...\n";
      while(!node_queue.empty()){
        std::cout << "value-popping "<<mstd::front(node_queue)<<"\n";
        const NodeDesc u = mstd::value_pop(node_queue);
        std::cout << "ex-cherry on node "<<u<<"\n";
        const auto& label_match = manager.find_label_in_host(u);
        std::cout << "using label-match " << *label_match << "\n";
        if(extended_cherry_reduction_from(label_match)) return true;
      }
      return false;
    }


    // remove all branches between a node 'bottom' and a set 'top' using a predicate NextParent that is true for (u,v) if u is v's next parent
    // NOTE: return the number of branches encountered
    // NOTE: no edges of top are removed and bottom has only its in-edges removed (except to the parent)
    template<class Top, pred::PredicateType<NodeDesc, NodeDesc> NextParent = pred::TruePredicate>
      requires (NodeSetType<Top> || std::is_same_v<std::remove_cvref_t<Top>, NodeDesc>)
    size_t remove_branches_between(Top&& top, NodeDesc bottom, NextParent&& next_parent = NextParent()) {
      std::cout << "\texCHERRY: removing branches between "<<top<<" and "<<bottom<<"\n";
      std::vector<NodePair> removals;
      while(1) {
        NodeDesc parent = NoNode;
        // step 1: mark incoming edges of bottom for removal (except from the next parent)
        for(const NodeDesc z: Host::parents(bottom))
          if(next_parent(z, bottom)) {
            parent = z;
          } else mstd::append(removals, z, bottom);
        // step 2: test the parent
        if((parent != NoNode) && !mstd::test(top, parent)) {
          // step 3: mark outgoing edges of the parent for removal
          for(const NodeDesc z: Host::children(parent)) if(bottom != z) mstd::append(removals, parent, z);
          bottom = parent;
        } else break;
      }
      std::cout << "removing edges in "<<removals<<"\n";
      for(const auto& [u, v]: removals)
        manager.remove_edge_in_host(u, v);
      return removals.size();
    }

    // find a visible leaf different from 'vis_except' directly below the reticulation x
    NodeDesc visible_except_reti(const NodeDesc x, const NodeDesc vis_except, NodeMap<Degree>& seen_retis) {
      assert(Host::out_degree(x) == 1);
      assert(Host::in_degree(x) >= 2);
      if(++seen_retis[x] == Host::in_degree(x))
        if(const NodeDesc xvl = visible_leaf_of(Host::child(x)); xvl != vis_except)
          return xvl;
      return NoNode;
    }
    // find a visible leaf different from 'vis_except' directly below the tree component of px
    NodeDesc visible_except(const NodeDesc px, const NodeDesc vis_except, const NodeDesc child_except, NodeMap<Degree>& seen_retis) {
      assert(Host::in_degree(px) <= 1);
      const NodeDesc vl = visible_leaf_of(px);
      std::cout << px << " visible on "<<vl<<" (except = "<<vis_except<<")\n";
      if((vl == NoNode) || (vl == vis_except)) {
        for(const NodeDesc x: Host::children(px))
          if(x != child_except) {
            const NodeDesc xvl = (Host::in_degree(x) >= 2)
              ? visible_except_reti(x, vis_except, seen_retis)
              : visible_except(x, vis_except, NoNode, seen_retis);
            if(xvl != NoNode) return xvl;
          }
      } else return vl;
      return NoNode;
    }
    NodeDesc visible_except(const NodeDesc px, const NodeDesc vis_except, const NodeDesc child_except) {
      NodeMap<Degree> degree_map;
      return visible_except(px, vis_except, child_except, degree_map);
    }

    // categorize all nodes above x that are not stable on anyone else than vl_except into 2 categories:
    // (1) those that are below a node in P
    // (2) those that are not
    template<bool construct_P = false>
    bool unstable_above(NodeDesc x, const NodeDesc vl_except, NodeDesc child_except = NoNode, const NodeSet* cherry_leaves = nullptr) {
      // we'll use a reverse DFS from x
      // step 1: find the first reti above x
      NodeVec current_nodes;
      while(1) {
        // if x is in_P or below_P, then mark all current_nodes as below_P
        // if x is not_below_P, then mark all current nodes as not_below_P
        const auto x_rel = mark_relative_to_P<construct_P>(x, current_nodes);
        std::cout << "marking unstable from "<<x<<" (coming from "<<child_except<<") - x_rel = "<< rel_to_string[static_cast<int>(x_rel)] <<"\tcurrent_nodes = "<<current_nodes<<"\n";
        // if x none of the three, then recurse 
        if(x_rel == P_contain::invalid) {
          if(Host::in_degree(x) == 1) {
            assert(comp_root_of(x) != NoNode);
            // step 2: if x is stable on anyone else than u, then we consider the current path 'not_below_P' since the path cannot be used
            const NodeDesc vl = visible_except(x, vl_except, child_except);
            std::cout << "found that "<<x<<" is stable on "<<vl<<" != "<<vl_except<<"\n";
            if(vl != NoNode) {
              mark_all(current_nodes, construct_P ? P_contain::in_P : P_contain::not_below_P);
              if constexpr (construct_P) {
                assert(cherry_leaves);
                if(mstd::test(*cherry_leaves, vl))
                  mark_one(x, P_contain::in_P);
              }
              return false;
            } else {
              current_nodes.push_back(x);
              x = Host::parent(child_except = x);
            }
          } else {
            bool result = construct_P;
            for(const NodeDesc u: Host::parents(x))
              result |= unstable_above<construct_P>(u, vl_except, x, cherry_leaves);
            const P_contain marking = result ? (construct_P ? P_contain::in_P : P_contain::below_P) : P_contain::not_below_P;
            mark_all(current_nodes, marking);
            mark_one(x, marking);
            return result;
          }
        } else return x_rel != P_contain::not_below_P;
      }
    }

    // move from x to the parent of x, marking incoming edges for removal if they are not below P
    // NOTE: if there is more than one incoming edge in_P or below_P, then return NoNode so we can abort the process
    NodeDesc advance_to_parent(const NodeDesc x, auto& to_remove) {
      bool no_parents_below_P = true;
      NodeDesc result = NoNode;
      for(const NodeDesc px: Host::parents(x)) {
        switch(rel_lookup(px)) {
          case P_contain::in_P:
            no_parents_below_P = false;
            result = NoNode;
            break;
          case P_contain::below_P:
            if(no_parents_below_P) {
              result = px;
              no_parents_below_P = false;
            } else result = NoNode;
            break;
          default:
            mstd::append(to_remove, px, x);
        }
      }
      // if we found no parent that is in or below P, then host cannot display guest
      if(no_parents_below_P) {
        manager.contain.failed = true;
        // in order to fail early, we'll clear to_remove as to avoid treating the edges therein unnecessarily
        to_remove.clear();
      }
      return result;
    }

    bool climb_and_remove_edges(const NodeDesc x) {
      NodeDesc y = x;
      NodeDesc child_except = NoNode;
      NodePairSet to_remove;
      while(y != NoNode) {
        // step 1: remove branches
        if(Host::out_degree(y) > 1) {
          for(const NodeDesc cy: Host::children(y))
            if(cy != child_except)
              mstd::append(to_remove, y, cy);
        }
        // step 2: climb until we see anyone in_P or below_P
        std::cout << "\texCHERRY: finding rev paths from "<<y<<" into P\n";
        child_except = y;
        y = advance_to_parent(y, to_remove);
      }
      std::cout << "want to remove "<<to_remove<<"\n";
      assert(to_remove.size() <= (Host::out_degree(x) - 1 + std::max<size_t>(Host::in_degree(x) - 1, 0)));
      // step 3: go up from u until we hit a node z with a path to x and remove all branches from nodes before z
      if(!to_remove.empty()) {
        // step 4: remove all marked edges
        for(const auto [s, t]: to_remove)
          manager.remove_edge_in_host(s, t);
        return true;
      } else return manager.contain.failed;
    }

    bool extended_cherry_reduction_from(const mstd::iterator_of_t<LabelMatching>& uv_label_iter) {
      std::cout << "\texCHERRY: checking label matching "<<*uv_label_iter<<"\n";
      const auto& UV = uv_label_iter->second;
      const auto& [U, V] = UV;
      assert(U.size() == 1);
      assert(V.size() == 1);
      const NodeDesc u = mstd::front(U);
      const NodeDesc v = mstd::front(V);
      const auto [pu, pv, success] = label_matching_sanity_check(u, v);
     
      LabelSet cherry_labels = get_labels_of_children(pv);
      if(!cherry_labels.empty()) {
        P_Rel_clearer tmp{p_rel};
        std::cout << "\texCHERRY: found sibling-labels "<<cherry_labels<<"\n";
        // step 0: translate labels into nodes in the host
        NodeSet cherry_leaves;
        cherry_leaves.reserve(cherry_labels.size() - 1);
        for(const auto& label: cherry_labels) {
          const auto iter = manager.find_label(label);
          assert(iter != manager.contain.HG_label_match.end());
          const auto& host_leaves = iter->second.first;
          assert(host_leaves.size() == 1);
          const NodeDesc x = mstd::front(host_leaves);
          assert(Host::label(x) == label);
          if(x != u) mstd::append(cherry_leaves, x);
        }
        // step 1:  move up from u to emplace the set P of unstable nodes above u
        unstable_above<true>(u, u, NoNode, &cherry_leaves);
        // step 2: move up from the nodes in host that are label-matched to siblings x of u
        bool result = false;
        for(const NodeDesc x: cherry_leaves){
          std::cout << "next leaf: "<<x<<" ["<<Host::label(x)<<"]\n";
          std::cout << *this;
          unstable_above(x, x);
          std::cout << "after marking:\n";
          std::cout << *this;
          std::cout << "now climbing from "<<x<<"\n";
          result |= climb_and_remove_edges(x);
          std::cout << "result is now "<<result<<"\n";
        } // for all 'siblings' x of u
        return result;
      } else return false; // if get_labels_of_children returned the empty set, this indicates that pv has a child that is not a leaf, so give up
    } 
  };


  // a class managing the application of all reduction rules in the correct order
  template<class _Containment>
  struct ReductionManager {
    using Containment = _Containment;
    using ComponentInfos = std::remove_reference_t<typename Containment::ComponentInfos>;
    using ComponentDAG = typename ComponentInfos::ComponentDAG;
    using Host = typename Containment::Host;
    using Guest = typename Containment::Guest;

    Containment& contain;
    NodeSet orphan_queueu;

    OrphanRemover<ReductionManager> remove_orphans;
    ReticulationMerger<ReductionManager> reti_merge;
    TriangleReducer<ReductionManager> triangle_rule;
    CherryPicker<ReductionManager> cherry_rule;
    VisibleComponentRule<ReductionManager> visible_comp_rule;
    HostGuestMatch<ReductionManager> HG_match_rule;
    ExtendedCherryPicker<ReductionManager> extended_cherries;

    ReductionManager(Containment& c):
      contain(c),
      remove_orphans(*this),
      reti_merge(*this),
      triangle_rule(*this),
      cherry_rule(*this),
      visible_comp_rule(*this),
      HG_match_rule(*this),
      extended_cherries(*this)
    {}


    void clean_orphan_later(const NodeDesc u) {
      std::cout << "requested to clean orphan "<<u<<" later\n";
      if(contain.host.label(u).empty())
        remove_orphans.add(u);
    }

    void remove_from_comp_DAG(const NodeDesc u) {
      auto& info = contain.comp_info;
      const auto iter = info.N_to_comp_DAG.find(u);
      if(iter != info.N_to_comp_DAG.end()) {
        const NodeDesc u_in_cDAG = iter->second;
        if(info.comp_DAG.out_degree(u_in_cDAG) == 1)
          info.comp_DAG.contract_down_unique(u_in_cDAG);
        else info.comp_DAG.remove_node(u_in_cDAG);
        info.N_to_comp_DAG.erase(iter);
      }
    }

    void contract_reti_onto_reti_child(const NodeDesc u) {
      assert(Host::out_degree(u) == 1);
      const auto u_child = Host::child(u);
      assert(Host::is_reti(u_child));
      remove_from_queues(u);
      if(contain.host.contract_down_unique(u, u_child) != 0)
        for(const NodeDesc pu: Host::parents(u_child))
          if((Host::in_degree(pu) <= 1) && (Host::out_degree(pu) <= 1))
            clean_orphan_later(pu);
      if(Host::in_degree(u_child) > 1)
        triangle_rule.add(u_child);
    }

    void remove_edge_in_host(const NodeDesc u, const NodeDesc v) {
      assert(Host::in_degree(u) < 2);

      if(Host::in_degree(v) > 1) {
        assert(Host::out_degree(v) <= 1);
        auto& host = contain.host;
        std::cout << "reduction manager removing edge "<< u << " -> "<< v<<'\n';
        host.remove_edge(u, v);
        contain.comp_info.react_to_edge_deletion(u, v);

        // if v is now suppressible, then contract v onto its child
        if(Host::in_degree(v) == 1) {
          mstd::erase(triangle_rule.node_queue, u);
          if(Host::label(v).empty()) 
            clean_orphan_later(v);
        }
        // if u is now suppressible, then we either contract u down or its child up
        if((Host::out_degree(u) <= 1) && Host::label(u).empty())
          clean_orphan_later(u);
      } else remove_edges_to_retis_below(v);
    }

    // remove edges that run from the 'tree-node-body' below u to any reticulation
    // NOTE: if any reticulation r below u is visible by u, then this recurses on r's child
    void remove_edges_to_retis_below(const NodeDesc u) {
      assert(!contain.host.is_reti(u));
      auto& host = contain.host;
      const auto& u_children = host.children(u);
      NodeVec to_delete;
      to_delete.reserve(u_children.size());
      for(const NodeDesc v: u_children)
        if(!host.is_reti(v))
          remove_edges_to_retis_below(v);
      for(const NodeDesc v: u_children)
        if(host.is_reti(v))
          mstd::append(to_delete, v);
      std::cout << "removing edges from "<<u<<" to nodes in "<<to_delete<<"\n";
      for(const NodeDesc v: to_delete)
        remove_edge_in_host(u, v);
    }

/*   
    NodeDesc& comp_root_of(const NodeDesc u) const { return contain.comp_root_of(u); }
    NodeDesc& visible_leaves_of(const NodeDesc u) const { return contain.visible_leaves_of(u); }
    // clean up dangling leaves/reticulations and suppressible nodes in the host
    //NOTE: this will also update comp_info and comp_info.comp_DAG
    void clean_up_node(const NodeDesc y, const bool recursive = true, const bool apply_reti_reduction = true) {
      std::cout << "\tCLEAN: called for "<< y << " (reduction flag: "<<apply_reti_reduction<<") in\n"<<contain.host<<"\n";
      switch(contain.host.out_degree(y)){
        case 0: // host.out_degree(y) == 0
          {
            std::cout << "\tCLEAN: "<< y << " is a dangling leaf, so we'll remove it from host\n";
            // remove y and treat y's former parents
            for(const NodeDesc z: contain.host.parents(y)) clean_up_later(z);
            contain.host.remove_node(y);
            // if y was a component root, then suppress that root in the component DAG
            const auto iter = contain.comp_info.N_to_comp_DAG.find(y);
            if(iter != contain.comp_info.N_to_comp_DAG.end())
              contain.comp_info.comp_DAG.suppress_node(iter->second);
          }
          break;
        case 1: // host.out_degree(y) == 1
          if(contain.host.in_degree(y) < 2){
            const NodeDesc x = contain.host.parent(y);
            const auto yz = contain.host.any_out_edge(y);
            const NodeDesc z = yz.head();
            std::cout << "\tCLEAN: "<< y <<" ("<<contain.comp_info.component_data_of(y)<<") is suppressible with child: "<<z \
                      <<" ("<<contain.comp_info.component_data_of(z)<<"))\n";
            // if y is the root of a tree-component, then contract the other outgoing arc yz from y,
            // unless z is a reti, in which case we kill the tree comp
            std::cout << "\tCLEAN: comp-info of "<<y<<": "<<contain.comp_info.component_data_of(y)<<"\n";

            // if there is a leaf below y, then contract that onto y
            if(contain.host.out_degree(z) == 0){
              const NodeDesc y_in_cDAG = contain.host_to_comp_DAG(y);
              std::cout << "\tCLEAN: "<< y <<"'s only remaining child "<<z<<" is a leaf\n";
              remove_from_queues(y);
              contain.host.contract_down(yz);
              if(y_in_cDAG != NoNode) contain.comp_info.comp_DAG.remove_node(y_in_cDAG);
              contain.comp_info.inherit_root(z);
              cherry_rule.add(z);
              if(recursive) clean_up_node(z, true, apply_reti_reduction); else clean_up_later(z);
            } else {
              if(comp_root_of(z) != z){ // if z is not a component root, then contract z onto y
                std::cout << "\tCLEAN: "<< y <<"'s only remaining child "<<z<<" is not a component-root\n";
                remove_from_queues(z);
                contain.host.contract_up(z, y);
                contain.comp_info.inherit_root(y);
                if(recursive) clean_up_node(y, true, apply_reti_reduction); else clean_up_later(z);
              } else { // if z is a component root, then contract y onto z
                const NodeDesc y_in_cDAG = contain.host_to_comp_DAG(y);
                std::cout << "\tCLEAN: "<< y <<"'s only remaining child "<<z<<" is a component-root\n";
                remove_from_queues(y);
                contain.host.contract_down(yz);
                contain.comp_info.inherit_root(z);
                if(y_in_cDAG != NoNode) contain.comp_info.comp_DAG.suppress_node(y); // y may have been a comp-root
                if(recursive) clean_up_node(z, true, apply_reti_reduction); else clean_up_later(z);
              }
              // if the parent of y still exists in host, then also recurse for him
              if(recursive) clean_up_node(x, true, apply_reti_reduction); else clean_up_later(x);
            }
          } else {
            std::cout << "\tCLEAN: "<< y <<" is a reticulation\n";
            if(!apply_reti_reduction || !triangle_rule.triangle_rule(y)){
              const NodeDesc z = contain.host.any_child(y);
              switch(contain.host.out_degree(z)){
                case 0:
                  contain.comp_info.inherit_root(z);
                  break;
                case 1:
                  if(apply_reti_reduction) reti_merge.contract_reti(y);
                  break;
                default: break;
              } // end switch out-degree(z)
            } else if(recursive) clean_up_node(y, true, apply_reti_reduction);
          }// end if(in-degree(y)...)
          break;
        default: // y is a non-leaf tree node
          break;
      }
      
      std::cout << "\tCLEAN: done.\n";
    }

    void clean_up_now(const bool recursive = true, const bool apply_reti_reduction = true) {
      while(!to_clean.empty()){
        std::cout << "remaining queue to clean: "<<to_clean<<'\n';
        clean_up_node(mstd::value_pop(to_clean), recursive, apply_reti_reduction);
      }
    }
    void clean_up_later(const NodeDesc u) {
      mstd::append(to_clean, u);
      std::cout << "cleaning "<<u<<" later, queue is now "<<to_clean<<"\n";
    }
*/
    auto find_label(auto& label) const {
      return contain.HG_label_match.find(label);
    }

    auto find_label_in_guest(const NodeDesc u) const {
      return contain.HG_label_match.find(contain.guest.label(u));
    }
    auto find_label_in_host(const NodeDesc u) const {
      return contain.HG_label_match.find(contain.host.label(u));
    }
    void remove_from_queues(const NodeDesc u) {
      mstd::erase(reti_merge.node_queue, u);
      mstd::erase(triangle_rule.node_queue, u);
      mstd::erase(remove_orphans.node_queue, u);
      mstd::erase(cherry_rule.node_queue, u);
      mstd::erase(extended_cherries.node_queue, u);
    }
    void clear_queues() {
      reti_merge.node_queue.clear();
      triangle_rule.node_queue.clear();
      remove_orphans.node_queue.clear();
      cherry_rule.node_queue.clear();
      extended_cherries.node_queue.clear();
    }

    void apply() {
      std::cout << "\n ===== REDUCTION RULES ======\n\n";
      std::cout << "reti-m queue: "<<reti_merge.node_queue << "\n";
      std::cout << "host:\n" << contain.host << "\nguest:\n" << contain.guest << "\ncomp-DAG:\n";
      contain.comp_info.comp_DAG.print_subtree(std::cout, [](const NodeDesc u){ return std::to_string(u); });
        

      remove_orphans.init_queue();
      remove_orphans.apply();

      reti_merge.init_queue();
      reti_merge.apply();

      mstd::append(triangle_rule.node_queue, contain.host.retis());
      // if, at some point, there are only 2 leaves left, then simply say 'yes'
      while(!contain.failed && (contain.HG_label_match.size() > 2)) {
        std::cout << "\n === restart rule-application ===\n";
        std::cout << "orphan queue: "<<remove_orphans.node_queue << "\n";
        std::cout << "triangle queue: "<<triangle_rule.node_queue << "\n";
        std::cout << "cherry queue: "<<cherry_rule.node_queue << "\n";

        std::cout << "host:\n" << contain.host << "\nguest:\n" << contain.guest << "\ncomp-DAG:\n";
        contain.comp_info.comp_DAG.print_subtree(std::cout, [](const NodeDesc u){ return std::to_string(u); });
        std::cout << "label matching: "<<contain.HG_label_match<<"\n";
        std::cout << "visible leaves: "; for(const NodeDesc u:contain.host.nodes()) {
          std::cout << '(' << u << ": "<<contain.comp_info.visible_leaf_of(u)<<") "; } std::cout << '\n';
        std::cout << "comp-roots:\n"; for(const NodeDesc u: contain.host.nodes()) std::cout << u <<": " << contain.comp_info.comp_root_of(u) <<'\n';

        if(remove_orphans.apply()) continue;
        if(triangle_rule.apply()) continue;
      
        cherry_rule.init_queue();
        if(cherry_rule.apply()) continue;
        
        if(visible_comp_rule.apply()) continue;

        reti_merge.init_queue();
        if(reti_merge.apply()) continue;
#warning "TObenchmark: on smaller networks, the extended cherry might actually be faster than visible-component rule, so maybe switch those in that case..."
        if(contain.host.num_edges() - contain.host.num_nodes() >= config::min_retis_to_apply_extended_cherry) {
          extended_cherries.init_queue();
          if(extended_cherries.apply()) continue;
        }
        break;
      }
      std::cout << "\n ===== REDUCTION done ======\n\n";
      std::cout << "host:\n" << contain.host << "guest:\n" << contain.guest << "comp-DAG:\n";
      contain.comp_info.comp_DAG.print_subtree_with_data();
    }
  };


}
