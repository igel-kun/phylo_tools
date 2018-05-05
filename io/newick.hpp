
#pragma once

#include <vector>
#include "utils/iter_bitset.hpp"
#include "utils/types.hpp"
#include "utils/network.hpp"

//! an exception for the case that a graph property is read that is not up to date
struct MalformedNewick : public std::exception 
{
  const size_t pos;
  const std::string msg;

  MalformedNewick(const size_t _pos, const std::string _msg = "unknown error"):
    pos(_pos), msg(_msg + " (position " + std::to_string(_pos) + ")") {}

  const char* what() const throw() {
    return msg.c_str();
  }
};


std::string get_extended_newick(const PT::Network& N, const uint32_t sub_root, std::iterable_bitset& retis_seen)
{
  const PT::Network::Node& v = N[sub_root];
  std::string accu = "";
  if(!v.is_reti() || !retis_seen.test(sub_root)){
    accu += "(";
    for(uint32_t i = 0; i < v.succ.count; ++i)
      accu += get_extended_newick(N, v.succ[i], retis_seen) + ",";
    // remove last "," (or the "(" for leaves)
    accu.pop_back();
    if(!v.is_leaf()) accu += ")";
  }
  accu += N.get_name(sub_root);
  if(v.is_reti()) {
    accu += "#H" + std::to_string(sub_root);
    retis_seen.set(sub_root);
  }
  return accu;
}

std::string get_extended_newick(const PT::Network& N)
{
  std::iterable_bitset retis_seen(N.get_num_nodes());
  return get_extended_newick(N, N.get_root(), retis_seen) + ";";
}


//! a newick parser
//NOTE: we parse newick from the back to the front since the node names are _appended_ to the node instead of _prepended_
class NewickParser
{
  const std::string& s;
  std::vector<std::string>& names;
  std::unordered_map<uint32_t, uint32_t> hybrids;
  ssize_t back;

  bool parsed = false;

  // forbid default construction by skipping its implementation
  NewickParser();
public:

  NewickParser(const std::string& _newick_string, std::vector<std::string>& _names):
    s(_newick_string), names(_names), back(_newick_string.length() - 1)
  {
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

  const PT::NameVec& get_names() const
  {
    if(!parsed) throw std::logic_error("need to parse a newick string before testing anything");
    return names;
  }

  // a tree is a branch followed by a semicolon
  // EL can be an EdgeList or a WEdgeList if we are interested in branch-lengths
  template<typename EL = PT::EdgeList>
  void read_tree(EL& el)
  {
    skip_whitespaces();
    if(s != "") {
      if(s.at(back) == ';') --back; else throw MalformedNewick(back, "expected ';' but got \"" + s.substr(back) + "\"\n");
      skip_whitespaces();
      DEBUG5(std::cout << "parsing \"" << s << "\""<<std::endl);
      read_subtree(el);
    }
    parsed = true;
  }

private:

  void skip_whitespaces()
  {
    while((back > 0) && std::isspace(s.at(back))) --back;
  }

  inline bool is_reticulation(const PT::IndexPair& root) const
  {
    return root.second != UINT32_MAX;
  }

  // a subtree is a leaf or an internal vertex
  template<typename EL = PT::EdgeList>
  PT::IndexPair read_subtree(EL& el)
  {
    skip_whitespaces();

    // read the name of the root, any non-trailing whitespaces are considered part of the name
    uint32_t root = names.size();
    names.push_back(read_name());

    // find if root is a hybrid and, if so, it's number
    const auto hyb_info = get_hybrid_info(names.back());

    // if root is a known hybrid, then remove its name from 'names' and lookup its index in 'hybrids', else register it in 'hybrids'
    if(hyb_info.second != UINT32_MAX){
      const auto hyb_it = hybrids.find(hyb_info.second);
      if(hyb_it != hybrids.end()){
        names.pop_back();
        root = hyb_it->second;
      } else {
        names.back() = hyb_info.first;
        hybrids.emplace_hint(hyb_it, hyb_info.second, root);
      }
    }
    PT::IndexPair result = {root, hyb_info.second};

    // if the subtree dangling from root is non-empty, recurse
    if((back > 0) && s.at(back) == ')') read_internal(el, result);

    skip_whitespaces();
    return result;
  }

  // an internal vertex is ( + branchlist + )
  template<typename EL = PT::EdgeList>
  void read_internal(EL& el, const PT::IndexPair& root)
  {
    if(s.at(back) == ')') --back; else throw MalformedNewick(back, std::string("expected ')' but got '")+(char)(s.at(back))+"'");
    read_branchset(el, root);
    if(s.at(back) == '(') --back; else throw MalformedNewick(back, std::string("expected '(' but got '")+(char)s.at(back)+"'");
  }

  // a branchset is a comma-separated list of branches
  template<typename EL = PT::EdgeList>
  void read_branchset(EL& el, const PT::IndexPair& root)
  {
    read_branch(el, root);
    while(s.at(back) == ',') {
//      if(is_reticulation(root))
//        throw MalformedNewick(back, "reticulations may not have multiple children");
      --back;
      read_branch(el, root);
    }
  }

  // a branch is a subtree + a length
  template<typename EL = PT::EdgeList>
  void read_branch(EL& el, const PT::IndexPair& root)
  {
    const float len = read_length();
    const PT::IndexPair child = read_subtree(el);
    put_branch_in_edgelist(el, {root.first, child.first}, len);
  }

  inline void put_branch_in_edgelist(PT::EdgeList& el, const PT::Edge& e, const float& len) const
  {
    el.emplace_back(e);
  }
  inline void put_branch_in_edgelist(PT::EdgeVec& el, const PT::Edge& e, const float& len) const
  {
    el.emplace_back(e);
  }
  inline void put_branch_in_edgelist(PT::WEdgeList& el, const PT::Edge& e, const float& len) const
  {
    el.emplace_back(e, len);
  }
  inline void put_branch_in_edgelist(PT::WEdgeVec& el, const PT::Edge& e, const float& len) const
  {
    el.emplace_back(e, len);
  }

  // check if this is a hybrid and return name and hybrid number
  std::pair<std::string, uint32_t> get_hybrid_info(const std::string& name)
  {
    const size_t sharp = name.rfind('#');
    if(sharp != std::string::npos){
      std::pair<std::string, uint32_t> result(name.substr(0, sharp), UINT32_MAX);
      const size_t hybrid_num_start = name.find_first_of("0123456789", sharp);
      if(hybrid_num_start == std::string::npos) throw MalformedNewick(back, "found '#' but no hybrid number: \"" + name + "\"\n");
      result.second = std::atoi(name.c_str() + hybrid_num_start);
      //const std::string hybrid_type = name.substr(sharp + 1, hybrid_num_start - sharp - 1);
      return result;
    } else return {"", UINT32_MAX};
  }

  // read a branch-length
  float read_length()
  {
    const size_t sep = s.find_last_not_of("9876543210.-+Ee", back);
    if((sep != std::string::npos) && (s.at(sep) == ':')){
      const float result = std::strtof(s.c_str() + sep, NULL);
      back = sep - 1;
      return result;
    } else return 0;
  }


  // read the root's name
  std::string read_name()
  {
    size_t next = s.find_last_of("(),", back);
    // if we cannot find any of "()," before back, then the name stars at s[0]
    if(next == std::string::npos) next = 0;
    const size_t length = back - next;
    back = next;
    return s.substr(next + 1, length);
  }

};


