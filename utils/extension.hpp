
#pragma once

#include <unordered_map>
#include "set_interface.hpp"


namespace PT{

  class Extension: public NodeVec {
    using Parent = NodeVec;
  public:
    // import constructors
    using Parent::Parent;

    void get_inverse(std::unordered_map<NodeDesc, sw_t>& inverse) const {
       for(unsigned i = 0; i < this->size(); ++i) inverse.emplace(this->at(i), i);
    }

    // return if the extension is valid for a given network
    template<PhylogenyType Net>
    bool is_valid_for(const Net& N) {
      // construct inverse of the extension, mapping each node to its position
      std::unordered_map<NodeDesc, sw_t> inverse;
      get_inverse(inverse);

      // check if all arcs in the network go backwards in the extension
      for(const auto& uv: N.get_edges())
        if(inverse.at(uv.head()) > inverse.at(uv.tail())) return false;

      return true;
    }

    // return the scanwidth of the extension in the network N
    template<PhylogenyType Net>
    sw_t scanwidth() const {
      return this->empty() ? 0 : *mstd::max_element(mstd::seconds(sw_map<Net>()));
    }

    // add a node u and update sw using the set forest representing the current weak components in the extension
    // return the scanwidth of the given node
    template<PhylogenyType Net>
    sw_t update_sw(const NodeDesc u, mstd::DisjointSetForest<NodeDesc, sw_t>& weak_components) const {
      try{
        DEBUG5(std::cout << "adding "<<u<<" to "<<weak_components<< std::endl);
        const auto& u_node = node_of<Net>(u);
        auto& u_comp = weak_components.add_new_set(u, u_node.in_degree());
        auto& u_payload = u_comp.payload;
        // step 1: merge all weak components of chilldren of u
        for(const auto& v: u_node.children()) {
          if(weak_components.in_different_sets(u, v)) {
            // if v is in a different weak component than u, then merge the components and increases sw(u) by sw(v) minus 1 for the edge u-->v
            weak_components.merge_sets(u, v);
            u_payload += weak_components.at(v).payload - 1;
          } else u_payload--; // if u and v are already in the same component, then just discount the edge u-->v from the scanwidth of u
        }
        return u_payload;
      } catch(std::out_of_range& e){
        throw(std::logic_error("trying to compute scanwidth of a non-extension"));
      }
    }

    // get mapping of nodes to their scanwidth in the extension
    template<PhylogenyType Net, mstd::ContainerType _Container>
    void sw_map(_Container& out) const {
      DEBUG3(std::cout << "computing sw-map of extension "<<*this<<std::endl);
      mstd::DisjointSetForest<NodeDesc, sw_t> weak_components;
      for(const NodeDesc u: *this) mstd::append(out, u, update_sw<Net>(u, weak_components));
    }

    template<PhylogenyType Net, NodeMapType _Container = std::unordered_map<NodeDesc, sw_t>>
    _Container sw_map() const {
      _Container result;
      sw_map<Net>(result);
      return result;
    }

  };

}// namespace
