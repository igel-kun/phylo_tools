
#pragma once

#include "types.hpp"
#include "tree_tree_containment.hpp"
#include "tree_comp_containment.hpp"

namespace PT {
 
  template<class T>
  concept Reduction = requires(T t) { t.apply(); };

  template<class Manager>
  struct ContainmentReduction {
    using Host = typename Manager::Containment::DecayedHost;
    using Guest = typename Manager::Containment::DecayedGuest;
    using LabelMatching = typename Manager::Containment::LabelMatching;
    using LabelType = LabelTypeOf<Host>;
    
    Manager& manager;

    ContainmentReduction(Manager& c): manager(c) {}

    NodeDesc comp_root_of(const NodeDesc u) const { return manager.contain.comp_info.comp_root_of(u); }
  };

  template<class Manager>
  struct ContainmentReductionWithNodeQueue: public ContainmentReduction<Manager> {
    using Parent = ContainmentReduction<Manager>;
    using Parent::Parent;

    NodeSet node_queue;
    void add(const NodeDesc x) {
      append(node_queue, x);
      std::cout << "adding node "<<x<<" to queue, now: "<<node_queue<<"\n";
    }
    
  };


  // a class to remove orphaned leaves
  template<class Manager>
  struct OrphanRemover: public ContainmentReductionWithNodeQueue<Manager> {
    using Parent = ContainmentReductionWithNodeQueue<Manager>;
    using typename Parent::Host;
    using Parent::manager;
    using Parent::node_queue;
  
    // remove leaves without label from the host
    bool apply() {
      auto& host = manager.contain.host;
      std::cout << "removing orphans from "<<node_queue<<'\n';
      bool result = false;
      while(!node_queue.empty()){
        const NodeDesc v = std::value_pop(node_queue);
        // if u is unlabeled, queue its parents and remove u itself
        if(host.label(v).empty() && (host.out_degree(v) <= 1)) {
          if(host.out_degree(v) == 0) {
            for(const NodeDesc u: host.parents(v))
              if(host.out_degree(u) <= 2)
                append(node_queue, u);
            std::cout << "removing node "<<v<<"\n";
            manager.remove_from_queues(v);
            host.remove_node(v);
          } else manager.suppress_node_in_host(v);
          result = true;
        }
      }
      return result;
    }

    // prepare first round of reticulations to merge
    void init_queue() {}
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
        const NodeDesc x = value_pop(node_queue);
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
  struct TriangleReducer: public ContainmentReduction<Manager> {
    using Parent = ContainmentReduction<Manager>;
    using typename Parent::Host;
    using Parent::manager;

    bool apply() { return triangle_rule(); }

    // triangle rule:
    // if z has 2 parents x and y s.t. xy is an arc and x has indeg-1 and outdeg-2 & y has outdeg at most 2, then remove the arc xz
    bool triangle_rule(const NodeDesc z, const NodeDesc y) {
      auto& host = manager.contain.host;
      assert(host.out_degree(z) == 1);
      std::cout << "applying triangle-rule to "<<z<< " & "<<y<<"\n";
      if(host.out_degree(y) <= 2){
        for(const NodeDesc x: host.parents(y)){
          std::cout << "checking parent "<<x<<" of "<<y<<" for triangle with "<<z<<"\n";
          if((host.out_degree(x) == 2) && host.is_edge(x, z)) {
            std::cout << "removing edge "<<x<<" -> "<<z<<" due to triangle rule\n\n";
            manager.remove_edge_in_host(x, z);
            return true;
          }
        }
      } 
      return false;
    }

    bool triangle_rule(const NodeDesc z) {
      bool result = false;
      if(manager.contain.host.out_degree(z) == 1){
        bool local_result;
        do {
          local_result = false;
          for(const NodeDesc y: manager.contain.host.parents(z))
            if(triangle_rule(z, y)) { // if the triangle rule applied, then we messed with z's parents so we have to exit the for-each here
              result = local_result = true;
              break;
            }
        } while(local_result);
      } 
      return result;
    }

#warning "TODO: turn this into a rule with queue that is fed by the reti-merger or something (remember: non-retis stay non-retis!)"

    bool triangle_rule() {
      bool result = false;
      NodeVec retis = manager.contain.host.retis().to_container();
      while(!retis.empty())
        result |= triangle_rule(value_pop(retis));
      return result;
    }

  };

  // a class for containment cherry reduction
  template<class Manager>
  struct CherryPicker: public ContainmentReductionWithNodeQueue<Manager> {
    using Parent = ContainmentReductionWithNodeQueue<Manager>;
    using typename Parent::Host;
    using typename Parent::Guest;
    using typename Parent::LabelMatching;
    using typename Parent::LabelType;
    using Parent::manager;
    using Parent::node_queue;
    using Parent::comp_root_of;
    using Parent::add;

    bool apply() { return simple_cherry_reduction(); }

    void init_queue() {
      // put all leaves of the host in the queue
      for(const auto uv: manager.contain.HG_label_match)
        add(uv.second.first);
    }

    bool simple_cherry_reduction() {
      std::cout << "\tCHERRY: applying reduction...\n";
      bool result = false;
      while(!node_queue.empty()){
        std::cout << "value-popping "<<front(node_queue)<<"\n";
        const NodeDesc u = value_pop(node_queue);
        std::cout << "cherry on node "<<u<<"\n";
        const auto& label_match = manager.find_label_in_host(u);
        std::cout << "using label-match " << *label_match << "\n";
        if(simple_cherry_reduction_from(label_match))
          result = true;
      }
      std::cout << "done with cherry reduction, result is "<<result<<"\n";
      return result;
    }

    bool simple_cherry_reduction_from(const std::iterator_of_t<LabelMatching>& uv_label_iter) {
      std::cout << "\tCHERRY: checking label matching "<<*uv_label_iter<<"\n";
      const auto& uv = uv_label_iter->second;
      const NodeDesc u = uv.first;
      const auto& u_node = node_of<Host>(u);
      if(u_node.in_degree() != 1) return false;
      
      const NodeDesc v = uv.second;
      const auto& v_node = node_of<Guest>(v);
      if(v_node.in_degree() != 1) return false;
      
      const NodeDesc pu = u_node.any_parent();
      const auto& pu_node = node_of<Host>(pu); 
      if(pu_node.in_degree() > 1) return false;

      const NodeDesc pv = v_node.any_parent();
      const auto& pv_node = node_of<Guest>(pv);
      

      // degrees match, so see if the labels of the children match
      // step 1: compute a set of leaves directly below the parent of v (in guest)
      HashSet<AsMapKey<LabelType>> seen;
      for(const NodeDesc x: pv_node.children()) {
        const auto& x_node = node_of<Guest>(x);
        if(x_node.out_degree() != 0) return false;
        append(seen, x_node.label());
      }
      //NOTE: detect reticulated cherries by passing through reticulations
      const auto& puC = pu_node.children();
      std::cout << "\tCHERRY: considering children "<<puC<<" of "<<pu<<" in the host (leaves in guest: "<<seen<<")\n";
      if(puC.size() >= seen.size()){
        const auto& host = manager.contain.host;
        std::vector<NodePair> edge_removals;
        for(auto iter = puC.begin(); iter != puC.end();){
          NodeDesc x = *iter; ++iter;
          NodeDesc z = pu;
          // get the first reti on the pu-->x path into y and it's parent into z
          while((host.out_degree(x) == 1) && (host.in_degree(x) == 1)) x = host.any_child(z = x);
          NodeDesc y = x;
          // skip all remaining outdeg-1 nodes
          while(host.out_degree(x) == 1) x = host.any_child(x);
          // if we've arrived at a label below pu that is not below pv, then pu-->x will never be in an embedding of guest in host!
          const auto seen_iter = seen.find(host.label(x));
          if(seen_iter == seen.end()) {
            append(edge_removals, z, y);
          } else seen.erase(seen_iter);
        }
        bool result = !edge_removals.empty();
        for(const auto& [z,y]: edge_removals)
          if(host.in_degree(y) > 1)
            manager.remove_edge_in_host(z, y);
        if(!edge_removals.empty()) std::cout << "new host is:\n" << host <<'\n';
        // if we found all labels occuring below pv also below pu, then we'll apply the cherry reduction
        if(seen.empty()) {
          std::cout << "\tCHERRY: found (reticulated) cherry at "<<pu<<" (host) and "<<pv<<" (guest)\n";
          // step 1: fix visibility labeling to u, because the leaf that pu's root is visible from may not survive the cherry reduction (but u will)
          manager.contain.comp_info.set_visible_leaf(comp_root_of(pu), u);
          // step 2: prune host and guest
          manager.HG_match_rule.match_nodes(pu, pv, uv_label_iter);
          std::cout << "\tCHERRY: after reduction:\nhost:\n"<<host<<"guest:\n"<<manager.contain.guest<<"\n";
          std::cout << "comp-roots:\n"; for(const NodeDesc z: manager.contain.host.nodes()) std::cout << z <<": " << manager.contain.comp_info.comp_root_of(z) <<'\n';
        } else return result;
      } else manager.HG_match_rule.match_nodes(pu, v, uv_label_iter); // if puC is smaller than seen, then pu cannot display pv, so pu has to display v
      return true;
    }

    bool simple_cherry_reduction_for_label(const LabelType& label) {
      return label.empty() ? false : simple_cherry_reduction_from(manager.find_label(label));
    }

#warning "TODO: apply general cherry reduction before branching: 1. uv is cherry in guest and 2. lca(uv) is unique in host OR, slightly cheaper: 2. exists lowest ancestor x of u & v s.t. x visible on v (tree-path f.ex.) and the x-->u path is unqiue"

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

    using MulSubtree = DefaultLabeledTree<>;

    void treat_comp_root(const NodeDesc u, const NodeDesc vis_leaf) {
      TreeInComponent<MulSubtree, Guest, leaf_labels_only> tree_comp_display(manager.contain.host, u, manager.contain.guest, manager.contain.HG_label_match);
      
      assert(vis_leaf != NoNode);
      const auto& vlabel = manager.contain.host.label(vis_leaf);

      std::cout << "using visible leaf "<<vis_leaf<<" with label "<<vlabel<<"\n";
      std::cout << "label matching: "<<manager.contain.HG_label_match<<"\n";

      const auto uv_label_iter = manager.find_label(vlabel);
      assert(uv_label_iter != manager.contain.HG_label_match.end());

      // step 2: get the highest ancestor v of vis_leaf in T s.t. T_v is still displayed by N_u
      const NodeDesc visible_leaf_in_guest = uv_label_iter->second.second;
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

    // return whether u is half-eligible and, update u's paths
    //NOTE: the update to num_paths[u] are incorrect if u is not half-elighible!
    bool is_half_eligible(const NodeDesc u, PathProfile& num_paths, const NodeSet& half_eligible) const {
      auto& u_paths = num_paths[u];
      std::cout << "checking node "<<u<<" ("<< comp_root_of(u) <<") with current paths "<<u_paths<<"\n";
      const auto& host = manager.contain.host;
      const auto& cDAG_node_u = node_of<ComponentDAG>(manager.contain.comp_info.N_to_comp_DAG.at(u));
      for(const NodeDesc cDAG_v: cDAG_node_u.children()) if(const NodeDesc v = node_of<ComponentDAG>(cDAG_v).data(); test(half_eligible, v)) {
        std::cout << "next child: "<<v<<"\n";
        // to test if the u-v-path in host is unique, we'll look at all reticulations above v in host and check if we see u more than once
        assert(host.in_degree(v) == 1);
        NodeDesc r = host.parent(v);
        if(r != u) { // if v is also the child of u in host (!) then the u-v-path is unqiue
          NodeVec retis_above{r};
          while(!retis_above.empty()){
            r = value_pop(retis_above);
            std::cout << "exploring reti "<<r<<" ("<<manager.contain.comp_info.comp_root_of(r)<<") above "<<v<<"\n";
            if(host.in_degree(r) <= 1) {
              assert(comp_root_of(r) != NoNode);
              num_paths[comp_root_of(r)][v]++;
              std::cout << "registered "<< comp_root_of(r) <<"--"<<v<<" path. Now, "<<u<<" has "<<num_paths[comp_root_of(r)][v]<<" of them\n";
            } else append(retis_above, host.parents(r));
          }
          if(u_paths[v] > 1) return false; // if we ended up counting more than one path, return non-eligibility
        } else u_paths[v] = 1;
        // for each root that v has paths to, u also has paths to via v
        for(const auto& node_and_paths: num_paths[v])
          if((u_paths[node_and_paths.first] += node_and_paths.second) > 1)
            return false;
        std::cout << "added paths to "<<u<<"'s paths, now: "<<u_paths<<"\n";
      } else return false;
      std::cout << u<< " eligible with paths: "<<u_paths<<'\n';
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
            if(manager.contain.comp_info.visible_leaf_of(u) != NoNode) return u;
            half_eligible.emplace(u);
          } else std::cout << u<< " not eligible\n";
        }
        // if we never found an eligible node, then return failure
        return NoNode;
      } else return manager.contain.host.root();
    }

    // visible tree-component reduction: find a lowest visible tree component C and reduce it in O(|C|) time; return true if network & tree changed
    bool apply() {
      const NodeDesc rt = get_eligible_component_root();
      if(rt != NoNode){
        std::cout << "tree-component rule with eligible node "<< rt <<" ("<< manager.contain.comp_info.comp_root_of(rt)<<")\n";
        std::cout << "on tree-component DAG:\n";
        manager.contain.comp_info.comp_DAG.print_subtree_with_data();
        const NodeDesc vis_leaf = manager.contain.comp_info.visible_leaf_of(rt);

        std::cout << "host:\n"<<manager.contain.host<<"\n";
        std::cout << "guest:\n"<<manager.contain.guest<<"\n";
        std::cout << "visible on leaf "<<vis_leaf<<"\n";
        assert(vis_leaf != NoNode);
        treat_comp_root(rt, vis_leaf);
        return true;
      } else return false;
    } 

  };

  // whenever a node in the host is identified that optimally displays a node in the guest, this class updates host and guest accordingly
  template<class Manager>
  struct HostGuestMatch {
    using Host = typename Manager::Containment::DecayedHost;
    using Guest = typename Manager::Containment::DecayedGuest;
    using LabelMatching = typename Manager::Containment::LabelMatching;

    Manager& manager;
   
    HostGuestMatch(Manager& c): manager(c) {}

    // prune the guest below Node x, 
    // ATTENTION: also remove leaves of the host that have a label below x in the guest!!!
    // ATTENTION: also remove the corresponding entries in HG_label_match
    void prune_guest_except(const NodeDesc x, const NodeDesc except) {
      std::cout << "pruning "<<x<<" in guest (except "<<except<<")\n";
      const auto& gC = manager.contain.guest.children(x);
      if(x != except){
        for(auto child_iter = gC.begin(); child_iter != gC.end();) {
          if(*child_iter != except) {
            prune_guest_except(*child_iter, except);
            child_iter = gC.begin();
          } else ++child_iter;
        }
        // if x has a label that matches in the host, then remove the corresponding node from the host and remove the matching entry as well
        const auto HG_match_iter = manager.find_label_in_guest(x);
        if(HG_match_iter != manager.contain.HG_label_match.end()) {
          const NodeDesc host_x = HG_match_iter->second.first;
          manager.contain.HG_label_match.erase(HG_match_iter);
          std::cout << "removing "<<x<<" (guest) and its correspondant "<<host_x<<" (host)...\n";
          // we remove host_x by clearing its label, declaring it orphaned and, thus, removing it
          // NOTE: this is OK since host_x is a leaf and, thus, is not contained in the comp_DAG
          manager.contain.host.label(host_x).clear();
          manager.clean_orphan_later(host_x);
        }
        std::cout << "SUPPRESSING "<<x<<" in guest...\n";
        manager.contain.guest.suppress_node(x);
      }
    }

    // remove edges that run from the 'tree-node-body' below u to any reticulation
    void remove_edges_to_retis_below(const NodeDesc u) {
      auto& host = manager.contain.host;
      const auto& u_children = host.children(u);
      NodeVec to_delete;
      to_delete.reserve(u_children.size());
      for(const NodeDesc v: u_children)
        if(!host.is_reti(v))
          remove_edges_to_retis_below(v);
      for(const NodeDesc v: u_children)
        if(host.is_reti(v))
          append(to_delete, v);
      std::cout << "removing edges from "<<u<<" to nodes in "<<to_delete<<"\n";
      for(const NodeDesc v: to_delete)
        manager.remove_edge_in_host(u, v);
    }

    // this prunes the tree-remains left after recursively removing dangling leaves after pruning guest
    void prune_host_except(const NodeDesc rt, const NodeDesc except) {
      assert(rt != except);
      auto& host = manager.contain.host;
      // NOTE: all leaves below rt except 'except' have been declared orphans already.
      // step 1: remove all edges between tree nodes below rt and reticulations 
      remove_edges_to_retis_below(rt);
      // step 2: rehang except onto rt and mark its parents for cleanup
      for(const NodeDesc p: host.parents(except))
        manager.clean_orphan_later(p);

      std::cout << "rehanging "<<except<<" (parents "<<host.parents(except)<<") below "<<rt<<"\n"; 
      host.replace_parents(except, rt);
      manager.clean_orphan_later(rt);
    }

    // after having identified a node host_u in the host that will be used to display the subtree below guest_v, remove the subtrees below them
    //NOTE: xy_label_iter points to the label-pair that will be spared (u and v will end up receiving this label)
    void match_nodes(const NodeDesc host_u, const NodeDesc guest_v, const typename LabelMatching::iterator& xy_label_iter) {
      // step 3a: prune guest and remove nodes with label below v (in guest) from both host and guest - also remove their label entries in HG_label_match
      const auto& vlabel = xy_label_iter->first;
      const NodeDesc host_x = xy_label_iter->second.first;
      const NodeDesc guest_y = xy_label_iter->second.second;
      const auto& host = manager.contain.host;

      std::cout << "\tMATCH: marking "<<guest_v<<" (guest) & "<<host_u<<" (host) with label "<<vlabel<<"\n";
      
      std::cout << "\tMATCH: pruning guest at "<<guest_v<<" (except "<<guest_y<<"):\n"<<manager.contain.guest<<"\n";
      prune_guest_except(guest_v, guest_y); // note: prune_guest automatically removes everything it encounters from HG_label_match
      std::cout << "\tMATCH: pruned guest:\n"<<manager.contain.guest<<"\n";

      //NOTE: keep track of nodes in the host who have one of their incoming edges removed as component roots may now see them
      std::cout << "\tMATCH: pruning host at "<<host_u<<" (except "<<host_x<<"):\n"<<host<<" with comp-DAG\n";
      manager.contain.comp_info.comp_DAG.print_subtree_with_data();

      std::cout << "component roots:\n";
      for(const NodeDesc u: manager.contain.host.nodes()) std::cout << u << ": "<<manager.contain.comp_info.comp_root_of(u) << "\n";
      
      prune_host_except(host_u, host_x);
      
      std::cout << "\tMATCH: done pruning host; orphan-queue "<<manager.remove_orphans.node_queue<<"\n";
      manager.remove_orphans.apply();
      std::cout << "\tMATCH: pruned host:\n"<<host<<"\n";
    }
  };


  // a class managing the application of all reduction rules in the correct order
  template<class _Containment>
  struct ReductionManager {
    using Containment = _Containment;
    using ComponentDAG = typename Containment::ComponentInfos::ComponentDAG;

    Containment& contain;
    NodeSet orphan_queueu;

    OrphanRemover<ReductionManager> remove_orphans;
    ReticulationMerger<ReductionManager> reti_merge;
    TriangleReducer<ReductionManager> triangle_rule;
    CherryPicker<ReductionManager> cherry_rule;
    VisibleComponentRule<ReductionManager> visible_comp_rule;
    HostGuestMatch<ReductionManager> HG_match_rule;

    ReductionManager(Containment& c):
      contain(c),
      remove_orphans(*this),
      reti_merge(*this),
      triangle_rule(*this),
      cherry_rule(*this),
      visible_comp_rule(*this),
      HG_match_rule(*this)
    {}


    void clean_orphan_later(const NodeDesc u) {
      remove_orphans.add(u);
    }

    void suppress_node_in_host(const NodeDesc u) {
      auto& host = contain.host;
      auto& info = contain.comp_info;
      assert(host.in_degree(u) <= 1);
      assert(host.out_degree(u) == 1);

      const NodeDesc u_child = host.any_child(u);
      std::cout << "suppressing node "<<u<<" with child "<<u_child<<"\n";
      if(host.in_degree(u_child) == 1) {
        if(host.is_leaf(u_child)) {
          std::cout << "suppressing node "<<u<<" with leaf-child "<<u_child<<" and comp-root "<<info.comp_root_of(u)<<"\n";
          // if u's child is a leaf, we contract that leaf onto u
          if(info.comp_root_of(u) == u) {
            assert((host.in_degree(u) == 0) || host.is_reti(host.parent(u)));
            // if u is a component-root, then it should be a leaf in the comp_DAG, which we can remove now
            const NodeDesc u_in_cDAG = info.N_to_comp_DAG.at(u);
            assert(info.comp_DAG.is_leaf(u_in_cDAG));
            info.comp_DAG.remove_node(u_in_cDAG);
            info.replace_comp_root(u, u_child);
            std::cout << "changed comp-root of "<<u_child<<" to "<<info.comp_root_of(u_child)<<"\n";
          }
          std::cout << "contracting-down "<<u<<" into "<<u_child<<"\n";
          // then, we can safely contract-up u's child
          remove_from_queues(u);
          host.contract_down(u, u_child);
        } else {
          std::cout << "contracting-up "<<u_child<<" into "<<u<<"\n";
          assert(info.comp_root_of(u_child) != u_child);
          remove_from_queues(u_child);
          host.contract_up(u_child, u);
          // if u_child was also suppressible, we have to re-run for u now
          if(host.out_degree(u) == 1) suppress_node_in_host(u);
        }
      } else {
        remove_from_queues(u);
        // if u's child is a reticulation then contract u upwards
        assert(host.is_reti(u_child));
        // NOTE: if everything is consistent then u's parent is a reti if and only if u is a comp root
        const NodeDesc u_parent = host.parent(u);
        std::cout << u << " has parent "<<u_parent<<" and reticulation child "<<u_child << "\n";
        std::cout << "contracting-up "<<u<<" into "<<u_parent<<"\n";
        if(info.comp_root_of(u) == u) {
          assert(host.is_reti(u_parent));
          assert(info.N_to_comp_DAG.contains(u));
          // if u is a component-root, then it should have out-deg 1 in the comp_DAG, so we can contract down
          const NodeDesc u_in_cDAG = info.N_to_comp_DAG.at(u);
          if(info.comp_DAG.out_degree(u_in_cDAG) == 1)
            info.comp_DAG.contract_down_unique(u_in_cDAG);
          else info.comp_DAG.remove_node(u_in_cDAG);
          // NOTE: since both u's child and parent are reticulations, we also want to contract u's parent down onto u's child
          host.contract_up(u, u_parent);
          contract_reti_onto_reti_child(u_parent);
        } else {
          // NOTE: it might be that u_child is already a child of u_parent, in which case we'll simply delete u instead of contracting it
          if(host.contract_up_unique(u, u_parent))
            clean_orphan_later(u_parent);
        }
      }
    }

    void remove_edge_in_host(const NodeDesc u, const NodeDesc v) {
      auto& host = contain.host;
      assert(host.in_degree(u) == 1);
      assert(host.out_degree(v) == 1);

      std::cout << "reduction manager removing edge "<< u << " -> "<< v<<'\n';
      host.remove_edge(u, v);
      contain.comp_info.react_to_edge_deletion(u, v);

      // if v is now suppressible, then contract v onto its child
      if(host.in_degree(v) == 1) {
        remove_from_queues(v);
        host.contract_down(v);
      }
      // if u is now suppressible, then we either contract u down or its child up
      if(host.out_degree(u) == 1) {
        clean_orphan_later(u);
        //suppress_node_in_host(u);
      }
    }

    void contract_reti_onto_reti_child(const NodeDesc u) {
      auto& host = contain.host;
      assert(host.out_degree(u) == 1);
      assert(host.is_reti(host.child(u)));
      remove_from_queues(u);
      host.contract_down_unique(u);
    }


/*   
    NodeDesc& comp_root_of(const NodeDesc u) const { return contain.comp_root_of(u); }
    NodeDesc& visible_leaf_of(const NodeDesc u) const { return contain.visible_leaf_of(u); }
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
        clean_up_node(value_pop(to_clean), recursive, apply_reti_reduction);
      }
    }
    void clean_up_later(const NodeDesc u) {
      append(to_clean, u);
      std::cout << "cleaning "<<u<<" later, queue is now "<<to_clean<<"\n";
    }
*/
    auto find_label(auto& label) const { return contain.HG_label_match.find(label); }

    auto find_label_in_guest(const NodeDesc u) const {
      return contain.HG_label_match.find(contain.guest.label(u));
    }
    auto find_label_in_host(const NodeDesc u) const {
      return contain.HG_label_match.find(contain.host.label(u));
    }
    void remove_from_queues(const NodeDesc u) {
      my_erase(reti_merge.node_queue, u);
      my_erase(remove_orphans.node_queue, u);
      my_erase(cherry_rule.node_queue, u);
    }

    void apply() {
      std::cout << "\n ===== REDUCTION RULES ======\n\n";
      std::cout << "reti-m queue: "<<reti_merge.node_queue << "\n";

      reti_merge.init_queue();
      reti_merge.apply();

      // if, at some point, there are only 2 leaves left, then simply say 'yes'
      while(!contain.failed && (contain.HG_label_match.size() > 2)) {
        std::cout << "\n === restart rule-application ===\n";
        std::cout << "orphan queue: "<<remove_orphans.node_queue << "\n";
        std::cout << "cherry queue: "<<cherry_rule.node_queue << "\n";

        std::cout << "host:\n" << contain.host << "\nguest:\n" << contain.guest << "\ncomp-DAG:\n";
        contain.comp_info.comp_DAG.print_subtree(std::cout, [](const NodeDesc u){ return std::to_string(u); });
        std::cout << "label matching: "<<contain.HG_label_match<<"\n";
        std::cout << "comp-roots:\n"; for(const NodeDesc u: contain.host.nodes()) std::cout << u <<": " << contain.comp_info.comp_root_of(u) <<'\n';

        if(remove_orphans.apply()) continue;
        if(triangle_rule.apply()) continue;
      
        cherry_rule.init_queue();
        if(cherry_rule.apply()) continue;
        
        if(visible_comp_rule.apply()) continue;

        reti_merge.init_queue();
        if(reti_merge.apply()) continue;
      }
      std::cout << "\n ===== REDUCTION done ======\n\n";
      std::cout << "host:\n" << contain.host << "guest:\n" << contain.guest << "comp-DAG:\n";
      contain.comp_info.comp_DAG.print_subtree_with_data();
    }
  };


}
