
#pragma once

#include <unordered_map>
#include "set_interface.hpp"


namespace PT{

  struct partial_extension_tag {};


  template<StrictPhylogenyType Net>
  struct DefaultDegrees { Degrees operator()(const NodeDesc u) const { return Net::degrees(u); } };


  class Extension: public NodeVec {
    using Parent = NodeVec;
  public:
    // import constructors
    using Parent::Parent;

    void compute_inverse(std::unordered_map<NodeDesc, sw_t>& inverse) const {
       for(size_t i = 0; i < this->size(); ++i) inverse.emplace(this->at(i), i);
    }
    auto get_inverse() const {
      std::unordered_map<NodeDesc, sw_t> result;
      compute_inverse(result);
      return result;
    }

    // return if the extension is valid for a given network
    template<StrictPhylogenyType Net>
    bool is_valid_for(const Net& N) const {
      // construct inverse of the extension, mapping each node to its position
      const std::unordered_map<NodeDesc, sw_t> inverse = get_inverse();

      // check if all arcs in the network go backwards in the extension
      for(const auto& uv: N.get_edges())
        if(inverse.at(uv.head()) > inverse.at(uv.tail())) return false;

      return true;
    }

    // return the scanwidth of the extension in the network N
    template<StrictPhylogenyType Net, class NetDeg = DefaultDegrees<Net>>
    sw_t scanwidth(NetDeg&& degrees = NetDeg()) const {
      if(!this->empty()) {
        return std::ranges::max(mstd::seconds(get_sw_map<Net>(std::forward<NetDeg>(degrees))));
      } else return 0;
    }

    // add a node u and update sw using the set forest representing the current weak components in the extension
    // return the scanwidth of the given node
    template<PhylogenyType Net,
             NodeMapType Out = NodeMap<sw_t>,
             class NetworkDegrees = DefaultDegrees<Net>>
        requires (!mstd::ContainerType<mstd::mapped_type_of_t<Out>> || mstd::SetType<mstd::mapped_type_of_t<Out>>)
    struct DynamicScanwidth {
      Out out;
      // the union-find structure knows what nodes are in the same weak-component "below" the current node
      // it also knows for each weak component which of their nodes is the most recent one
      mstd::DisjointSetForest<NodeDesc, NodeDesc> weak_components;
      [[no_unique_address]] NetworkDegrees network_degrees;

      DynamicScanwidth()
        requires (!std::is_reference_v<Out> && !std::is_reference_v<NetworkDegrees>)
      = default;

      template<class NDInit>
      DynamicScanwidth(Out& _out, NDInit&& net_deg_init): out{_out}, network_degrees{std::forward<NDInit>(net_deg_init)} {}
      DynamicScanwidth(Out& _out): out{_out} {}
    
      template<class CallBack = mstd::IgnoreFunction<void>>
      void update_sw(const NodeDesc u, CallBack&& save_highest_child_of = CallBack()) {
        DEBUG5(std::cout << "adding "<<u<<" to "<<weak_components<< std::endl);
        const auto& u_node = node_of<Net>(u);
        auto [sw_u, outdeg] = network_degrees(u);
        DEBUG5(std::cout << "received modified degrees of "<<u<<": "<<sw_u<< " & "<<outdeg<<"\n");
        weak_components.add_new_set(u, u);
        // step 1: merge all weak components of chilldren of u
        try{
          DEBUG5(std::cout << "working children "<<u_node.children()<<" of "<<u<<"\n");
          for(const auto& v: u_node.children()) {
            if(weak_components.in_different_sets(u, v)) {
              // if v is in a different weak component than u, then merge the components and increase sw(u) by sw(v)
              const auto& v_set = weak_components.set_of(v);
              const NodeDesc most_recent_in_component = v_set.payload;
              // most_recent_in_component is a highest leaf of u
              // this information might be valuable for some users, so we give the opportunity to save it
              append(save_highest_child_of, u, most_recent_in_component);
              sw_u += out.at(most_recent_in_component);
              weak_components.merge_sets_keep_order(u, v);
            }
          }
          sw_u -= outdeg; // discount the edges u-->v from the scanwidth of u
          append(out, u, sw_u);
        } catch(std::out_of_range& e){
          throw(std::logic_error("trying to compute scanwidth of a non-extension"));
        }
      }
      template<class CallBack = mstd::IgnoreFunction<void>>
      void update_all(const Extension& ex, CallBack&& save_highest_child_of = CallBack()) {
        for(const NodeDesc u: ex) update_sw(u, std::forward<CallBack>(save_highest_child_of));
      }
    };


    // compute the scanwidth of all nodes in a given extension
    // NOTE: NetworkDegree is used to return the degrees (pair of indegree and outdegree) of a node in the network
    // NOTE: this can be used to construct the actual edge- or node- set corresponding to the scanwidth entry by passing a suitable function network_degrees
    template<StrictPhylogenyType Net, class NetDeg, class Out, class... Args>
    void sw_map_meta(NetDeg&& network_degrees, Out& out, Args&&... args) const {
      DEBUG3(std::cout << "computing sw-map of extension "<<*this<<std::endl);
      DEBUG3(std::cout << "degree-extracter is "<<mstd::type_name<NetDeg>()<<"\n");
      DynamicScanwidth<Net, Out&, NetDeg>{out, std::forward<NetDeg>(network_degrees)}.update_all(*this, std::forward<Args>(args)...);
    }



    template<StrictPhylogenyType Network, class NetDeg, class Callback, class Out>
    void sw_map(Out&& out, NetDeg&& degrees, Callback&& save_highest_child) const {
      return sw_map_meta<Network>(std::forward<NetDeg>(degrees), std::forward<Out>(out), std::forward<Callback>(save_highest_child));
    }
    template<StrictPhylogenyType Network, class Out, class NetDeg = DefaultDegrees<Network>>
      requires (std::invocable<NetDeg, NodeDesc> && !std::is_void_v<std::invoke_result<NetDeg, NodeDesc>>)
    void sw_map(Out&& out, NetDeg&& degrees = NetDeg()) const {
      return sw_map_meta<Network>(std::forward<NetDeg>(degrees), std::forward<Out>(out));
    }
    template<StrictPhylogenyType Network, class Out, class Callback = DefaultDegrees<Network>>
      requires (!std::invocable<Callback, NodeDesc> || std::is_void_v<std::invoke_result<Callback, NodeDesc>>)
    void sw_map(Out&& out, Callback&& save_highest_child) const {
      return sw_map_meta<Network>(DefaultDegrees<Network>(), std::forward<Out>(out), std::forward<Callback>(save_highest_child));
    }


    template<StrictPhylogenyType Network, class Out = NodeMap<sw_t>, class... Args>
    Out get_sw_map(Args&&... args) const {
      Out result;
      sw_map<Network>(result, std::forward<Args>(args)...);
      return result;
    }


    template<StrictPhylogenyType Network, NodeMapType Out, class... Args>
    void sw_nodes_map(Out&& out, Args&&... args) const {
      using Nodes = mstd::mapped_type_of_t<Out>;
      static_assert(std::is_same_v<std::remove_cvref_t<mstd::value_type_of_t<Nodes>>, NodeDesc>);
      using NodesAndNode = std::pair<Nodes, NodeDesc>;
      return sw_map_meta<Network>([&](const NodeDesc u){
          const auto& u_parents = Network::parents(u);
          return NodesAndNode(std::piecewise_construct, std::tuple{u_parents.begin(), u_parents.end()}, std::tuple{u});
        },
        std::forward<Out>(out),
        std::forward<Args>(args)...
      );
    }
    template<StrictPhylogenyType Network, NodeMapType Out = NodeMap<NodeSet>, class... Args>
    Out get_sw_nodes_map(Args&&... args) const {
      Out result;
      sw_nodes_map<Network>(result, std::forward<Args>(args)...);
      return result;
    }

    template<StrictPhylogenyType Network, NodeMapType Out, class... Args>
    void sw_edges_map(Out&& out, Args&&... args) const {
      using Edges = mstd::mapped_type_of_t<Out>;
      static_assert(EdgeType<mstd::value_type_of_t<Edges>>);
      using EdgesPair = std::pair<Edges, Edges>; 
      return sw_map_meta<Network>([&](const NodeDesc u) {
          const auto& u_in_edges = Network::in_edges(u);
          const auto& u_out_edges = Network::out_edges(u);
          return EdgesPair(u_in_edges.template to_container<Edges>(), u_out_edges.template to_container<Edges>());
        },
        std::forward<Out>(out),
        std::forward<Args>(args)...
      );
    }
    template<StrictPhylogenyType Network, NodeMapType Out = NodeMap<typename Network::EdgeSet>, class... Args>
    Out get_sw_edges_map(Args&&... args) const {
      Out result;
      sw_edges_map<Network>(result, std::forward<Args>(args)...);
      return result;
    }


  };


  // given an extension E and a network N, compute an arbitrary extension respecting both N and E
  // NOTE: this can be used to extend partial extensions to the node-set of a network and/or remove nodes from E that do not occur in N
  // NOTE: set extend_only to leave nodes in the extension that do not occur in N
  template<bool extend_only = false, PhylogenyType Network, NodeSetType Nodes>
  Extension apply_to_network(const Extension& ext, const Network& N, const Nodes& nodes_of_N) {
    Extension result;
    std::cout << "extending "<<ext<<" to nodes "<<nodes_of_N<<" of network\n"<<ExtendedDisplay(N)<<"\n";
    if(!ext.empty()) {
      // we will continue to use a single NodeTraversal, and keep adding to its seen_set
      NodeTraversal<postorder, Network, void, NodeSet> traversal(N);
      for(const NodeDesc u: ext) {
        if(test(nodes_of_N, u)) {
          assert(!test(traversal.seen_nodes(), u)); // if ext is an extension, the postorder traversal shouldn't have seen u before
          // add to the extension all nodes between u and the seen nodes 
          traversal.root = u;
          append(result, traversal);
          std::cout << "appended traversal from "<<u<<" - result now: "<<result<<"\n";
        } else // if u is not a node of N
          if constexpr (extend_only)
            append(result, u);
      }
    } else append(result, N.nodes_postorder());
#warning "TODO: cannot use std::ranges::copy(N.nodes_postorder(), std::back_inserter(result)) here because TraversalTraits may contain a _SeenSet in a form of a reference! In this case, TraversalTraits (and, thus, DFSIterators) are not assignable, but assignable iterators are required by the ranges library"
    return result;
  }
  template<bool extend_only = false, PhylogenyType Network>
  Extension apply_to_network(const Extension& ext, const Network& N) {
    return apply_to_network(ext, N, N.nodes().template to_container<NodeSet>());
  }



}// namespace
