

#pragma once

#include <memory>
#include <vector>
#include "edgelist.hpp"
#include "newick.hpp"

namespace PT{

  template<class Network, class LabelMap = typename Network::LabelMap, class EdgeVec = std::vector<typename Network::Edge>>
  using EdgeVecAndNodeLabel = std::pair<EdgeVec, std::shared_ptr<LabelMap>>;

  template<class _EdgeListAndNodeLabel>
  bool read_edges(std::ifstream& in, _EdgeListAndNodeLabel& el)
  {
    try{
      DEBUG3(std::cout << "trying to read newick..." <<std::endl);
      parse_newick(in, el.first, el.second);
    } catch(const MalformedNewick& nw_err){
      try{
        in.seekg(0);
        DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
        parse_edgelist(in, el.first, el.second);
      } catch(const MalformedEdgeVec& el_err){
        return false;
      }
    }
    DEBUG3(std::cout << "read edges: "<<el<<"\n");
    return true;
  }

  template<class _EdgeListAndNodeLabel>
  bool read_edges(const std::string& filename, _EdgeListAndNodeLabel& el)
  {
    return read_edges(std::ifstream(filename), el);
  }

  //! read all edgelists provided in files
  template<class _EdgeListAndNodeLabel>
  void read_edgelists(const std::string& filename, std::vector<_EdgeListAndNodeLabel>& edgelists)
  {
    std::ifstream in(filename);
    while(read_edges(in, *(edgelists.emplace(edgelists.end()))))
      while(std::isspace(in.peek())) in.get(); // remove leading whitespaces and newlines to prepare reading the next line
    // as the last read attempt failed, we'll have an empty item in the back of edgelists... let's remove that
    edgelists.pop_back();
  }

  //! read all edgelists provided in files
  template<class _EdgeListAndNodeLabel>
  inline void read_edgelists(const std::vector<std::string>& filenames, std::vector<_EdgeListAndNodeLabel>& edgelists)
  {
    for(const auto& fn: filenames) read_edgelists(fn, edgelists);
  }

}


