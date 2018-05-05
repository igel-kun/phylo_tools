

#pragma once

#include "utils/types.hpp"
#include <unordered_map>

struct MalformedEdgeVec : public std::exception 
{

  const char* what() const throw() {
    return "error reading edgelist";
  }
};


class EdgeVecParser
{
  std::ifstream& edgestream;
  std::vector<std::string>& names;

  std::unordered_map<std::string, uint32_t> name_to_id;

  EdgeVecParser();
public:

  EdgeVecParser(std::ifstream& _edgestream, std::vector<std::string>& _names):
    edgestream(_edgestream), names(_names)
  {
    names.clear();
  }

  uint32_t get_id(const std::string& name)
  {
    auto name_it = name_to_id.find(name);
    if(name_it == name_to_id.end()){
      name_to_id.emplace_hint(name_it, name, names.size());
      names.push_back(name);
      return names.size() - 1;
    } else return name_it->second;
  }

  void read_tree(PT::EdgeVec& el)
  {
    PT::Edge e;
    while(!edgestream.eof()){
      std::string name;
      edgestream >> name;
      e.first = get_id(name);
      if(edgestream.bad() || edgestream.eof() || edgestream.fail()) throw MalformedEdgeVec();
      edgestream >> name;
      e.second = get_id(name);
      if(edgestream.bad()) throw MalformedEdgeVec();
      if(!edgestream.eof() && edgestream.fail()) throw MalformedEdgeVec();
      
      el.push_back(e);
      while(edgestream.peek() == 10) edgestream.get();
    }
  }

};
