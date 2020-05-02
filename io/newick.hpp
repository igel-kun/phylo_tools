
#pragma once

#include <vector>
#include "utils/types.hpp"
#include "utils/set_interface.hpp"
#include "utils/iter_bitset.hpp"
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"

namespace PT{
  using Index = uintptr_t;

  //! an exception for problems with the input string
  struct MalformedNewick : public std::exception 
  {
    const ssize_t pos;
    const std::string msg;

    MalformedNewick(const std::string& newick_string, const ssize_t _pos, const std::string _msg = "unknown error"):
      pos(_pos), msg(_msg + " (position " + std::to_string(_pos) + ")" + DEBUG3(" - relevant substring: "+newick_string.substr(_pos)) + "") {}

    const char* what() const throw() {
      return msg.c_str();
    }
  };

  template<class _Network>
  std::string get_extended_newick(const _Network& N, const Node sub_root, std::unordered_bitset& retis_seen)
  {
    std::string accu = "";
    if(!N.is_reti(sub_root) || !test(retis_seen, (Index)(sub_root))){
      accu += "(";
      for(const auto& w: N.children(sub_root))
        accu += get_extended_newick(N, w, retis_seen) + ",";
      // remove last "," (or the "(" for leaves)
      accu.pop_back();
      if(!N.is_leaf(sub_root)) accu += ")";
    }
    accu += N.label(sub_root);
    if(N.is_reti(sub_root)) {
      accu += "#H" + std::to_string(sub_root);
      retis_seen.set((Index)(sub_root));
    }
    return accu;
  }

  template<class _Network>
  std::string get_extended_newick(const _Network& N)
  {
    std::unordered_bitset retis_seen(N.num_nodes());
    return get_extended_newick(N, N.root(), retis_seen) + ";";
  }


  // helper functions to be able to transparently use weights or not
  template<template<class,class...> class _Container, class... Args>
  inline void put_branch_in_edgelist(_Container<Edge<>,Args...>& container, const Node x, const Node y, const float len)
  { append(container, x, y); }
  template<template<class,class...> class _Container, class... Args>
  inline void put_branch_in_edgelist(_Container<WEdge,Args...>& container, const Node x, const Node y, const float len)
  { append(container, x, y, len); }



  //! a newick parser
  //NOTE: we parse newick from the back to the front since the node names are _appended_ to the node instead of _prepended_
  // EL can be an EdgeList or a WEdgeList if we are interested in branch-lengths
  template<class EL, class LabelMap>
  class NewickParser
  {
    // a HybridInfo is a name of a hybrid together with it's hybrid-index
    using HybridInfo = std::pair<std::string, Index>;
    using IndexAndDegree = std::pair<Index, Degree>;
    using Edge = typename EL::value_type;

    const std::string& newick_string;
    LabelMap& names;

    // map a hybrid-index to a node index (and in-degree) so that we can find the corresponding hybrid when reading a hybrid number
    std::unordered_map<Index, IndexAndDegree> hybrids;
    ssize_t back;

    bool parsed = false;
    bool is_binary = true;

    // allow reading non-binary networks
    const bool allow_non_binary;
    // allow reading networks containing nodes with in- & out- degree both >1
    const bool allow_junctions;

    EL& edges;

    // forbid default construction by skipping its implementation
    NewickParser();
  public:

    NewickParser(const std::string& _newick_string,
                 EL& _edges,
                 LabelMap& _names,
                 const bool _allow_non_binary = true,
                 const bool _allow_junctions = true):
      newick_string(_newick_string),
      names(_names),
      hybrids(),
      back(_newick_string.length() - 1),
      allow_non_binary(_allow_non_binary),
      allow_junctions(_allow_junctions),
      edges(_edges)
    { read_tree(); }

    bool is_tree() const { return hybrids.empty(); }
    size_t num_nodes() const { return names.size(); }
    LabelMap& get_names() const { return names; }

    // a tree is a branch followed by a semicolon
    void read_tree()
    {
      skip_whitespaces();
      if(back >= 0) {
        if(newick_string.at(back) == ';') --back; else throw MalformedNewick(newick_string, back, "expected ';' but got \"" + newick_string.substr(back) + "\"\n");
        DEBUG5(std::cout << "parsing \"" << newick_string << "\""<<std::endl);
        read_subtree();
      }
      parsed = true;
      std::cout << "done parsing"<<std::endl;
    }

  private:

    inline void not_binary()
    {
      is_binary = false;
      if(!allow_non_binary)
        throw MalformedNewick(newick_string, back, "found non-binary node, which has been explicitly disallowed");
    }

    inline void skip_whitespaces()
    {
      while((back >= 0) && std::isspace(newick_string.at(back))) --back;
    }

    inline bool is_reticulation_degree(const Degree root_deg) const
    {
      return root_deg != UINT32_MAX;
    }

    // a subtree is a leaf or an internal vertex
    Index read_subtree()
    {
      skip_whitespaces();

      // read the name of the root, any non-trailing whitespaces are considered part of the name
      Index root = names.size();
      // keep root's name as rvalue in the air - we may or may not insert it into names, depending on its hybrid status
      std::string&& root_name = read_name();

      // find if root is a hybrid and, if so, it's number
      const HybridInfo hyb_info = get_hybrid_info(root_name);

      // if root is a known hybrid, then don't add its name to 'names' but lookup its index in 'hybrids', else register it in 'hybrids'
      if(hyb_info.second != UINT32_MAX){
        const auto emp_res = hybrids.try_emplace(hyb_info.second, root, 0);
        if(!emp_res.second){
          IndexAndDegree& stored = emp_res.first->second;
          // we've already seen a hybrid by that name - so don't insert root_name into names, but replace 'root' by the correct index
          root = stored.first;
          // increase the registered in-degree of 'root'
          const Index root_deg = ++stored.second;
          if((root_deg) == 3) not_binary();
        } else names.emplace(root, hyb_info.first);
      } else names.emplace(root, std::forward<std::string>(root_name));

      // if the subtree dangling from root is non-empty, then recurse
      if((back > 0) && newick_string.at(back) == ')') read_internal(root, hyb_info.second);

      skip_whitespaces();
      return root;
    }

    // an internal vertex is ( + branchlist + )
    void read_internal(const Index root, const Degree root_deg)
    {
      assert(back >= 2);
      if(newick_string.at(back) == ')') --back;
      else throw MalformedNewick(newick_string, back, std::string("expected ')' but got '")+(char)(newick_string.at(back))+"'");

      read_branchset(root, root_deg);
      
      if(newick_string.at(back) == '(') --back;
      else throw MalformedNewick(newick_string, back, std::string("expected '(' but got '")+(char)newick_string.at(back)+"'");
    }

    // a branchset is a comma-separated list of branches
    void read_branchset(const Index root, const Degree root_deg)
    {
      std::unordered_set<Index> children_seen;
      children_seen.insert(read_branch(root));
      while(newick_string.at(back) == ',') {
        if(is_reticulation_degree(root_deg)){
          not_binary();
          if(!allow_junctions)
            throw MalformedNewick(newick_string, back, "found reticulation with multiple children ('junction') which has been explicitly disallowed");
        }
        if(children_seen.size() == 3) not_binary();
        --back;
        const Index new_child = read_branch(root);
        if(!children_seen.emplace(new_child).second)
          throw MalformedNewick(newick_string, back, "read double edge "+ std::to_string(root) + " --> "+std::to_string(new_child));
        if(back < 0) throw MalformedNewick(newick_string, back, "unmatched ')'");
      }
    }

    // a branch is a subtree + a length
    // return the head of the read branch
    Index read_branch(const Index root)
    {
      const float len = read_length();
      const Index child = read_subtree();
      put_branch_in_edgelist(edges, (Node)(root), (Node)(child), len);
      return child;
    }

    // check if this is a hybrid and return name and hybrid number
    HybridInfo get_hybrid_info(const std::string& name)
    {
      const size_t sharp = name.rfind('#');
      if(sharp != std::string::npos){
        HybridInfo result = {name.substr(0, sharp), UINT32_MAX};
        const size_t hybrid_num_start = name.find_first_of("0123456789", sharp);
        if(hybrid_num_start != std::string::npos) {
          result.second = std::atoi(name.c_str() + hybrid_num_start);
          //const std::string hybrid_type = name.substr(sharp + 1, hybrid_num_start - sharp - 1);
          return result;
        } else throw MalformedNewick(newick_string, back, "found '#' but no hybrid number: \"" + name + "\"\n");
      } else return {"", UINT32_MAX};
    }

    // read a branch-length
    float read_length()
    {
      const size_t sep = newick_string.find_last_not_of("9876543210.-+Ee", back);
      if((sep != std::string::npos) && (newick_string.at(sep) == ':')){
        const float result = std::strtof(newick_string.c_str() + sep + 1, NULL);
        back = sep - 1;
        return result;
      } else return 0;
    }


    // read the root's name
    std::string read_name()
    {
      size_t next = newick_string.find_last_of("(),", back);
      // if we cannot find any of "()," before back, then the name stars at newick_string[0]
      if(next == std::string::npos) next = 0;
      const size_t length = back - next;
      back = next;
      return newick_string.substr(next + 1, length);
    }

  };

  template<class EdgeList, class LabelMap>
  void parse_newick(const std::string in, EdgeList& el, LabelMap& names)
  {
    NewickParser<EdgeList, LabelMap>(in, el, names);
  }
  template<class EdgeList, class LabelMap>
  void parse_newick(std::istream& in, EdgeList& el, LabelMap& names)
  {
    std::string in_line;
    std::getline(in, in_line);
    NewickParser<EdgeList, LabelMap>(in_line, el, names);
  }

}

