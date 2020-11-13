

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

#warning TODO: teach it to parse weighted edges

  template<class EdgeList, class LabelMap>
  class EdgeVecParser
  {
    std::istream& edgestream;
    EdgeList& edges;
    LabelMap& names;
    HashMap<std::string, Node> name_to_node;

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

    Node get_id(const std::string& name)
    {
      auto emp_res = append(name_to_node, name);
      if(emp_res.second){
        const Node id = (Node)names.size();
        emp_res.first->second = id;
        append(names, id, name);
        return id;
      } else return emp_res.first->second;
    }

    //! read edges and return the number of nodes used by them
    size_t read_tree()
    {
      while(!edgestream.eof()){
        std::string name;
        
        edgestream >> name;
        const Node u = get_id(name);
        if(edgestream.bad() || edgestream.eof() || edgestream.fail() || !std::isblank(edgestream.peek()))
          throw MalformedEdgeVec();

        edgestream >> name;
        const Node v = get_id(name);
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


