

#pragma once

#include <unordered_map>
#include <algorithm> // for all_of
#include <cctype> // for isspace

#include "utils/set_interface.hpp"
#include "utils/PTconfig.hpp"
#include "utils/types.hpp"

#include "common.hpp"

namespace PT{

  // ------ WRITE OUTPUT --------
  template<PhylogenyType _Network, DataExtracterType Extracter = DefaultDataExtracter<_Network>>
  void write_edgelist(std::ostream& os,
                      const _Network& N,
                      Extracter&& extracter = Extracter())
  {
    NodeMap<size_t> node_number;
    // step 1: write all nodes with node-data
    if constexpr (not Extracter::ignoring_node_data) {
      for(const NodeDesc u: N.nodes_preorder()) {
        std::ostringstream tmp;
        if constexpr (not Extracter::ignoring_node_labels) {
          const std::string out = std::to_string(extracter(Ex_node_label{}, u));
          if(not out.empty()) tmp << "\t:" << out;
        }
        if constexpr (not Extracter::ignoring_node_data) {
          const std::string out = std::to_string(extracter(Ex_node_data{}, u));
          if(not out.empty()) {
            if(tmp.str().empty()) tmp << "\t:"; else tmp << ',';
            tmp << out;
          }
        }
        const size_t node_num = node_number.emplace(u, node_number.size()).first->second;
        if(not tmp.str().empty())
          os << node_num  << tmp.str() <<'\n';
      }
    }
    // step 2: write all edges with their edge-data
    for(const auto uv: N.edges_preorder()) {
      const NodeDesc u = uv.tail();
      const NodeDesc v = uv.head();
      const auto& u_num = node_number.emplace(u, node_number.size()).first->second;
      const auto& v_num = node_number.emplace(v, node_number.size()).first->second;
      os << u_num << '\t' << v_num;
      if constexpr (not Extracter::ignoring_edge_data) {
        const std::string out = std::to_string(extracter(Ex_edge_data{}, uv));
        if(not out.empty()) os << "\t:" << out;
      }
      os << '\n';
    }
  }

  // compute the extended newick string for a network N 
  template<StrictPhylogenyType _Network, class... Args>
  std::string get_edgelist(const _Network& N, Args&&... args) {
    std::stringstream os;
    write_edgelist(os, N, std::forward<Args>(args)...);
    return os.str();
  }




  // ------ READ INPUT --------
  // edgelist format is:
  // 1. list of nodes with node-data (does not necessarily contain all nodes):
  //    <node name> [edge data]
  //    example:
  //      12 0.1,abcd,-1e12
  //      13 0.2
  //      10
  //      11 0.3
  //      ...
  //
  // 2. list of all edges with edge data:
  //    <parent> <child> [edge data]
  //    example:
  //      5 9 0.1,0.2,100,foo
  //      5 1 0.31
  //      0 5 0.4192,bar
  //      0 9
  //      0 1
  template<StrictEdgeEmplacerType Emplacer, bool allow_non_binary = true, bool allow_junctions = true>
  class EdgeListParser
  {
    std::istream* edgestream;
    Emplacer emplacer;
  public:

    template<class... Args>
    EdgeListParser(std::istream& _edgestream, Args&&... args):
      edgestream(&_edgestream),
      emplacer(std::forward<Args>(args)...)
    {}

    template<class... Args>
    NodeDesc make_node(const std::string_view label, const std::string_view name, Args&&... args) {
      DEBUG5(std::cout << "making new node with label '"<<label<<"' and name '"<<name<<"' and "<<sizeof...(Args)<<" further arg(s)\n");
      const NodeDesc result = emplacer.create_copy_of(name, std::forward<Args>(args)...);
      emplacer.set_label(result, label);
      return result;
    }

    NodeDesc get_id(const std::string& name, std::string_view data = std::string_view{}) {
      DEBUG4(std::cout << "getting id for name "<<name<<" with data '"<<data<<"'\n");
      if(not data.empty()) {
        std::string_view label;
        if(mstd::split_prefix_at_next(data, label, config::EL_delimeters.start_of_node_data)) {
          return make_node(label, name, data);
        } else return make_node(data, name);
      } else return make_node(data, name);
    }

    //! read edges and return the number of nodes used by them
    void read_tree() {
      static constexpr auto my_isspace = [](const char x){ return std::isspace(x); };
      ssize_t line_no = 0;
      std::string line, names, data, s1, s2;

      // step 1: read nodes and their node-data
      while(edgestream->good()){
        std::getline(*edgestream, line); ++line_no;
        if(not std::ranges::all_of(line, my_isspace )) {
          DEBUG5(std::cout << "got line from file: '"<<line << "'\n");
          std::istringstream linestream{line};
          std::getline(linestream, names, config::EL_delimeters.start_of_node_label);

          if(not std::ranges::all_of(names, my_isspace)) {
            std::getline(linestream, data);
            DEBUG4(
                if((not data.empty()) && (data.back() == 13)) data.pop_back(); // TODO: remove this
                std::cout << "split line '"<<line.substr(0,line.size()-1)<<"' into names: '"<<names<<"' and data: '"<<data<<"'\n"
            );
            
            std::istringstream namestream{names};
            namestream >> s1;
            namestream >> s2;
            DEBUG4(std::cout << "got names '"<<s1<<"' and '"<<s2<<"'\n");


            if(not s2.empty()) {
              // if s2 is not empty, then the line is an edge declaration, possibly with edge-data
              emplacer.emplace_edge(s1, s2, data);
            } else get_id(s1, data); // if s2 is empty, then the line is a node-data declaration for node 's1'
          
            DEBUG5(std::cout << "translate-map now: "<<emplacer.helper.old_to_new << '\n');
          }
        }
      }
    }

    void parse() { read_tree(); }
  };

  template<EdgeEmplacerType Emplacer>
  EdgeListParser(std::istream&, Emplacer&&) -> EdgeListParser<std::remove_cvref_t<Emplacer>>;

  template<class Network, class First, class... Args>
  Network parse_edgelist(First&& first, Args&&... args) {
    using EL_Translation = HashMap<std::string, NodeDesc>;
    return parse_network<Network, EdgeListParser>(std::forward<First>(first), EL_Translation{}, std::forward<Args>(args)...);
  }

}


