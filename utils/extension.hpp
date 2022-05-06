
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
      return this->empty() ? 0 : *std::max_element(seconds(sw_map<Net>()));
    }

    // add a node u and update sw using the set forest representing the current weak components in the extension
    // return the scanwidth of the given node
    template<PhylogenyType Net, std::ContainerType _Container>
    sw_t update_sw(const NodeDesc u,
                   std::DisjointSetForest<typename Net::Edge>& weak_components,
                   _Container& out) const
    {
      sw_t result;
      try{
        DEBUG5(std::cout << "adding "<<u<<" to "<<weak_components<< std::endl);
        const auto& u_node = node_of<Net>(u);
        if(!u_node.is_leaf()) {
          // step 1: merge all in-edges to the same weak component
          const auto& uv = std::front(u_node.out_edges());
          for(const auto& wu: u_node.in_edges())
            weak_components.add_item_to_set_of(uv, wu);
          // step 2: merge all weak components of out-edges of u & remove all out-edges from the component
          for(const auto& uw: u_node.out_edges()) {
            weak_components.merge_sets_of(uv, uw);
            weak_components.shrink(uw);
          }
          // step 3: record the size of the remaining component
          result = weak_components.set_of(uv).size();
        } else result = weak_components.add_new_set(u_node.in_edges()).size();
      } catch(std::out_of_range& e){
        throw(std::logic_error("trying to compute scanwidth of a non-extension"));
      }
      append(out, u, result);
      return result;
    }

    // get mapping of nodes to their scanwidth in the extension
    template<PhylogenyType Net, std::ContainerType _Container>
    void sw_map(_Container& out) const {
      DEBUG3(std::cout << "computing sw-map of extension "<<*this<<std::endl);
      std::DisjointSetForest<typename Net::Edge> weak_components;
      for(const NodeDesc u: *this) update_sw<Net>(u, weak_components, out);
    }

    template<PhylogenyType Net, NodeMapType _Container = std::unordered_map<NodeDesc, sw_t>>
    _Container sw_map() const {
      _Container result;
      sw_map<Net>(result);
      return result;
    }

  };

}// namespace
