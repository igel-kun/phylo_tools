

#pragma once

#include <memory>
#include <vector>
#include "edgelist.hpp"
#include "newick.hpp"

namespace PT{

  template<class Network, class LabelMap = typename Network::LabelMap, class EdgeVec = std::vector<typename Network::Edge>>
  struct EdgesAndNodeLabels
  {
    EdgeVec edges;
    std::shared_ptr<LabelMap> labels = std::make_shared<LabelMap>();
    size_t num_nodes = 0;

    void from_newick(std::ifstream& in) { num_nodes = parse_newick(in, edges, *labels); }
    void from_edgelist(std::ifstream& in) { num_nodes = parse_edgelist(in, edges, *labels); }
    void clear() { edges.clear(); labels->clear(); }
    bool is_tree() const { return edges.size() == num_nodes - 1; }
  };

  //! read edges from an input-file stream and return the number of nodes used by edges in the container
  template<class _EdgeListAndNodeLabels>
  bool read_edges(std::ifstream& in, _EdgeListAndNodeLabels& el)
  {
    try{
      DEBUG3(std::cout << "trying to read newick..." <<std::endl);
      el.from_newick(in);
    } catch(const MalformedNewick& nw_err){
      try{
        in.seekg(0);
        DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
        el.clear();
        el.from_edgelist(in);
      } catch(const MalformedEdgeVec& el_err){
        return false;
      }
    }
    return true;
  }

  template<class _EdgeListAndNodeLabels>
  bool read_edges(const std::string& filename, _EdgeListAndNodeLabels& el)
  {
    return read_edges(std::ifstream(filename), el);
  }

  //! read all edgelists provided in files and return their number of nodes
  template<class _EdgeListAndNodeLabels>
  void read_edgelists(const std::string& filename, std::vector<_EdgeListAndNodeLabels>& edgelists)
  {
    std::ifstream in(filename);
    while(!in.bad() && !in.eof()){
      if(!read_edges(in, *(edgelists.emplace(edgelists.end()))))
        edgelists.pop_back();
      while(std::isspace(in.peek())) in.get(); // remove leading whitespaces and newlines to prepare reading the next line
    }
    // as the last read attempt failed, we'll have an empty item in the back of edgelists... let's remove that
  }

  //! read all edgelists provided in files
  template<class _EdgeListAndNodeLabels>
  inline void read_edgelists(const std::vector<std::string>& filenames, std::vector<_EdgeListAndNodeLabels>& edgelists)
  {
    for(const auto& fn: filenames) read_edgelists(fn, edgelists);
  }

}


