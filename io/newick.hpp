
#pragma once

#include <vector>
#include "../utils/types.hpp"

//! an exception for the case that a graph property is read that is not up to date
struct MalformedNewick : public std::exception 
{
  const size_t pos;
  const std::string msg;

  MalformedNewick(const size_t _pos, const std::string _msg = "unknown error"):
    pos(_pos), msg(_msg + "(position " + std::to_string(_pos) + ")") {}

  const char* what() const throw() {
    return msg.c_str();
  }
};

typedef std::pair<uint32_t, uint32_t> IndexPair;

class NewickParser
{
  const std::string& s;
  std::vector<std::string>& names;
  std::unordered_map<uint32_t, uint32_t> hybrids;
  std::unordered_map<uint32_t, uint32_t> rev_hybrids;
  uint32_t front;

  // indicate whether we are going to contract edges between reticulations
  const bool contract_retis;

  // forbid default construction by skipping its implementation
  NewickParser();
public:

  NewickParser(const std::string& _newick_string, std::vector<std::string>& _names, const bool _contract_retis = false):
    s(_newick_string), names(_names), front(0), contract_retis(_contract_retis)
  {
    DEBUG5(std::cout << "parsing " << _newick_string << std::endl);
  }

  bool is_tree() const
  {
    return hybrids.empty();
  }

  const uint32_t num_vertices() const
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
    read_subtree(el);
    if(s.at(front) != ';') throw MalformedNewick(front, "expected ';' but got \"" + s.substr(front) + "\"\n");
  }

private:

  void skip_whitespaces()
  {
    while((front < s.length()) && (s.at(front) == ' ')) ++front;
  }


  // a subtree is a leaf or an internal vertex
  template<typename EL = TC::Edgelist>
  IndexPair read_subtree(EL& el)
  {
    const uint32_t root = names.size();
    names.push_back("");

    skip_whitespaces();
    if(s.at(front) == '(')
      return read_internal(el, root);
    else 
      return read_leaf(root);
  }

  // a leaf is just a name
  IndexPair read_leaf(const uint32_t root)
  {
    return read_name(root, true);
  }

  // check the edges with the same tail on el for duplications
  template<class EL = TC::Edgelist>
  void check_last_edges(EL& el)
  {
    std::unordered_set<uint32_t> hybrids_seen;
    const uint32_t root = TC::get_tail(el.back());
    for(auto e_it = el.rbegin(); e_it != el.rend(); ++e_it){
      // only consider edges with the same tail
      if(TC::get_tail(*e_it) != root) break;

      const uint32_t child = TC::get_head(*e_it);
      const auto rev_h_it = rev_hybrids.find(child);
      if(rev_h_it != rev_hybrids.end()){
        const auto seen_it = hybrids_seen.find(child);
        if(seen_it != hybrids_seen.end())
          throw MalformedNewick(front, "vertex " + std::to_string(root) + " (" + names[root] + ") has the same child twice: " + std::to_string(child) + " (" + names[child]+")\n");
        else 
          hybrids_seen.emplace_hint(seen_it, child);
      }
    }
  }


  // an internal vertex is ( + branchlist + ) + name
  template<typename EL = TC::Edgelist>
  IndexPair read_internal(EL& el, const uint32_t root)
  {
    assert(s.at(front) == '(');
    ++front;
    const uint32_t num_children = read_branchset(el, root);
    assert(s.at(front) == ')');
    ++front;
    IndexPair root_ids = read_name(root, false);

    if(root_ids.second != UINT32_MAX){
      // so the root was a hybrid
      if(contract_retis){
        // if we want automatic hybrid-edge contraction, we have to change the inserted edge
        // (we allow only a single edge as, otherwise, the input could contain two hybrids and
        // we would have to merge those two, which would require a pass of the complete edgelist)
        if(num_children > 1) throw MalformedNewick(front, "reticulations may not have multiple children in automatic-contraction mode\n");
        // get the child
        const uint32_t child = TC::get_head(el.back());
        // check if the unique child of root is a reticulation
        const auto child_h_it = rev_hybrids.find(child);
        if(child_h_it != rev_hybrids.end()){
          DEBUG3(std::cout << "contracting edge "<<el.back()<<" ("<<names.at(TC::get_tail(el.back()))<<"->"<<names.at(TC::get_head(el.back()))<<") as it is between 2 hybrids"<<std::endl);
          // remove the last edge
          el.pop_back();
          // replace root by its child in the data-structures
          hybrids[root_ids.second] = child;
          root_ids.first = child;
        }
      }
    } else check_last_edges(el);
    return root_ids;
  }

  // a branchset is a comma-separated list of branches
  template<typename EL = TC::Edgelist>
  uint32_t read_branchset(EL& el, const uint32_t root)
  {
    read_branch(el, root);
    uint32_t count = 1;
    while(s.at(front) == ','){
      ++front;
      ++count;
      read_branch(el, root);
    }
    return count;
  }


  // a branch is a subtree + a length
  void read_branch(TC::Edgelist& el, const uint32_t root)
  {
    const IndexPair child = read_subtree(el);
    el.emplace_back(root, child.first);
  }

  // a branch is a subtree + a length
  void read_branch(TC::WEdgelist& el, const uint32_t root)
  {
    const IndexPair child = read_subtree(el);
    el.emplace_back(PWC, std::make_tuple(root, child.first), std::make_tuple(read_length()));
  }

  // check if this is a hybrid
  uint32_t get_hybrid_number(const std::string& name)
  {
    const size_t sharp = name.find('#');
    if(sharp != std::string::npos){
      const std::string hybrid_name = name.substr(0, sharp);
      const size_t hybrid_num_start = name.find_first_of("0123456789", sharp);
      if(hybrid_num_start == std::string::npos) throw MalformedNewick(front, "found '#' but no hybrid number: \"" + name + "\"\n");

      const std::string hybrid_type = name.substr(sharp + 1, hybrid_num_start - sharp - 1);
      return std::atoi(name.c_str() + hybrid_num_start);
    } else return UINT32_MAX;
  }


  // read the root's name and return the index of the vertex read (=root unless it's a hybrid) plus the hybrid index of the vertex
  IndexPair read_name(const uint32_t root, const bool is_leaf = false)
  {
    const size_t next = s.find_first_of("(),:;", front);
    if(next == std::string::npos) throw MalformedNewick(front, "expected name but got \"" + s.substr(front) + "\"\n");

    const std::string name = s.substr(front, next - front);
    const uint32_t hybrid_number = get_hybrid_number(name);

    IndexPair result = {root, hybrid_number};
    if(hybrid_number != UINT32_MAX){
      // see if we've already seen this hybrid
      const auto hyb_it = hybrids.find(hybrid_number);
      if(hyb_it == hybrids.end()){
        // this is a new hybrid, so register it in hybrids & rev_hybrids
        hybrids.emplace_hint(hyb_it, hybrid_number, root);
        rev_hybrids.emplace(root, hybrid_number);
      } else {
        // update the return value with the correct index
        result.first = hyb_it->second;
        // if we know this hybrid already, we have to pop_back the name-slot that we reserved for it
        names.pop_back(); 
        // already knowing a hybrid is only OK if it comes as a leaf, only so-far unknown hybrids may have a subtree!
        if(!is_leaf) throw MalformedNewick(front, "previously introduced hybrids may not have a subtree: \"" + s.substr(front) + "\"\n");
      }
    }
    
    names[result.first] = name;
    DEBUG6(std::cout << "vertex "<<result.first<<" is known as "<< name << std::endl);
    
    front = next;
    return result;
  }

  // read a branch length
  float read_length()
  {
    if(s.at(front) == ':') {
      const char* s_cstr = s.c_str();
      char* endptr;
      float length = std::strtof(s_cstr + front, &endptr);
      front = endptr - s_cstr;
      
      // advance front
      const size_t next = s.find_first_of("(),:;", front);
      if(next == std::string::npos) throw MalformedNewick(front, "expected branchlength but got \"" + s.substr(front) + "\"\n");
      front = next;
      return length;
    } else return -1;
  }


};


