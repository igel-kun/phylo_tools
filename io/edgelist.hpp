

#pragma once

#include <unordered_map>
#include "utils/types.hpp"
#include "utils/set_interface.hpp"

namespace PT{
  struct MalformedEdgeVec : public std::exception 
  {
    const char* what() const throw() {
      return "error reading edgelist";
    }
  };

  // ------ WRITE OUTPUT --------
  template<PhylogenyType _Network>
  void write_label(std::ostream& os, const NodeDesc x) {
    if constexpr (_Network::has_node_labels){
      const auto x_node = _Network::node_of(x);
      if(!x_node.label().empty()) os << '_' << x_node.label();
    }
  }

  template<PhylogenyType _Network>
  void write_edgelist(std::ostream& os, const _Network& N, const NodeDesc sub_root) {
    NodeMap<size_t> node_number;
    for(const auto uv: N.edges_below_preorder(sub_root)) {
      const NodeDesc u = uv.tail();
      const NodeDesc v = uv.head();
      const auto& u_num = node_number.emplace(u, node_number.size()).first->second;
      const auto& v_num = node_number.emplace(v, node_number.size()).first->second;
      os << u_num;
      write_label<_Network>(os, u);
      os << '\t' << v_num;
      write_label<_Network>(os, v);
      os << '\n';
    }
  }

  // compute the extended newick string for a network N
  template<PhylogenyType _Network>
  void write_edgelist(std::ostream& os, const _Network& N) { write_edgelist(os, N, N.root()); }




#warning TODO: teach it to parse weighted edges

  // ------ READ INPUT --------
  // read an edgelist
  template<class EdgeList, class LabelMap>
  class EdgeVecParser
  {
    std::istream& edgestream;
    EdgeList& edges;
    LabelMap& names;
    std::unordered_map<std::string, NodeDesc> name_to_node;

    EdgeVecParser();
  public:
    using Edge = typename EdgeList::value_type;

    EdgeVecParser(std::istream& _edgestream, EdgeList& _edges, LabelMap& _names):
      edgestream(_edgestream),
      edges(_edges),
      names(_names),
      name_to_node()
    {
      names.clear();
      edges.clear();
    }

    NodeDesc get_id(const std::string& name)
    {
      auto emp_res = mstd::append(name_to_node, name);
      if(emp_res.second){
        const NodeDesc id = NodeDesc(names.size());
        emp_res.first->second = id;
        mstd::append(names, id, name);
        return id;
      } else return emp_res.first->second;
    }

    //! read edges and return the number of nodes used by them
    size_t read_tree()
    {
      while(!edgestream.eof()){
        std::string name;
        
        edgestream >> name;
        const NodeDesc u = get_id(name);
        if(edgestream.bad() || edgestream.eof() || edgestream.fail() || !std::isblank(edgestream.peek()))
          throw MalformedEdgeVec();

        edgestream >> name;
        const NodeDesc v = get_id(name);
        if(edgestream.bad() || (!edgestream.eof() && edgestream.fail()) || !std::isspace(edgestream.peek()))
          throw MalformedEdgeVec();
        
        edges.emplace_back(u, v);
        while(edgestream.peek() == 10) edgestream.get();
      }
      return name_to_node.size();
    }

  };


  template<class EdgeList, class LabelMap>
  size_t parse_edgelist(std::istream& in, EdgeList& el, LabelMap& names)
  {
    return EdgeVecParser<EdgeList, LabelMap>(in, el, names).read_tree();
  }
  template<class EdgeList, class LabelMap>
  size_t parse_edgelist(std::istream& in, EdgeList& el, std::shared_ptr<LabelMap>& names)
  {
    return EdgeVecParser<EdgeList, LabelMap>(in, el, *names).read_tree();
  }

}


