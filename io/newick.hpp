
#pragma once

#include <vector>
#include "utils/iter_bitset.hpp"
#include "utils/edge_iter.hpp"
#include "utils/network.hpp"

namespace PT{

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
  std::string get_extended_newick(const _Network& N, const uint32_t sub_root, std::unordered_bitset& retis_seen)
  {
    std::string accu = "";
    if(!N.is_reti(sub_root) || !retis_seen.test(sub_root)){
      accu += "(";
      for(const auto& w: N.children(sub_root))
        accu += get_extended_newick(N, w, retis_seen) + ",";
      // remove last "," (or the "(" for leaves)
      accu.pop_back();
      if(!N.is_leaf(sub_root)) accu += ")";
    }
    accu += N.get_name(sub_root);
    if(N.is_reti(sub_root)) {
      accu += "#H" + std::to_string(sub_root);
      retis_seen.set(sub_root);
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
  template<class Edge = Edge<> >
  inline void put_branch_in_edgelist(EdgeList& el, const Edge& e, const float& len)
  {
    el.emplace_back(e);
  }

  template<class Edge = Edge<> >
  inline void put_branch_in_edgelist(EdgeVec& el, const Edge& e, const float& len)
  {
    el.emplace_back(e);
  }

  template<class Edge = Edge<> >
  inline void put_branch_in_edgelist(WEdgeList& el, const Edge& e, const float& len)
  {
    el.emplace_back(e, len);
  }

  template<class Edge = Edge<> >
  inline void put_branch_in_edgelist(WEdgeVec& el, const Edge& e, const float& len)
  {
    el.emplace_back(e, len);
  }




  //! a newick parser
  //NOTE: we parse newick from the back to the front since the node names are _appended_ to the node instead of _prepended_
  // EL can be an EdgeList or a WEdgeList if we are interested in branch-lengths
  template<class EL = EdgeList>
  class NewickParser
  {
    // a HybridInfo is a name of a hybrid together with it's hybrid-index
    using HybridInfo = std::pair<std::string, uint32_t>;

    const std::string& newick_string;
    std::vector<std::string>& names;

    // map a hybrid-index to a node index (and in-degree) so that we can find the corresponding hybrid when reading a hybrid number
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> hybrids;
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
                 NameVec& _names,
                 EL& _edges,
                 const bool _allow_non_binary = true,
                 const bool _allow_junctions = true):
      newick_string(_newick_string),
      names(_names),
      back(_newick_string.length() - 1),
      allow_non_binary(_allow_non_binary),
      allow_junctions(_allow_junctions),
      edges(_edges)
    {
      names.clear();
      edges.clear();
    }

    bool is_tree() const
    {
      if(!parsed) throw std::logic_error("need to parse a newick string before testing anything");
      return hybrids.empty();
    }

    const size_t num_nodes() const
    {
      if(!parsed) throw std::logic_error("need to parse a newick string before testing anything");
      return names.size();
    }

    const NameVec& get_names() const
    {
      if(!parsed) throw std::logic_error("need to parse a newick string before testing anything");
      return names;
    }

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

    inline bool is_reticulation(const IndexPair& root) const
    {
      return root.second != UINT32_MAX;
    }

    // a subtree is a leaf or an internal vertex
    IndexPair read_subtree()
    {
      skip_whitespaces();

      // read the name of the root, any non-trailing whitespaces are considered part of the name
      uint32_t root = names.size();
      names.push_back(read_name());

      // find if root is a hybrid and, if so, it's number
      const HybridInfo hyb_info = get_hybrid_info(names.back());

      // if root is a known hybrid, then remove its name from 'names' and lookup its index in 'hybrids', else register it in 'hybrids'
      if(hyb_info.second != UINT32_MAX){
        const auto hyb_it = hybrids.find(hyb_info.second);
        if(hyb_it != hybrids.end()){
          names.pop_back();
          root = hyb_it->second.first;
          if(++(hyb_it->second.second) == 3) not_binary();
        } else {
          names.back() = hyb_info.first;
          hybrids.emplace_hint(hyb_it, hyb_info.second, std::pair<uint32_t, uint32_t>(root, 0));
        }
      }
      IndexPair result = {root, hyb_info.second};

      // if the subtree dangling from root is non-empty, then recurse
      if((back > 0) && newick_string.at(back) == ')') read_internal(result);

      skip_whitespaces();
      return result;
    }

    // an internal vertex is ( + branchlist + )
    void read_internal(const IndexPair& root)
    {
      assert(back >= 2);
      if(newick_string.at(back) == ')') --back;
      else throw MalformedNewick(newick_string, back, std::string("expected ')' but got '")+(char)(newick_string.at(back))+"'");

      read_branchset(root);
      
      if(newick_string.at(back) == '(') --back;
      else throw MalformedNewick(newick_string, back, std::string("expected '(' but got '")+(char)newick_string.at(back)+"'");
    }

    // a branchset is a comma-separated list of branches
    void read_branchset(const IndexPair& root)
    {
      std::unordered_set<uint32_t> children_seen;
      children_seen.insert(read_branch(root));
      while(newick_string.at(back) == ',') {
        if(is_reticulation(root)){
          not_binary();
          if(!allow_junctions)
            throw MalformedNewick(newick_string, back, "found reticulation with multiple children ('junction') which has been explicitly disallowed");
        }
        if(children_seen.size() == 3) not_binary();
        --back;
        const uint32_t new_child = read_branch(root);
        if(!children_seen.emplace(new_child).second)
          throw MalformedNewick(newick_string, back, "read double edge "+ std::to_string(root.first) + " --> "+std::to_string(new_child));
        if(back < 0) throw MalformedNewick(newick_string, back, "unmatched ')'");
      }
    }

    // a branch is a subtree + a length
    // return the head of the read branch
    uint32_t read_branch(const IndexPair& root)
    {
      const float len = read_length();
      const IndexPair child = read_subtree();
      put_branch_in_edgelist(edges, {root.first, child.first}, len);
      return child.first;
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

}

