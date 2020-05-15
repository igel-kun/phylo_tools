
// this enumerates all valid sub-extensions of a given network in such a way that all subsets of an enumerated set have already been enumerated!
// a sub extension is a prefix of an extension and it is valid if there is no arc uv s.t. u is in the sub-extension but v is not
// note that we always have to branch on the node with the lowest post-order number
#pragma once

#include "stl_utils.hpp" // iterable_stack

namespace PT{


  // conost _Container& will be the return type of the dereference operator
  // for performance reasons, it should support fast std::test() queries, as well as insert() and erase()
  template<class _Network, class _Container = NodeSet>
  class NetworkConstraintSubsetIterator
  {
  protected:
    const _Network& N;
    const bool ignore_deg2_nodes;

    _Container current; // current output set
    std::map<size_t, Node> available; // this maps post-order numbers to their nodes for all available nodes, sorted by post-order number
    std::iterable_stack<Node> branched; // nodes whose value has been branched

    std::unordered_map<Node, size_t> zero_fixed_children; // number of children that are not in the current set
    std::unordered_map<Node, size_t> po_number; // save the post-order number for each node
  
    // mark all leaves available and initialize zero_fixed_children to the out-degrees
    void init_DFS(const Node u, size_t& time)
    {
      // use zero_fixed_children as indicator whether we've already seen u
      auto emp_res = zero_fixed_children.emplace(u, N.out_degree(u));
      if(emp_res.second){
        if(N.is_leaf(u))
          branched.push(u);
        else
          for(Node v: N.children(u)){
            if(ignore_deg2_nodes) while(N.is_suppressible(v)) v = std::front(N.children(v));
            init_DFS(v, time);
          }
        append(po_number, u, ++time);
      }
    }

    inline bool current_state(const Node u) const { return test(current, u); }
    inline void first_subset() {
      size_t time = 0;
      init_DFS(N.root(), time);
    }

    // set a branched-on node to 0 and mark it available
    void un_branch(const Node u)
    {
      append(available, po_number.at(u), u);
      current.erase(u);
    }

    // propagate a change ?/1 -> 0 of u upwards
    void propagate_zero_up(const Node u)
    {
      //std::cout << "propagating 0 to "<<u<<"\n";
      // if u was available, it no longer is, since a child of u is now 0-fixed
      for(Node v: N.parents(u)){
        if(ignore_deg2_nodes) while(N.is_suppressible(v)) v = std::front(N.parents(v));
        if(++zero_fixed_children[v] == 1){
          available.erase(po_number.at(v));
          propagate_zero_up(v);
        }
      }
    }

    // propagate a change 0 -> 1/? of u upwards
    void propagate_nonzero_up(const Node u)
    {
      //std::cout << "propagating non-0 to "<<u<<"\n";
      for(Node v: N.parents(u)){
        if(ignore_deg2_nodes) while(N.is_suppressible(v)) v = std::front(N.parents(v));
        if(--zero_fixed_children[v] == 0){
          append(available, po_number.at(v), v);
          propagate_nonzero_up(v);
        }
      }
    }

    // set a node to 1 and propagate
    // note: no need to change availability since it's already non-available due to its 0-branch
    void branch_to_one(const Node u)
    {
      DEBUG4(std::cout << "switching branch of "<< u << " from "<<current_state(u)<<" ["<<test(available, po_number.at(u))<<"] to 1"<<std::endl);
      append(current, u);
      propagate_nonzero_up(u);
      DEBUG5(std::cout << "b1("<<u<<") -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
    }

    // first branch of a node u: mark u unavailable and propagate
    void branch_to_zero(const Node u)
    {
      DEBUG4(std::cout << "switching branch of "<< u << " from "<<current_state(u)<<" ["<<test(available, po_number.at(u))<<"] to 0"<<std::endl);
      available.erase(po_number.at(u));
      propagate_zero_up(u);
      branched.push(u);
      DEBUG5(std::cout << "b0("<<u<<") -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
    }

    void next_subset()
    {
      Node last_branched;
      // eat up all the 1's on the branching-stack and mark them available
      while(is_valid() && current_state(last_branched = branched.top())){
        un_branch(last_branched);
        branched.pop();
      }
      // if the stack has been consumed, we are considered the end-iterator
      // if the stack still has items, the top item is currently branched to zero, so change its branch and update the available nodes
      if(is_valid()){
        branch_to_one(branched.top());
        // establish branches on all available nodes
        while(!available.empty()) branch_to_zero(front(available).second);
        DEBUG5(std::cout << "++ -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
      }
    }

  public:

    NetworkConstraintSubsetIterator(const _Network& _N, const bool construct_end_iterator = false, const bool _ignore_deg2_nodes = true):
      N(_N), ignore_deg2_nodes(_ignore_deg2_nodes)
    {
      if(!construct_end_iterator)
        first_subset();
      DEBUG5(std::cout << "init -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
    }

    inline bool is_valid() const { return !branched.empty(); }
    //inline operator bool() { return is_valid(); }
 
    bool operator==(const NetworkConstraintSubsetIterator& it) const
    {
      if(!is_valid()) return !it.is_valid();
      if(!it.is_valid()) return false;
      return current == it.current;
    }
    inline bool operator!=(const NetworkConstraintSubsetIterator& it) const { return !operator==(it); }

    const _Container& operator*() { return current; }

    //! increment operator
    NetworkConstraintSubsetIterator& operator++() { next_subset(); return *this; }
    //! post-increment
    NetworkConstraintSubsetIterator operator++(int) { NetworkConstraintSubsetIterator tmp(*this); ++(*this); return tmp; }
  
  };


  template<class _Network, class _Container = std::ordered_bitset>
  struct NetworkConstraintSubsetFactory
  {
    using iterator = NetworkConstraintSubsetIterator<_Network, _Container>;
    using const_iterator = iterator;

    const _Network& N;

    NetworkConstraintSubsetFactory(const _Network& _N): N(_N) {}

    iterator begin() const { return iterator(N); }
    iterator end() const { return iterator(N, true); }
  };
 
}// namespace

