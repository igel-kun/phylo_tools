

#pragma once

#include "utils/types.hpp"
#include <unordered_map>

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
    std::unordered_map<std::string, Node> name_to_node;

    EdgeVecParser();
  public:
    using Edge = typename EdgeList::value_type;

    EdgeVecParser(std::istream& _edgestream, EdgeList& _edges, LabelMap& _names):
      edgestream(_edgestream),
      edges(_edges),
      names(_names)
    {
      names.clear();
      edges.clear();
    }

    Node get_id(const std::string& name)
    {
      const auto name_it = name_to_node.find(name);
      if(name_it == name_to_node.end()){
        const Node id = (Node)names.size();
        name_to_node.emplace_hint(name_it, name, id);
        names.push_back(name);
        return id;
      } else return name_it->second;
    }

    void read_tree()
    {
      Edge e;
      while(!edgestream.eof()){
        std::string name;
        
        edgestream >> name;
        e.tail() = get_id(name);
        if(edgestream.bad() || edgestream.eof() || edgestream.fail() || !std::isblank(edgestream.peek()))
          throw MalformedEdgeVec();

        edgestream >> name;
        e.head() = get_id(name);
        if(edgestream.bad() || (!edgestream.eof() && edgestream.fail()) || !std::isspace(edgestream.peek()))
          throw MalformedEdgeVec();
        
        edges.push_back(e);
        while(edgestream.peek() == 10) edgestream.get();
      }
    }

  };

  template<class EdgeList, class LabelMap>
  void parse_edgelist(std::istream& in, EdgeList& el, LabelMap& names)
  {
    EdgeVecParser<EdgeList, LabelMap> parser(in, el, names);
    parser.read_tree();
  }



}


