
#pragma once

#include "optional_tuple.hpp"
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
    // N_to_compDAG translates nodes of N into nodes of the component DAG
    NodeTranslation N_to_compDAG;
    ComponentDAG comp_DAG;
   

    TreeComponentData& component_data_of(const NodeDesc u) {
      if constexpr (use_internal_node_data) return node_of<Network>(u).data(); else return Parent::template get<0>()[u];
    }

    const TreeComponentData& component_data_of(const NodeDesc u) const {
      if constexpr (use_internal_node_data) return node_of<Network>(u).data(); else return Parent::template get<0>()[u];
    }

  protected:
    void compute_comp_DAG() {
      DEBUG3(std::cout << "constructing component-DAG...\n");

      // create an edge-emplacer for the result DAG; we won't have to track roots since they will be the same as the roots of N
      // NOTE: the lambda tells the emplacer that the node data of the node corresponding to u will be u itself
      //       (thus, each new node has, as its data, the NodeDesc of its corresponding original node)
      std::cout << "making emplacer...\n";
      auto emplacer = EdgeEmplacers<false>::make_emplacer(comp_DAG, N_to_compDAG, [](const NodeDesc u){ return u;});
      
      NodeVec non_trivial_roots;
      NodeVec trivial_roots;

      emplacer.create_copy_of(N.root());
      emplacer.mark_root(N.root());
      std::cout << "setting comp data of "<<N.root()<<"\n";
      component_data_of(N.root()).comp_root = N.root();
      std::cout << "computing component roots\n";
      compute_component_roots(trivial_roots, non_trivial_roots);
      // construct the component DAG from the non-trivial roots
      std::cout << "1st pass over component roots\n";
      std::cout << "emplacer translation @"<< &(emplacer.helper.old_to_new)<<" our translation @"<<&N_to_compDAG<<"\n";
      compute_edges<true>(non_trivial_roots, emplacer);
      // construct visibility also from the trivial roots (the leaves), but don't add their edges to the component DAG
      std::cout << "2nd pass over component roots\n";
      compute_edges<false>(trivial_roots, emplacer);
      // the root has not yet stored in the list of non-trivial roots, so add it here
      //non_trivial_roots.emplace_back(N.root());
    }

    // compute component roots and return list of leaves encountered
    void compute_component_roots(NodeVec& trivial_roots, NodeVec& non_trivial_roots) {
      for(const NodeDesc u: N.nodes_preorder()) {
        auto& u_node = node_of<Network>(u);
        if(!u_node.is_leaf()) {
          if(u_node.is_tree_node()) {
            auto& u_data = component_data_of(u);
            const auto& pu = u_node.parents();
            if(pu.empty() || Network::is_reti(front(pu))){ // if the parent is a reticulation, register new component root
              u_data.comp_root = u;
              non_trivial_roots.push_back(u);
            } else u_data.comp_root = component_data_of(front(pu)).comp_root; // if the parent is not a reticulation, copy their component root
          }
        } else trivial_roots.push_back(u); // NOTE: leaves don't get a component-root
      }
    }


    // update visibility of reticulation chains and return a list of components seen above
    //NOTE: also returns the tree component root that sees r, or NoNode if there is no such root
    template<bool save_components>
    NodeDesc update_reticulations(const NodeDesc r, NodeSet& components_above) {
      // we use "r" to denote "uninitialized" and "NoNode" to denote "more than one component sees r"
      //NOTE: we cannot simply use "components_above.size() == 1" since previous branches might have already filled that
      NodeDesc rt = r;
      std::cout << "updating 'reticulation' "<<r<<" with parents "<<N.parents(r)<<'\n';
      for(const NodeDesc pr: N.parents(r)){
        NodeDesc next_rt;
        if(!N.is_reti(pr)){
          next_rt = component_data_of(pr).comp_root;
          if constexpr (save_components)
            if(next_rt != NoNode) append(components_above, next_rt);
        } else next_rt = update_reticulations<save_components>(pr, components_above);
        if(rt == r) rt = next_rt;
        if(rt != next_rt) rt = NoNode;
      }
      if(rt == r) return NoNode; // rt == r only if r has no parents, in which case, there is no component root above
      if(N.out_degree(r) <= 1) component_data_of(r).comp_root = rt;
      return rt;
    }

    // build the network of component roots by following the reticulations above each root
    template<bool make_edges>
    void compute_edges(const NodeVec& roots, auto& edge_emplacer) {
      for(const NodeDesc u: roots){
        assert(!N.is_reti(u));
        NodeSet components_above;
        const NodeDesc rt = update_reticulations<make_edges>(u, components_above);
        if((N.out_degree(u) == 0) && (rt != NoNode)) {
          std::cout << "setting visible leaf of "<<rt<<" to the leaf "<<u<<"\n";
          component_data_of(rt).visible_leaf = u;
        }
        if constexpr (make_edges)
          for(const NodeDesc v: components_above)
            edge_emplacer.emplace_edge(v, u);
      }
    }



  public:

    // construction
    TreeComponentInfos(Network& _N):
      N(_N)
    {
      if(!N.empty())
        compute_comp_DAG();
    }

    // we want to be able to give a new Network-reference when copy-constructing
    TreeComponentInfos(const TreeComponentInfos& tc, Network& _N):
      N(_N), N_to_compDAG(), comp_DAG(tc.comp_DAG)
    {}
    TreeComponentInfos(TreeComponentInfos&& tc, Network& _N):
      N(_N), N_to_compDAG(std::move(tc.N_to_compDAG)), comp_DAG(std::move(tc.comp_DAG))
    {}


    // get the highest component-root that is visible on u
    NodeDesc get_highest_visible_comp_root(const NodeDesc u) {
      // get highest initial component root reachable without reticulation
      std::cout << "getting highest visible from "<<u<<" ("<<component_data_of(u)<<")\n";
      NodeDesc rt = component_data_of(u).comp_root;
      while(rt != NoNode) {
        std::cout << "rt = "<<rt<<" ("<<component_data_of(rt)<<")\n";
        if(N.in_degree(rt) == 1){
          const NodeDesc p_rt = N.parent(rt);
          switch(N.in_degree(p_rt)){
            case 0:
              return p_rt;
            case 1:
              rt = component_data_of(p_rt).comp_root;
              assert(rt != NoNode); // the parent is a tree-node, so it should have a component-root
              return rt;
            default:
              return rt;
          }
        } else {
          assert(N.in_degree(rt) == 0);
          return rt;
        }
      }
      return rt;
    }



    // recomputes the component root of a node y from the component roots of its parents
    // return whether the root of y changed
    bool inherit_root(const NodeDesc y, const bool spread_info = false) {
      NodeDesc& y_root = component_data_of(y).comp_root;
      if((y_root != y) || (N.in_degree(y) > 1)){ // update only for non-component roots!
        std::cout << "computing new component root of "<<y<<" (spread = "<<spread_info<<")\n";
        NodeDesc rt;
        switch(N.in_degree(y)){
          case 0: 
            rt = y;
            break;
          case 1:
            {
              const NodeDesc x = N.parent(y);
              std::cout << "inheriting comp-root from "<<x<<": "<<component_data_of(x)<<"\n";
              // if we have in-degree 1, then we take the component-root of our parent x if x is a tree node or we are a leaf
              rt = ((N.in_degree(x) > 1) && (N.out_degree(y) > 1)) ? y : component_data_of(x).comp_root;
            }
            break;
          default:
            rt = comp_root_consensus(N.parents(y));
        }
        std::cout << "updating comp-root of "<<y<<" with "<<rt<<'\n';
        if(y_root != rt) {
          if(y_root == y) {
            std::cout << "as "<<y<<" was a comp-root, we will suppress it in the component DAG\n";
            comp_DAG.suppress_node(y);
          }
          y_root = rt;
          std::cout << "updated comp-info of "<<y<<" to "<< component_data_of(y)<<"\n";
          if(N.out_degree(y) > 0) {
            if(spread_info) for(const NodeDesc z: N.children(y)) inherit_root(z);
          } else if(rt != NoNode) {
            std::cout << "setting visible leaf of "<<rt<<" to "<<y<<"\n";
            component_data_of(rt).visible_leaf = y; // further, if we are a leaf, then mark the component root(s) visible from us
          }
          return true;
        } else return false;
      } else return false;
    }

    // if all nodes in the container c have the same component root r then return r, otherwise return NoNode
    template<NodeIterableType Container>
    NodeDesc comp_root_consensus(const Container& c) {
      if(!c.empty()) {
        auto c_iter = c.begin();
        const NodeDesc result = get_highest_visible_comp_root(*c_iter);
        while(++c_iter != c.end())
          if(get_highest_visible_comp_root(*c_iter) != result) return NoNode;
        return result;
      } else return NoNode;
    }
  };
    



}
