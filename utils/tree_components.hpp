
#pragma once

#include "optional_tuple.hpp"
#include "union_find.hpp"
#include "types.hpp"
#include "network.hpp"
#include "edge_emplacement.hpp"

namespace PT{

  //! get a list of component roots in preorder
  template<class Network, class NodeContainer = typename Network::NodeVec>
  NodeContainer get_tree_non_trivial_roots(const Network& N) {
    NodeContainer comp_roots;
    for(const NodeDesc u: N.nodes()){
      if(N.is_inner_tree_node(u)){
        const NodeDesc v = N.parent(u);
        if(N.is_reti(v)) append(comp_roots, v);
      }
    }
    return comp_roots;
  }


#warning "TODO: check: I think nodes cannot have a component-root and a visible-leaf at the same time --> save some memory!"
  struct TreeComponentData {
    NodeDesc comp_root = NoNode;
    NodeDesc visible_leaf = NoNode;

    void force_both(const NodeDesc x) { comp_root = visible_leaf = x; }
    void clear() { force_both(NoNode); }
    friend std::ostream& operator<<(std::ostream& os, const TreeComponentData& cd)
    { return os<<"{rt: "<< (cd.comp_root == NoNode ? std::string(".") : std::to_string(cd.comp_root))
                <<" vl: "<<(cd.visible_leaf == NoNode ? std::string(".") : std::to_string(cd.visible_leaf))<<"}"; }
  };

  template<class T>
  concept StrictHasTreeComponentData = (StrictPhylogenyType<T> && std::is_base_of_v<TreeComponentData, typename T::NodeData>);
  template<class T>
  concept HasTreeComponentData = StrictHasTreeComponentData<std::remove_reference_t<T>>;

  // each node in the component DAG will know its corresponding node in the Network
  using ComponentDAG = DefaultNetwork<NodeDesc>;

  // map nodes to their TreeComponentData
  //NOTE: if the nodes of Network have node data that derives from TreeComponentData, then we use this node data by default, otherwise we use a map
  template<StrictPhylogenyType Network, bool use_internal_node_data = HasTreeComponentData<Network>>
  class TreeComponentInfos: public std::optional_tuple<std::conditional_t<use_internal_node_data, void, NodeMap<TreeComponentData>>> {
    using Parent = std::optional_tuple<std::conditional_t<use_internal_node_data, void, NodeMap<TreeComponentData>>>;
    // TODO: deal with the case the Network has multiple roots! For now, we just disallow it
    static_assert(Network::has_unique_root);

    Network& N;

  public:
    // N_to_comp_DAG translates nodes of N into nodes of the component DAG
    NodeTranslation N_to_comp_DAG;
    ComponentDAG comp_DAG;

    // we want to be able to merge tree components; as such, we need to be able to 
    std::DisjointSetForest<NodeDesc> comp_root_equality;
   

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

  protected:
    TreeComponentData& component_data_of(const NodeDesc u) {
      if constexpr (use_internal_node_data) return node_of<Network>(u).data(); else return Parent::template get<0>()[u];
    }

    const TreeComponentData& component_data_of(const NodeDesc u) const {
      if constexpr (use_internal_node_data) return node_of<Network>(u).data(); else return Parent::template get<0>()[u];
    }
  public:

    const NodeDesc& visible_leaf_of(const NodeDesc u) const { return component_data_of(u).visible_leaf; }
    NodeDesc& visible_leaf_of(const NodeDesc u) { return component_data_of(u).visible_leaf; }

    // return the root of x's tree component
    NodeDesc comp_root_of(const NodeDesc x) {
      const NodeDesc rt = component_data_of(x).comp_root;
      if(rt == NoNode)
        return NoNode;
      else return comp_root_equality.set_of(rt).get_representative();
    }

  protected:

    // register the fact that the component of x is now rooted at rt
    void set_comp_root(const NodeDesc x, const NodeDesc rt) {
      std::cout << "updating comp root of "<<x<<" to "<<rt<<'\n';
      NodeDesc& old_rt = component_data_of(x).comp_root;
      if(old_rt != NoNode) {
        if(old_rt != rt) {
          comp_root_equality.merge_sets_keep_order(rt, old_rt);
          // if rt didn't have a visible leaf before, inherit it from old_rt
          NodeDesc& rt_vis_leaf = component_data_of(rt).visible_leaf;
          if(rt_vis_leaf == NoNode)
            rt_vis_leaf = component_data_of(old_rt).visible_leaf;
        }
      } else old_rt = rt;
    }

    // if all v's parents have the same component root, then set that root for v and return it, otherwise, return NoNode
    // NOTE: the callable argument is applied to the component roots of all node we encounter
    // NOTE: if quit_early is true, we'll return failure once we know there is no consensus
    // NOTE: if recursive is true, we will call ourselves recursively for all reticulation parents
    template<bool recursive = true, bool set_this_root = false, bool quit_early = !recursive, class Callback = std::IgnoreFunction<void>>
    NodeDesc component_root_consensus_among_parents(const NodeDesc v, Callback&& callback = Callback()) {
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
        set_comp_root(v, consensus);
      return consensus;
    }


    void compute_comp_DAG() {
      DEBUG3(std::cout << "constructing component-DAG...\n");

      // create an edge-emplacer for the result DAG; we won't have to track roots since they will be the same as the roots of N
      // NOTE: the lambda tells the emplacer that the node data of the node corresponding to u will be u itself
      //       (thus, each new node has, as its data, the NodeDesc of its corresponding original node)
      std::cout << "making emplacer...\n";
      auto emplacer = EdgeEmplacers<false>::make_emplacer(comp_DAG, N_to_comp_DAG, [](const NodeDesc u){ return u;});
      
      NodeVec non_trivial_roots;
      NodeVec trivial_roots;

      emplacer.create_copy_of(N.root());
      emplacer.mark_root(N.root());
      std::cout << "setting comp data of "<<N.root()<<"\n";
      component_data_of(N.root()).comp_root = N.root();
      comp_root_equality.add_new_set(N.root());
      std::cout << "computing component roots\n";
      compute_component_roots(trivial_roots, non_trivial_roots);
      // construct the component DAG from the non-trivial roots
      std::cout << "1st pass over component roots\n";
      std::cout << "emplacer translation @"<< &(emplacer.helper.old_to_new)<<" our translation @"<<&N_to_comp_DAG<<"\n";
      for(const NodeDesc rt: non_trivial_roots)
        compute_edges<true>(rt, emplacer);
      // construct visibility also from the trivial roots (the leaves), but don't add their edges to the component DAG
      std::cout << "2nd pass over component roots\n";
      for(const NodeDesc rt: trivial_roots)
        compute_edges<false>(rt, emplacer);
      // the root has not yet stored in the list of non-trivial roots, so add it here
      //non_trivial_roots.emplace_back(N.root());
    }

    // compute component roots (trivial (that is, leaves) and non-trivial)
    // NOTE: this sets the comp_root of all (non-leaf) tree nodes
    void compute_component_roots(NodeVec& trivial_roots, NodeVec& non_trivial_roots) {
      for(const NodeDesc u: N.nodes_preorder()) {
        auto& u_node = node_of<Network>(u);
        if(!u_node.is_reti()) {
          auto& u_data = component_data_of(u);
          if(u_node.is_leaf()) u_data.visible_leaf = u;
          const auto& pu = u_node.parents();
          // if the parent is a reticulation, register new component root
          if(pu.empty() || Network::is_reti(front(pu))){
            u_data.comp_root = u;
            comp_root_equality.add_new_set(u);
            if(u_node.is_leaf()) {
              trivial_roots.push_back(u);
            } else {
              non_trivial_roots.push_back(u);
            }
          } else {
            // if the parent is not a reticulation, copy their component root
            u_data.comp_root = component_data_of(front(pu)).comp_root;
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
      if((N.is_leaf(u)) && (rt != NoNode)) {
        std::cout << "setting visible leaf of "<<rt<<" to the leaf "<<u<<"\n";
        component_data_of(rt).visible_leaf = u;
      }
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
      
      if(rt_below_v != v) { // v is a reticulation
        std::cout << "\tREACT: "<< v <<" is a reticulation\n";
        assert(rt_below_v != NoNode);
        // step 0: check if u_rt is still above v_rt or not
        // NOTE: this updates the component root of v and all reticulation children of u on the way
        bool u_rt_above_rt_below_v = false;
        component_root_consensus_among_parents(rt_below_v, [&u_rt_above_rt_below_v, u_rt](const NodeDesc x){ u_rt_above_rt_below_v |= (x == u_rt); });
        // step 1: if u_rt is no longer above rt_below_v, then remove the edge from the comp_DAG
        if(!u_rt_above_rt_below_v) {
          const NodeDesc rt_below_v_in_cDAG = N_to_comp_DAG.at(rt_below_v);
          const NodeDesc u_rt_in_cDAG = N_to_comp_DAG.at(u_rt);
          comp_DAG.remove_edge(u_rt_in_cDAG, rt_below_v_in_cDAG);
          assert(comp_DAG.in_degree(rt_below_v_in_cDAG) > 0);
        }
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
            // if v's child is not a reticulation, it must be a component root (remember that v was a reticulation before removing uv)
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
              assert(v_child_in_cDAG_iter != N_to_comp_DAG.end());
              const NodeDesc v_child_in_cDAG = v_child_in_cDAG_iter->second;
              // NOTE: v_parent_rt should be the only predecessor of v_child in the component DAG
              assert(comp_DAG.in_degree(v_child_in_cDAG) == 1);
              comp_DAG.contract_up(v_child_in_cDAG);
              N_to_comp_DAG.erase(v_child_in_cDAG_iter);
              // step 2: set v's component root to v_parent's
              std::cout << "\tREACT: updating component roots...\n";
              set_comp_root(v_child, v_parent_rt);
              set_comp_root(v, v_parent_rt);
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
