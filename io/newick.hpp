
#pragma once

#include <vector>
#include "utils/types.hpp"

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

typedef std::pair<uint32_t, uint32_t> IndexPair;

//! a newick parser
//NOTE: we parse newick from the back to the front since the node names are _appended_ to the node instead of _prepended_
class NewickParser
{
  const std::string& s;
  std::vector<std::string>& names;
  std::unordered_map<uint32_t, uint32_t> hybrids;
  ssize_t back;

  // indicate whether we are going to contract edges between reticulations
  const bool contract_retis;

  // forbid default construction by skipping its implementation
  NewickParser();
public:

  NewickParser(const std::string& _newick_string, std::vector<std::string>& _names, const bool _contract_retis = false):
    s(_newick_string), names(_names), back(_newick_string.length() - 1), contract_retis(_contract_retis)
  {
    DEBUG5(std::cout << "parsing " << _newick_string << std::endl);
  }

  bool is_tree() const
  {
    return hybrids.empty();
  }

  const size_t num_vertices() const
  {
    return names.size();
  }

  const TC::NameVec& get_names() const
  {
    return names;
  }

  // a tree is a branch followed by a semicolon
  // EL can be an Edgelist or a WEdgelist if we are interested in branch-lengths
  template<typename EL = TC::Edgelist>
  void read_tree(EL& el)
  {
    skip_whitespaces();
    if(s.at(back) == ';') --back; else throw MalformedNewick(back, "expected ';' but got \"" + s.substr(back) + "\"\n");
    skip_whitespaces();
    read_subtree(el);
  }

private:

  void skip_whitespaces()
  {
    while((back > 0) && (s.at(back) == ' ' || s.at(back) == '\t')) --back;
  }


  // a subtree is a leaf or an internal vertex
  template<typename EL = TC::Edgelist>
  IndexPair read_subtree(EL& el)
  {
    skip_whitespaces();

    // read the name of the root, any non-trailing whitespaces are considered part of the name
    uint32_t root = names.size();
    names.push_back(read_name());

    // find if root is a hybrid and, if so, it's number
    const uint32_t root_hnum = get_hybrid_number(names.back());

    // if root is a known hybrid, then remove its name from 'names' and lookup its index in 'hybrids', else register it in 'hybrids'
    if(root_hnum != UINT32_MAX){
      const auto hyb_it = hybrids.find(root_hnum);
      if(hyb_it != hybrids.end()){
        names.pop_back();
        root = hyb_it->second;
      } else hybrids.emplace_hint(hyb_it, root_hnum, root);
    }
    IndexPair result = {root, root_hnum};

    // if the subtree dangling from root is non-empty, recurse
    if((back > 0) && s.at(back) == ')') read_internal(el, result);

    skip_whitespaces();
    return result;
  }

  // an internal vertex is ( + branchlist + )
  template<typename EL = TC::Edgelist>
  void read_internal(EL& el, const IndexPair& root)
  {
    if(s.at(back) == ')') --back; else throw MalformedNewick(back, std::string("expected ')' but got '")+(char)(s.at(back))+"'");
    read_branchset(el, root);
    if(s.at(back) == '(') --back; else throw MalformedNewick(back, std::string("expected '(' but got '")+(char)s.at(back)+"'");
  }

  // a branchset is a comma-separated list of branches
  template<typename EL = TC::Edgelist>
  void read_branchset(EL& el, const IndexPair& root)
  {
    read_branch(el, root);
    while(s.at(back) == ',') {
      --back;
      read_branch(el, root);
    }
  }

  // a branch is a subtree + a length
  template<typename EL = TC::Edgelist>
  void read_branch(EL& el, const IndexPair& root)
  {
    const float len = read_length();
    const IndexPair child = read_subtree(el);
    put_branch_in_edgelist(el, {root.first, child.first}, len);
  }

  void put_branch_in_edgelist(TC::Edgelist& el, const TC::Edge& e, const float& len)
  {
    el.emplace_back(e);
  }
  void put_branch_in_edgelist(TC::WEdgelist& el, const TC::Edge& e, const float& len)
  {
    el.emplace_back(e, len);
  }

  // check if this is a hybrid
  uint32_t get_hybrid_number(const std::string& name)
  {
    const size_t sharp = name.rfind('#');
    if(sharp != std::string::npos){
      const std::string hybrid_name = name.substr(0, sharp);
      const size_t hybrid_num_start = name.find_first_of("0123456789", sharp);
      if(hybrid_num_start == std::string::npos) throw MalformedNewick(back, "found '#' but no hybrid number: \"" + name + "\"\n");

      const std::string hybrid_type = name.substr(sharp + 1, hybrid_num_start - sharp - 1);
      return std::atoi(name.c_str() + hybrid_num_start);
    } else return UINT32_MAX;
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


