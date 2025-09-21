#pragma once

#include "types.hpp"

// The DynamicScanwidth class administrates a node ordering for a network and its corresponding
// weak components and scanwidth values
// One can add new nodes to the ordering and the weak components as well as the scanwidths will be updated

namespace PT {

  // add a node u and update sw using the set forest representing the current weak components in the extension
  // return the scanwidth of the given node
  template<PhylogenyType Net,
           NodeMapType Output = NodeMap<sw_t>,
           class NetworkDegrees = DefaultDegrees<Net>>
      requires (!mstd::ContainerType<mstd::mapped_type_of_t<Output>> || mstd::SetType<mstd::mapped_type_of_t<Output>>)
  struct DynamicScanwidth {
  protected:
    Output out;
    // the union-find structure knows what nodes are in the same weak-component "below" the current node
    // it also knows for each weak component which of their nodes is the most recent one
    mstd::DisjointSetForest<NodeDesc, NodeDesc> weak_components;
    [[no_unique_address]] NetworkDegrees network_degrees;

    STAT(size_t sw_raising = 0; size_t sw_shrinking = 0);
    static constexpr bool sw_accumulatable = std::is_convertible_v<decltype(network_degrees(NoNode).first), sw_t>;

  public:
    using SWReturnType = typename std::invoke_result_t<NetworkDegrees, const NodeDesc>::first_type;
    static constexpr bool return_simple_int = std::is_arithmetic_v<SWReturnType>;

    DynamicScanwidth()
      requires (!std::is_reference_v<Output> && !std::is_reference_v<NetworkDegrees>)
    = default;

    template<class NDInit>
    DynamicScanwidth(Output& _out, NDInit&& net_deg_init): out{_out}, network_degrees{std::forward<NDInit>(net_deg_init)} {}
    DynamicScanwidth(Output& _out): out{_out} {}

    const Output& get_sw_map() const { return out; }

    // add a new node u to the scanwidth calculation and return its scanwidth
    template<class CallBack = mstd::IgnoreFunction<>>
    auto update_sw(const NodeDesc u, CallBack&& save_highest_child_of = CallBack()) {
      STAT(size_t child_sw_max = 0;);
      DEBUG5(std::cout << "adding "<<u<<" to "<<weak_components<< std::endl);
      const auto& u_node = node_of<Net>(u);
      auto [sw_u, outdeg] = network_degrees(u);
      DEBUG5(std::cout << "received modified degrees of "<<u<<": "<<sw_u<< " & "<<outdeg<<"\n");
      weak_components.add_new_set(u, u);
      // step 1: merge all weak components of chilldren of u
      try{
        DEBUG5(std::cout << "working children "<<u_node.children()<<" of "<<u<<"\n");
        for(const auto& v: u_node.children()) {
          STAT(if constexpr (sw_accumulatable) {child_sw_max = std::max(child_sw_max, out.at(v));} );

          if(weak_components.in_different_sets(u, NodeDesc{v})) {
            // if v is in a different weak component than u, then merge the components and increase sw(u) by sw(v)
            const auto& v_set = weak_components.set_of(v);
            const NodeDesc most_recent_in_component = v_set.payload;
            // most_recent_in_component is a highest leaf of u
            // this information might be valuable for some users, so we give the opportunity to save it
            append(save_highest_child_of, u, most_recent_in_component);
            mstd::append(sw_u, out.at(most_recent_in_component));
            weak_components.merge_sets_keep_order(u, v);
          }


        }
        STAT(if constexpr (sw_accumulatable) {
            if(child_sw_max != 0) {if(sw_u < child_sw_max) {++sw_shrinking;} else if(sw_u > child_sw_max) {++sw_raising;}}});
        mstd::erase(sw_u, outdeg); // discount the edges u-->v from the scanwidth of u
        mstd::append(out, u, sw_u);
      } catch(std::out_of_range& e){
        throw(std::logic_error("trying to compute scanwidth of a non-extension"));
      }
      return sw_u;
    }



    
    template<class CallBack = mstd::IgnoreFunction<>, NodeContainerType Nodes>
    sw_t update_all(const Nodes& nodes, CallBack&& save_highest_child_of = CallBack()) {
      sw_t result = 0;
      for(const NodeDesc u: nodes) {
        const auto tmp = update_sw(u, std::forward<CallBack>(save_highest_child_of));
        if constexpr (std::is_arithmetic_v<decltype(tmp)>) {
          result = std::max(tmp, result);
        } else result = std::max(tmp.size(), result);
      }
      STAT(std::cout << "sw-raising nodes: "<<sw_raising<<" sw-shrinking nodes: "<<sw_shrinking<<" sw-neutral: " << nodes.size() - sw_raising - sw_shrinking<<"\t "<< (100 * sw_raising) / (sw_raising + sw_shrinking) <<"\% raising\n");
      return result;
    }

    void clear() { weak_components.clear(); out.clear(); }

    sw_t get_scanwidth() const {
      if(!out.empty()) {
        return std::ranges::max(mstd::seconds(out));
      } else return 0;
    }

  };


}
