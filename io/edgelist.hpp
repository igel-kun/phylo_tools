

#pragma once

#include "utils/types.hpp"
#include <unordered_map>

struct MalformedEdgeVec : public std::exception 
{
  const char* what() const throw() {
    return "error reading edgelist";
  }
};

#warning TODO: teach it to parse weighted edges

template<class Edge = PT::Edge<> >
class EdgeVecParser
{
  std::ifstream& edgestream;
  std::vector<std::string>& names;

  std::unordered_map<std::string, uint32_t> name_to_id;
  PT::EdgeVec& edges;

  EdgeVecParser();
public:

  EdgeVecParser(std::ifstream& _edgestream, std::vector<std::string>& _names, PT::EdgeVec& _edges):
    edgestream(_edgestream),
    names(_names),
    edges(_edges)
  {
    names.clear();
    edges.clear();
  }

  uint32_t get_id(const std::string& name)
  {
    auto name_it = name_to_id.find(name);
    if(name_it == name_to_id.end()){
      const uint32_t id = names.size();
      name_to_id.emplace_hint(name_it, name, id);
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
