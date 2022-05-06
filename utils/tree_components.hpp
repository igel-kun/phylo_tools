
#pragma once

#include "optional_tuple.hpp"
#include "union_find.hpp"
#include "types.hpp"
#include "network.hpp"
#include "edge_emplacement.hpp"

namespace PT{

#warning "TODO: enable the user to use node-data instead of NodeMap's throughout the library (especially for translations)!"

  // map nodes to their TreeComponentData
  //NOTE: if the nodes of Network have node data that derives from TreeComponentData, then we use this node data by default, otherwise we use a map
  template<StrictPhylogenyType Network>
  class TreeComponentInfos {
    // TODO: deal with the case the Network has multiple roots! For now, we just disallow it
    static_assert(Network::has_unique_root);

    Network& N;
    
    // we want to be able to merge tree components; thus, we will use a disjoint-set forest
    // NOTE: as payload, we store visible leaves along with component roots
    // NOTE: this has to be declared mutable to allow path-compression to occur
    mutable std::DisjointSetForest<NodeDesc, NodeDesc> comp_info;

  public:
    // each node in the component DAG will know its corresponding node in the Network
    using ComponentDAG = DefaultNetwork<NodeDesc>;
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
    TreeComponentInfos(const TreeComponentInfos& tc, Network& _N):
      N(_N), N_to_comp_DAG(), comp_DAG(tc.comp_DAG)
    {}
    TreeComponentInfos(TreeComponentInfos&& tc, Network& _N):
      N(_N), N_to_comp_DAG(std::move(tc.N_to_comp_DAG)), comp_DAG(std::move(tc.comp_DAG))
    {}

  public:

    NodeDesc visible_leaf_of(const NodeDesc u) const { assert(comp_info.contains(u)); return comp_info.set_of(u).payload; }
    void set_visible_leaf(const NodeDesc u, const NodeDesc leaf) { assert(comp_info.contains(u)); comp_info.set_of(u).payload = leaf; }

    // return the root of x's tree component
    NodeDesc comp_root_of(const NodeDesc x) {
      return comp_info.contains(x) ? comp_info.set_of(x).get_representative() : NoNode;
    }

    // replace a component root by a leaf
    // NOTE: this is useful when a reticulation between a non-trivial comp root 'old_rt' and a trivial comp root 'new_rt' is destroyed
    //       since we cannot rename the trivial comp root, we have to replace the old comp root with the new comp root in the data structures
    void replace_comp_root(const NodeDesc old_rt, const NodeDesc new_rt) {
      assert(N.is_leaf(new_rt));
      assert(comp_root_of(old_rt) == old_rt);
      assert(comp_root_of(new_rt) == old_rt);
      comp_info.make_representative(new_rt);
    }

    // register the fact that the component of x is now rooted at rt
    // NOTE: you can also set a visible leaf with that
    void set_comp_root(const NodeDesc x, const NodeDesc rt, const NodeDesc vis_leaf = NoNode) {
      assert(x != NoNode);
      assert(rt != NoNode);
      if(x != rt) {
        assert(comp_info.contains(rt));
        // if x != rt then we suppose that rt already exists in the set forest
        auto& rt_set = comp_info.set_of(rt);
        // then, we can just add x to this set and update the visible leaf
        std::cout << "updating comp root of "<<x<<" to "<<rt_set.get_representative()<<"\n";
        const auto [iter, success] = comp_info.emplace_item_to_set(rt_set, x, vis_leaf);
        if(!success) {
          // if x was already represented in the disjoint set forest, then merge it onto rt
          comp_info.merge_sets_keep_order(rt_set, iter->second);
        }
        if(vis_leaf != NoNode) rt_set.payload = vis_leaf;
      } else {
        std::cout << "adding new comp root for "<<x<<" with visible leaf "<<vis_leaf<<"\n";
        comp_info.add_new_set(x, vis_leaf);
        // if x == rt then we are trying to add a new set into the set-forest
      }
    }
    void set_comp_root(const NodeDesc x) { set_comp_root(x, x); }
  
  protected:

    // if all v's parents have the same component root, then set that root for v and return it, otherwise, return NoNode
    // NOTE: the callable argument is applied to the component roots of all node we encounter
    // NOTE: if quit_early is true, we'll return failure once we know there is no consensus
    // NOTE: if recursive is true, we will call ourselves recursively for all reticulation parents
    template<bool recursive = true, bool set_this_root = false, bool quit_early = !recursive, class Callback = std::IgnoreFunction<void>>
    NodeDesc component_root_consensus_among_parents(const NodeDesc v, Callback&& callback = Callback()) {
      std::cout << "computing consensus for nodes above "<<v<<"\n";
      NodeDesc consensus = v;
      for(const NodeDesc u: N.parents(v)) {
        // step 0: get the component root of u, if it has one
        NodeDesc rt = comp_root_of(u);
        // step 0.5: apply the callback
        callback(rt);
        // step 1: if we are to recursively compute component roots and u doesn't have one, then recurse for u
        if constexpr (recursive) {
          if(rt == NoNode) rt = component_root_consensus_among_parents<recursive, true, quit_early>(u, callback);
        }
        // step 2: if we are supposed to quit early and we concluded that u does not have a compoennt-root, then there will be no consensus for v
        if constexpr (quit_early) {
          if(rt == NoNode) return NoNode;
        }
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
      std::cout << "making emplacer...\n";
      auto emplacer = EdgeEmplacers<false>::make_emplacer(comp_DAG, N_to_comp_DAG, [](const NodeDesc u){ return u;});
      
      NodeVec non_trivial_roots;
      NodeVec trivial_roots;

      emplacer.create_copy_of(N.root());
      emplacer.mark_root(N.root());
      //std::cout << "setting comp data of "<<N.root()<<"\n";
      //set_comp_root(N.root());
      std::cout << "computing component roots\n";
      compute_component_roots(trivial_roots, non_trivial_roots);
      // construct the component DAG from the non-trivial roots
      std::cout << "1st pass over component roots\n";
      std::cout << comp_info<<"\n";
      std::cout << "emplacer translation @"<< &(emplacer.helper.old_to_new)<<" our translation @"<<&N_to_comp_DAG<<"\n";
      for(const NodeDesc rt: non_trivial_roots)
        compute_edges<true>(rt, emplacer);
      // construct visibility also from the trivial roots (the leaves), but don't add their edges to the component DAG
      std::cout << "2nd pass over component roots\n";
      for(const NodeDesc rt: trivial_roots)
        compute_edges<false>(rt, emplacer);

    }

    // compute component roots (trivial (that is, leaves) and non-trivial)
    // NOTE: this sets the comp_root of all (non-leaf) tree nodes
    void compute_component_roots(NodeVec& trivial_roots, NodeVec& non_trivial_roots) {
      for(const NodeDesc u: N.nodes_preorder()) {
        auto& u_node = node_of<Network>(u);
        if(!u_node.is_reti()) {
          const auto& pu = u_node.parents();
          // if the parent is a reticulation, register new component root
          if(pu.empty() || Network::is_reti(front(pu))){
            if(u_node.is_leaf()) {
              trivial_roots.push_back(u);
              set_comp_root(u, u, u);
            } else {
              non_trivial_roots.push_back(u);
              set_comp_root(u, u);
            }
          } else {
            // if the parent is not a reticulation, copy their component root
            set_comp_root(u, front(pu), (u_node.is_leaf() ? u : NoNode));
          }
        }
      }
    }


    // build the network of component roots by following the reticulations above each root
    template<bool make_edges>
    void compute_edges(const NodeDesc u, auto& edge_emplacer) {
      assert(!N.is_reti(u));
      NodeSet components_above;
      NodeDesc rt;
      if constexpr (make_edges)
        rt = component_root_consensus_among_parents(u, [&components_above](const NodeDesc x) { if(x != NoNode) components_above.emplace(x); });
      else 
        rt = component_root_consensus_among_parents(u);
      if constexpr (make_edges)
        for(const NodeDesc v: components_above)
          edge_emplacer.emplace_edge(v, u);
    }


    // if x is a reticulation, get the root of the tree component below x, otherwise return x itself
    NodeDesc get_tree_comp_below(NodeDesc x) {
      while(N.is_reti(x)) {
        const auto& x_children = N.children(x);
        assert(x_children.size() <= 1);
        if(x_children.size() == 1) {
          x = front(x_children);
        } else return NoNode;
      }
      return x;
    }

  public:

    // react to an edge deletion in the network between nodes u & v
    void react_to_edge_deletion(const NodeDesc u, const NodeDesc v) {
      std::cout << "\tREACT: reacting to the deletion of "<<u<<" -> "<<v<<"\n";
      const NodeDesc u_rt = comp_root_of(u);
      assert(u_rt != NoNode);
      NodeDesc rt_below_v = get_tree_comp_below(v);
      std::cout << "\tREACT: comp-root of "<< u <<" is "<<u_rt<<" and the comp-root below "<<v<<" is "<<rt_below_v<<"\n";
      
      if(rt_below_v != v) { // v is a reticulation
        std::cout << "\tREACT: "<< v <<" is a reticulation above the comp-root "<<rt_below_v<<"\n";
        assert(rt_below_v != NoNode);

        if(!N.is_leaf(rt_below_v)) {
          // step 0: check if u_rt is still above v_rt or not
          // NOTE: this updates the component root of v and all reticulation children of u on the way
          bool u_rt_is_above_rt_below_v = false;
          component_root_consensus_among_parents(rt_below_v, [&u_rt_is_above_rt_below_v, u_rt](const NodeDesc x){ u_rt_is_above_rt_below_v |= (x == u_rt); });
          std::cout << "mark: "<<u_rt_is_above_rt_below_v<<"\n";
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
      } else {
        // v is no longer a reticulation, which means that v is now suppressible or a leaf; in the latter case nothing has to be done
        const auto& v_children = N.children(v);
        assert(v_children.size() <= 1);
        if(v_children.size() == 1) {
          std::cout << "\tREACT: "<< v <<" is now suppressible\n";
          // if v is suppressible, we still might have to remove the edge between rt_u and the component root below v in the component DAG
          const NodeDesc v_child = front(v_children);
          std::cout << "\tREACT: "<< v_child <<"'s comp root is "<< comp_root_of(v_child) << "\n";
          if(!N.is_reti(v_child)) {
            // if v's child is not a reticulation, it must be a (possibly trivial) component root (remember that v was a reticulation before removing uv)
            assert(comp_root_of(v_child) == v_child);
            // IMPORTANT NOTE: if v's parent is not a reticulation, then we just merged 2 tree compoents together, so there's plenty of work to do
            const auto& v_parents = N.parents(v);
            assert(v_parents.size() == 1);
            const NodeDesc v_parent = front(v_parents);
            if(!N.is_reti(v_parent)) {
              const NodeDesc v_parent_rt = comp_root_of(v_parent);
              // since v_parent is not a reticulation, it must have a component root
              assert(v_parent_rt != NoNode);
              // further, this component root cannot be the same as the root below v since otherwise we have a cycle in N
              assert(v_parent_rt != v_child);
              // thus, v_child's component_root is not the same as v_parent's -- we have to merge them in 2 steps:
              // step 1: contract v's component root onto v_parent's in the component DAG
              const auto v_child_in_cDAG_iter = N_to_comp_DAG.find(v_child);
              if(v_child_in_cDAG_iter != N_to_comp_DAG.end()) {
                const NodeDesc v_child_in_cDAG = v_child_in_cDAG_iter->second;
                // NOTE: v_parent_rt should be the only predecessor of v_child in the component DAG
                assert(comp_DAG.in_degree(v_child_in_cDAG) == 1);
                // NOTE: it might happen that v_parent_rt already has an edge to v_child_rt in the cDAG, in which case we simply remove v from the cDAG
                const NodeDesc v_parent_rt_in_cDAG = N_to_comp_DAG.at(v_parent_rt);
                for(const NodeDesc x: comp_DAG.children(v_parent_rt_in_cDAG))
                  comp_DAG.remove_edge(v_child_in_cDAG, x);
                // finally, contract v_child_in_cDAG up
                comp_DAG.contract_up(v_child_in_cDAG);
                N_to_comp_DAG.erase(v_child_in_cDAG_iter);
              }
              // step 2: set v's component root to v_parent's
              std::cout << "\tREACT: updating component roots...\n";
              set_comp_root(v_child, v_parent_rt);
              set_comp_root(v, v_parent_rt);
              
              std::cout << "cDAG after component update:\n";
              comp_DAG.print_subtree_with_data();
            } else {} // if v's parent is a reticulation, we're fine
          } else {
            // NOTE: if v's child is a reti, we may just act as if the edge removal was between u and that child (slightly abusing notation)
            react_to_edge_deletion(u, v_child);
          }
        }
      }
    }



  };
    



}
