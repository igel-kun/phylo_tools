
#pragma once

#include "optional_tuple.hpp"
#include "union_find.hpp"
#include "types.hpp"
#include "network.hpp"
#include "edge_emplacement.hpp"
#include "static_capacity_vector.hpp"

namespace PT{

#warning "TODO: enable the user to use node-data instead of NodeMap's throughout the library (especially for translations)!"

  // map nodes to their TreeComponentData
  //NOTE: if the nodes of Network have node data that derives from TreeComponentData, then we use this node data by default, otherwise we use a map
  template<StrictPhylogenyType Network, size_t num_vis_leaves = 1>
  class TreeComponentInfos {
    // TODO: deal with the case the Network has multiple roots! For now, we just disallow it
    static_assert(Network::has_unique_root);

    Network& N;
    
    // we want to be able to merge tree components; thus, we will use a disjoint-set forest
    // NOTE: as payload, we store one visible leaf
    //       we cannot store more than one because we have operations that might replace the second visible leaf by the first,
    //       in which case we cannot distinguish the cases where there's a single visible leaf or more than one
    // NOTE: this has to be declared mutable to allow path-compression to occur
    mutable mstd::DisjointSetForest<NodeDesc, NodeDesc> comp_root;

  public:
    // each node in the component DAG will know its corresponding node in the Network
    using ComponentDAG = DefaultNetwork<NodeDesc>;
//    using Emplacer = EdgeEmplacerWithHelper<false, ComponentDAG, void, NodeTranslation&, mstd::IdentityFunction<NodeDesc>>;
#warning "TODO: make those protected and expose only const refs"
#warning "TODO: store the emplacer instead of the translation here"
    // N_to_comp_DAG translates nodes of N into nodes of the component DAG
    NodeTranslation N_to_comp_DAG;
    ComponentDAG comp_DAG;
   

    // construction
    TreeComponentInfos(Network& _N):
      N(_N)
    {
      if(!N.empty())
        compute_comp_DAG();
    }

    // we want to be able to give a new Network-reference when copy-constructing
    TreeComponentInfos(TreeComponentInfos&& tc, Network& _N):
      N(_N), N_to_comp_DAG(std::move(tc.N_to_comp_DAG)), comp_DAG(std::move(tc.comp_DAG))
    {}

    NodeDesc comp_root_of(const NodeDesc x) const {
      if(const auto rep = comp_root.lookup(x).first; rep)
        return rep->get_representative();
      else return NoNode;
    }
    
    const auto& get_comp_root() const { return comp_root; }

    // replace a component root by a leaf
    // NOTE: this is useful when a reticulation between a non-trivial comp root 'old_rt' and a trivial comp root 'new_rt' is destroyed
    //       since we cannot rename the trivial comp root, we have to replace the old comp root with the new comp root in the data structures
    void replace_comp_root(const NodeDesc old_rt, const NodeDesc new_rt) {
      assert(N.is_leaf(new_rt));
      comp_root.make_representative(new_rt);
    }

    // register the fact that the component of x is now rooted at rt
    // NOTE: you can also set a visible leaf with that
    void set_comp_root(const NodeDesc x, const NodeDesc rt, const NodeDesc vis_leaf = NoNode) {
      assert(x != NoNode);
      assert(rt != NoNode);

      const auto [iter, success] = comp_root.emplace_set(x, vis_leaf);
      auto& x_set = iter->second;
      if(x != rt) {
        // if x != rt then we suppose that rt already exists in the set forest
        auto& rt_set = comp_root.set_of(rt);
        // then, we can just add x to this set and update the visible leaf
        DEBUG4(std::cout << "updating comp root of "<<x<<" ("<<x_set<<") to "<<rt_set.get_representative()<<" ("<<rt_set<<") with visible leaf "<<vis_leaf<<"\n");
        // if x was already represented in the disjoint set forest, then merge it onto rt
        if(vis_leaf != NoNode) {
          x_set.payload = rt_set.payload = vis_leaf;
        } else if(x_set.payload != NoNode) {
          rt_set.payload = x_set.payload;
        }
        comp_root.merge_sets_keep_order(rt_set, x_set);
      } else {
        if(!success) {
          DEBUG4(std::cout << "splitting off "<<x_set<<"\n");
          comp_root.split_element(x);
          if(vis_leaf) x_set.payload = vis_leaf;
        }
      }
      DEBUG4(std::cout << "comp root entries now "<<x<<": "<<x_set<<", "<<x_set.get_representative()<<": "<< comp_root.set_of(x) <<"\n");
    }
 
    NodeDesc visible_leaf_of(const NodeDesc u) const {
      const auto _set = comp_root.lookup(u).first;
      return _set ? _set->payload : NoNode;
    }
    bool replace_visible_leaf(const NodeDesc u, const NodeDesc new_leaf) {
      const auto [_set, _rep_set] = comp_root.lookup(u);
      if(_set) {
        assert(_rep_set);
        _set->payload = new_leaf;
        if(_rep_set != _set)
          _rep_set->payload = new_leaf;
        return true;
      } else return false;
    }

 
  protected:

    // if all v's parents have the same component root, then set that root for v and return it, otherwise, return NoNode
    // NOTE: the callable argument is applied to the component roots of all node we encounter
    // NOTE: if quit_early is true, we'll return failure once we know there is no consensus
    // NOTE: if recursive is true, we will call ourselves recursively for all reticulation parents
    template<bool recursive = true, bool set_this_root = !recursive, bool quit_early = set_this_root, class Callback = mstd::IgnoreFunction<void>>
    NodeDesc component_root_consensus_among_parents(const NodeDesc v, Callback&& callback = Callback()) {
      DEBUG4(std::cout << "computing consensus for nodes above "<<v<<"\n");
      NodeDesc consensus = v;
      for(const NodeDesc u: N.parents(v)) {
        // step 0: get the component root of u, if it has one
        NodeDesc rt = comp_root_of(u);
        // step 0.5: apply the callback
        callback(rt);
        // step 1: if we are to recursively compute component roots and u doesn't have one, then recurse for u
        if constexpr (recursive)
          if(rt == NoNode)
            rt = component_root_consensus_among_parents<recursive, true, quit_early>(u, callback);
        // step 2: if we are supposed to quit early and we concluded that u does not have a compoennt-root, then there will be no consensus for v
        if constexpr (quit_early)
          if(rt == NoNode)
            return NoNode;
        // step 3: merge rt into the current consensus
        if(consensus != v) {
          if(consensus != rt) {
            if constexpr (!quit_early)
              consensus = NoNode;
            else return NoNode;
          }
        } else consensus = rt;
      }
      // step 4: update the component_root of v
      if constexpr (set_this_root)
        if(consensus != NoNode)
          set_comp_root(v, consensus);
      return consensus;
    }


    void compute_comp_DAG() {
      DEBUG3(std::cout << "\nconstructing component-DAG...\n");

      // create an edge-emplacer for the result DAG; we won't have to track roots since they will be the same as the roots of N
      // NOTE: the lambda tells the emplacer that the node data of the node corresponding to u will be u itself
      //       (thus, each new node has, as its data, the NodeDesc of its corresponding original node)
      auto emplacer = EdgeEmplacers<false>::make_emplacer(comp_DAG, N_to_comp_DAG, [](const NodeDesc u){ return u;});
      
      NodeVec non_trivial_roots;
      NodeVec trivial_roots;

      emplacer.create_copy_of(N.root());
      emplacer.mark_root(N.root());
      //std::cout << "setting comp data of "<<N.root()<<"\n";
      //set_comp_root(N.root());
      DEBUG4(std::cout << "computing component roots\n");
      compute_component_roots(trivial_roots, non_trivial_roots);
      // construct the component DAG from the non-trivial roots
      DEBUG4(std::cout << "1st pass over component roots\n" << comp_root<<"\nemplacer translation @"<< &(emplacer.helper.old_to_new)<<" our translation @"<<&N_to_comp_DAG<<"\n");

      // construct visibility also from the trivial roots (the leaves), but don't add their edges to the component DAG
      // NOTE: by going through the roots in reverse order, we make sure to spread visible leaves upwards
      DEBUG4(std::cout << "2nd pass over component roots\n");
      for(auto iter = trivial_roots.rbegin(); iter != trivial_roots.rend(); ++iter)
        install_trivial_root(*iter);
      for(auto iter = non_trivial_roots.rbegin(); iter != non_trivial_roots.rend(); ++iter)
        compute_edges(*iter, emplacer);
    }

    // compute component roots (trivial (that is, leaves) and non-trivial)
    // NOTE: this sets the comp_root of all (non-leaf) tree nodes
    void compute_component_roots(NodeVec& trivial_roots, NodeVec& non_trivial_roots) {
      for(const NodeDesc u: N.nodes_preorder()) {
        auto& u_node = node_of<Network>(u);
        if(!u_node.is_reti()) {
          const auto& pu = u_node.parents();
          // if the parent is a reticulation, register new component root
          if(pu.empty() || Network::is_reti(mstd::front(pu))){
            if(u_node.is_leaf()) {
              trivial_roots.push_back(u);
              set_comp_root(u, u, u);
            } else {
              non_trivial_roots.push_back(u);
              set_comp_root(u, u);
            }
          } else {
            // if the parent is not a reticulation, copy their component root
            const NodeDesc u_parent = mstd::front(pu);
            const NodeDesc vl = u_node.is_leaf() ? u : NoNode;
            set_comp_root(u, u_parent, vl);
            if(vl != NoNode)
              comp_root.set_of(u_parent).payload = vl;
          }
        }
      }
    }


    // build the network of component roots by following the reticulations above each root
    void compute_edges(const NodeDesc u, auto& edge_emplacer) {
      NodeSet components_above;
      component_root_consensus_among_parents(u, [&components_above](const NodeDesc x) { if(x != NoNode) components_above.emplace(x); });
      // insert edges into the component DAG
      for(const NodeDesc v: components_above)
        edge_emplacer.emplace_edge(v, u);
    }

    void install_trivial_root(const NodeDesc u) {
      const NodeDesc rt = component_root_consensus_among_parents(u);
      // set the visible leaves if appropriate
      if(rt != NoNode)
        replace_visible_leaf(rt, u);
    }


    // if x is a reticulation, get the root of the tree component below x, otherwise return x itself
    NodeDesc get_tree_comp_below(NodeDesc x) {
      while(N.is_reti(x)) {
        const auto& x_children = N.children(x);
        assert(x_children.size() <= 1);
        if(x_children.size() == 1) {
          x = mstd::front(x_children);
        } else return NoNode;
      }
      return x;
    }

    // contract lower_root onto upper_root in the component after an edge deletion from one of the nodes in the compoenent of 'other_comp_root'
    // NOTE: this takes nodes in N, not in the comp_DAG!
    void merge_tree_components(const NodeDesc upper_root, const NodeDesc lower_root, const NodeDesc other_comp_root) {
      assert(other_comp_root != NoNode);
      assert(upper_root != NoNode);
      assert(lower_root != NoNode);
      assert(upper_root != lower_root);
      DEBUG4(std::cout << "component-DAG:\n");
      comp_DAG.print_subtree_with_data();
      DEBUG4(std::cout << "translate: " << N_to_comp_DAG << "\n");
      const auto lower_root_in_cDAG_iter = N_to_comp_DAG.find(lower_root);
      if(lower_root_in_cDAG_iter != N_to_comp_DAG.end()) {
        const NodeDesc lower_root_in_cDAG = lower_root_in_cDAG_iter->second;
        // NOTE: upper_root and other_comp_root MUST have a corresponding node in the cDAG
        assert(N_to_comp_DAG.contains(upper_root));
        assert(N_to_comp_DAG.contains(other_comp_root));
        
        const NodeDesc other_comp_root_in_cDAG = N_to_comp_DAG.at(other_comp_root);
        const NodeDesc upper_root_in_cDAG = N_to_comp_DAG.at(upper_root);
        // if the two are different in the component DAG, then remove the edge corresponding to the deleted edge in N
        if(other_comp_root_in_cDAG != upper_root_in_cDAG) {
          assert(comp_DAG.is_edge(other_comp_root_in_cDAG, lower_root_in_cDAG));
          comp_DAG.remove_edge(other_comp_root_in_cDAG, lower_root_in_cDAG);
        }
        // NOTE: upper_root should be the only predecessor of lower_root in the component DAG
        assert(comp_DAG.in_degree(lower_root_in_cDAG) == 1);
        // NOTE: it might happen that upper_root already has an edge to lower_root in the cDAG, in which case we simply remove v from the cDAG
        for(const NodeDesc x: comp_DAG.children(upper_root_in_cDAG))
          comp_DAG.remove_edge(lower_root_in_cDAG, x);
        // finally, contract lower_root_in_cDAG up
        comp_DAG.contract_up(lower_root_in_cDAG);
        N_to_comp_DAG.erase(lower_root_in_cDAG_iter);
      }
      DEBUG4(std::cout << "\tREACT: updating component roots...\n");
      set_comp_root(lower_root, upper_root);
    }

  public:

    // react to an edge deletion in the network between nodes u & v
    // NOTE: this updates the component DAG, the component-root map, the visible-leaf map, and  the N_to_comp DAG translation
    // NOTE: if the deletion of uv leaves v suppressible and v's parent is a reticulation and
    //       (1) 'v_may_become_comp_root' == true, then we'll create a new node in the component DAG for v and put it at the appropriate spot
    //       (2) 'v_may_become_comp_root' == false, then we will pretend as if v was a reticulation and not do anything else with it
    template<bool v_may_become_comp_root = false>
    void react_to_edge_deletion(const NodeDesc u, const NodeDesc v) {
      static_assert(!v_may_become_comp_root && "unimplemented");
      DEBUG4(std::cout << N << "\n");
      DEBUG4(std::cout << "\tREACT: reacting to the deletion of "<<u<<" -> "<<v<<"\n");
      const NodeDesc u_rt = comp_root_of(u);
      assert(u_rt != NoNode);
      NodeDesc rt_below_v = get_tree_comp_below(v);
      DEBUG4(std::cout << "\tREACT: comp-root of "<< u <<" is "<<u_rt<<" and the comp-root below "<<v<<" is "<<rt_below_v<<"\n");
      
      assert(N.out_degree(v) <= 1);
      if(rt_below_v == v) { // v is no longer a reticulation, which means that v is now suppressible or a leaf; in the latter case nothing has to be done
        assert(N.in_degree(v) == 1);
        const auto& v_children = N.children(v);
        if(!v_children.empty()) {
          assert(v_children.size() == 1);
          DEBUG4(std::cout << "\tREACT: "<< v <<" is now suppressible\n");
          // if v is suppressible, we still might have to remove the edge between rt_u and the component root below v in the component DAG
          const NodeDesc v_child = mstd::front(v_children);
          DEBUG4(std::cout << "\tREACT: "<< v_child <<"'s comp root is "<< comp_root_of(v_child) << "\n");
          const NodeDesc v_parent = N.parent(v);
          if(!N.is_reti(v_child)) {
            // if v's child is not a reticulation, it must be a (possibly trivial) component root (remember that v was a reticulation before removing uv)
            assert(comp_root_of(v_child) == v_child); 
            // IMPORTANT NOTE: if v's parent is not a reticulation, then we just merged 2 tree compoents together, so there's plenty of work to do
            if(!N.is_reti(v_parent)) {
              // since v_parent is not a reticulation, it must have a component root
              const NodeDesc v_parent_rt = comp_root_of(v_parent);
              DEBUG4(std::cout << "\tREACT: merging tree-components of "<<v_parent_rt<<" and "<<v_child<<"\n");
              merge_tree_components(v_parent_rt, v_child, comp_root_of(u));
              set_comp_root(v, v_parent_rt);

              // each reticulation below v may just have become visible by v_parent_rt
              NodeVec todo{v};
              NodeVec retis_below;
              while(!todo.empty()) {
                const NodeDesc x = mstd::value_pop(todo);
                assert(N.in_degree(x) <= 1);
                for(const NodeDesc y: N.children(x))
                  if(N.out_degree(y) != 0){
                    if(N.in_degree(y) == 1)
                      mstd::append(todo, y);
                    else mstd::append(retis_below, y);
                  }
              }
              DEBUG4(std::cout << "retis below: "<<retis_below<<"\n");
              for(NodeDesc x: retis_below) {
                while((N.out_degree(x) == 1) && (comp_root_of(x) == NoNode) && (component_root_consensus_among_parents<false>(x) != NoNode))
                  x = N.child(x);
              }

              DEBUG4(std::cout << "cDAG after component update:\n";comp_DAG.print_subtree_with_data();)
            } else {
              if constexpr (v_may_become_comp_root) {
                assert(false && "unimplemented");
#warning "TODO: write me"
              }
            }
          } else {
            if constexpr (v_may_become_comp_root) {
              assert(false && "unimplemented");
#warning "TODO: write me"
            } else {
              const NodeDesc v_parent_rt = comp_root_of(v_parent);
              if(v_parent_rt != NoNode) 
                set_comp_root(v, v_parent_rt);
            }
            // NOTE: if v's child is a reti, we may just act as if the edge removal was between u and that child (slightly abusing notation)
            react_to_edge_deletion(u, v_child);
          }
        }
      } else { // v is a reticulation
        DEBUG4(std::cout << "\tREACT: "<< v <<" is a reticulation above the comp-root "<<rt_below_v<<"\n");
        assert(rt_below_v != NoNode);

        if(!N.is_leaf(rt_below_v)) {
          // step 0: check if u_rt is still above v_rt or not
          // NOTE: this updates the component root of v and all reticulation children of u on the way
          bool u_rt_is_above_rt_below_v = false;
          component_root_consensus_among_parents(rt_below_v, [&u_rt_is_above_rt_below_v, u_rt](const NodeDesc x){ u_rt_is_above_rt_below_v |= (x == u_rt); });
          // step 1: if u_rt is no longer above rt_below_v, then remove the edge from the comp_DAG
          if(!u_rt_is_above_rt_below_v) {
            assert(N_to_comp_DAG.contains(rt_below_v));
            assert(N_to_comp_DAG.contains(u_rt));
            const NodeDesc rt_below_v_in_cDAG = N_to_comp_DAG.at(rt_below_v);
            const NodeDesc u_rt_in_cDAG = N_to_comp_DAG.at(u_rt);
            comp_DAG.remove_edge(u_rt_in_cDAG, rt_below_v_in_cDAG);
            assert(comp_DAG.in_degree(rt_below_v_in_cDAG) > 0);
          }
        } else component_root_consensus_among_parents(rt_below_v);
      }
    }

    // update comp-root and visibility above a leaf l after it has been regrafted
    void react_to_leaf_regraft(const NodeDesc l) {
      assert(N.in_degree(l) == 1);
      assert(!N.label(l).empty());
      const NodeDesc pl = N.parent(l);
      if(N.in_degree(pl) > 1) {
        set_comp_root(l, l, l);
      } else {
        set_comp_root(l, comp_root_of(pl), l);
      }
    }


  };
    



}
