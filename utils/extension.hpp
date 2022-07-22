
#pragma once

#include <unordered_map>
#include "set_interface.hpp"


namespace PT{

  struct partial_extension_tag {};

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
    template<StrictPhylogenyType Net>
    sw_t scanwidth() const {
      return this->empty() ? 0 : *mstd::max_element(mstd::seconds(sw_map<Net>()));
    }

    // add a node u and update sw using the set forest representing the current weak components in the extension
    // return the scanwidth of the given node
    template<PhylogenyType Net,
             NodeMapType Out = NodeMap<sw_t>,
             class NetworkDegrees = decltype([](const NodeDesc x)->std::pair<size_t,size_t>{return Net::degrees(x);})>
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
    
      void update_sw(const NodeDesc u) {
        DEBUG5(std::cout << "adding "<<u<<" to "<<weak_components<< std::endl);
        const auto& u_node = node_of<Net>(u);
        auto [indeg, outdeg] = network_degrees(u);
        auto& sw_u = indeg;
        weak_components.add_new_set(u, u);
        // step 1: merge all weak components of chilldren of u
        try{
          for(const auto& v: u_node.children()) {
            if(weak_components.in_different_sets(u, v)) {
              // if v is in a different weak component than u, then merge the components and increase sw(u) by sw(v)
              const auto& v_set = weak_components.set_of(v);
              const NodeDesc most_recent_in_component = v_set.payload;
              sw_u += out.at(most_recent_in_component);
              weak_components.merge_sets_keep_order(u, v);
            }
          }
          sw_u -= outdeg; // discount the edges u-->v from the scanwidth of u
          append(out, u, std::move(sw_u));
        } catch(std::out_of_range& e){
          throw(std::logic_error("trying to compute scanwidth of a non-extension"));
        }
      }
      void update_all(const Extension& ex) {
        for(const NodeDesc u: ex) update_sw(u);
      }
    };


    // compute the scanwidth of all nodes in a given extension
    // NOTE: NetworkDegree is used to return the degrees (pair of indegree and outdegree) of a node in the network
    // NOTE: this can be used to construct the actual edge- or node- set corresponding to the scanwidth entry by passing a suitable function network_degrees
    template<StrictPhylogenyType Net, class NetDeg, class Out>
    void sw_map_meta(NetDeg&& network_degrees, Out& out) const {
      DEBUG3(std::cout << "computing sw-map of extension "<<*this<<std::endl);
      DynamicScanwidth<Net, Out&, NetDeg>{out, std::forward<NetDeg>(network_degrees)}.update_all(*this);
    }



    template<StrictPhylogenyType Network, class Out>
    void sw_map(Out&& out) const {
      return sw_map_meta<Network>([&](const NodeDesc u){ return Network::degrees(u); }, std::forward<Out>(out));
    }
    template<StrictPhylogenyType Network, class Out = NodeMap<sw_t>>
    Out sw_map() const {
      Out result;
      sw_map<Network>(result);
      return result;
    }


    template<StrictPhylogenyType Network, NodeMapType Out>
    void sw_nodes_map(Out&& out) const {
      using Nodes = mstd::mapped_type_of_t<Out>;
      static_assert(std::is_same_v<std::remove_cvref_t<mstd::value_type_of_t<Nodes>>, NodeDesc>);
      using NodesAndNode = std::pair<Nodes, NodeDesc>;
      return sw_map_meta<Network>([&](const NodeDesc u){
          const auto& u_parents = Network::parents(u);
          return NodesAndNode(std::piecewise_construct, std::tuple{u_parents.begin(), u_parents.end()}, std::tuple{u});
        },
        std::forward<Out>(out)
      );
    }
    template<StrictPhylogenyType Network, NodeMapType Out = NodeMap<NodeSet>>
    Out sw_nodes_map() const {
      Out result;
      sw_nodes_map<Network>(result);
      return result;
    }

    template<StrictPhylogenyType Network, NodeMapType Out>
    void sw_edges_map(Out&& out) const {
      using Edges = mstd::mapped_type_of_t<Out>;
      static_assert(EdgeType<mstd::value_type_of_t<Edges>>);
      using EdgesPair = std::pair<Edges, Edges>; 
      return sw_map_meta<Network>([&](const NodeDesc u) {
          const auto& u_in_edges = Network::in_edges(u);
          const auto& u_out_edges = Network::out_edges(u);
          return EdgesPair(u_in_edges.template to_container<Edges>(), u_out_edges.template to_container<Edges>());
        },
        std::forward<Out>(out)
      );
    }
    template<StrictPhylogenyType Network, NodeMapType Out = NodeMap<typename Network::EdgeSet>>
    Out sw_edges_map() const {
      Out result;
      sw_edges_map<Network>(result);
      return result;
    }


  };





}// namespace
