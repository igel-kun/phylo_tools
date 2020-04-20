
#pragma once

#include "set_interface.hpp"


namespace PT{

  class Extension: public NodeVec
  {
    using Parent = NodeVec;
  public:
    // import constructors
    using Parent::Parent;

    void get_inverse(std::unordered_map<Node, uint32_t>& inverse) const
    { for(unsigned i = 0; i < this->size(); ++i) inverse.emplace(this->at(i), i); }

    // return if the extension is valid for a given network
    template<class Network>
    bool is_valid_for(const Network& N)
    {
      // construct inverse of the extension, mapping each node to its position
      std::unordered_map<Node, uint32_t> inverse;
      get_inverse(inverse);

      // check if all arcs in the network go backwards in the extension
      for(const auto& uv: N.get_edges())
        if(inverse.at(uv.head()) > inverse.at(uv.tail())) return false;

      return true;
    }

    // return the scanwidth of the extension in the network N
    template<class Network>
    uint32_t scanwidth(const Network& N) const
    {
      if(N.empty())
        return 0;
      else
        return *std::max_element(seconds(sw_map(N)));
    }

    // add a node u and update sw using the set forest representing the current weak components in the extension
    // return the scanwidth of the given node
    template<class Network, class _Container>
    uint32_t update_sw(const Network& N,
                   const Node u,
                   std::DisjointSetForest<typename Network::Edge>& weak_components,
                   _Container& out) const
    {
      using EdgeRef = typename Network::EdgeRef;
      try{
        DEBUG5(std::cout << "adding "<<u<<" to "<<weak_components<< std::endl);
        if(!N.is_leaf(u)) {
          // step 1: merge all in-edges to the same weak component
          const EdgeRef uv = std::front(N.out_edges(u));
          for(const auto& wu: N.in_edges(u)) weak_components.add_item_to_set_of(wu, uv);
          // step 2: merge all weak components of out-edges of u & remove all out-edges from the component
          for(const auto& uw: N.out_edges(u)) {
            weak_components.merge_sets_of(uv, uw);
            weak_components.remove_item(uw);
          }
          // step 3: record the size of the remaining component
          return append(out, u, (typename _Container::mapped_type)(weak_components.size_of_set_of(uv))).first->second;
        } else return append(out, u, (typename _Container::mapped_type)(weak_components.add_new_set(N.in_edges(u)).size())).first->second;
      } catch(std::out_of_range& e){
        throw(std::logic_error("trying to compute scanwidth of a non-extension"));
      }
    }

    // get mapping of nodes to their scanwidth in the extension
    template<class Network, class _Container>
    void sw_map(const Network& N, _Container& out) const
    {
      DEBUG3(std::cout << "computing sw-map of extension "<<*this<<std::endl);
      std::DisjointSetForest<typename Network::Edge> weak_components;
      for(const Node u: *this) update_sw(N, u, weak_components, out);
    }

    template<class Network, class _Container = std::unordered_map<Node, uint32_t>>
    _Container sw_map(const Network& N) const
    {
      _Container result;
      sw_map(N, result);
      return result;
    }

  };

}// namespace
namespace std{
  template<> struct is_vector<PT::Extension>: public true_type {};
}
