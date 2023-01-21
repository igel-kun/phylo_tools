
// this enumerates all valid sub-extensions of a given network in such a way that all subsets of an enumerated set have already been enumerated!
// a sub extension is a prefix of an extension and it is valid if there is no arc uv s.t. u is in the sub-extension but v is not
// note that we always have to branch on the node with the lowest post-order number
#pragma once

#include "stl_utils.hpp" // iterable_stack

namespace PT{

  // const Container& will be the return type of the dereference operator
  // for performance reasons, it should support fast mstd::test() queries, as well as insert() and erase()
  template<class Network, class Container = NodeSet, bool ignore_deg2_nodes = false>
  class NetworkConstraintSubsetIterator: public mstd::iter_traits_from_reference<const Container&> {
  protected:
    NodeDesc root;

    Container current; // current output set
    std::map<size_t, NodeDesc> available; // this maps post-order numbers to their nodes for all available nodes, sorted by post-order number
    mstd::iterable_stack<NodeDesc> branched; // nodes whose value has been branched

    std::unordered_map<NodeDesc, size_t> zero_fixed_children; // number of children that are not in the current set
    std::unordered_map<NodeDesc, size_t> po_number; // save the post-order number for each node
  
    static auto& node_of(const NodeDesc u) { return PT::node_of<Network>(u); }

    // mark all leaves available and initialize zero_fixed_children to the out-degrees
    void init_DFS(const NodeDesc u, size_t& time) {
      // use zero_fixed_children as indicator whether we've already seen u
      const auto& u_node = node_of(u);
      const bool success = mstd::append(zero_fixed_children, u, u_node.out_degree()).second;
      if(success){
        if(!u_node.is_leaf()) {
          for(NodeDesc v: u_node.children()){
            if constexpr (ignore_deg2_nodes){
              while(node_of(v).is_suppressible())
                v = mstd::front(node_of(v).children());
            }
            init_DFS(v, time);
          }
        } else branched.push(u);
        mstd::append(po_number, u, ++time);
      }
    }

    bool current_state(const NodeDesc u) const { return mstd::test(current, u); }

    void first_subset() {
      size_t time = 0;
      init_DFS(root, time);
    }

    // set a branched-on node to 0 and mark it available
    void un_branch(const NodeDesc u) {
      mstd::append(available, po_number.at(u), u);
      current.erase(u);
    }

    // propagate a change ?/1 -> 0 of u upwards
    void propagate_zero_up(const auto& u_node) {
      //std::cout << "propagating 0 to "<<u<<"\n";
      // if u was available, it no longer is, since a child of u is now 0-fixed
      for(NodeDesc v: u_node.parents()){
        if constexpr (ignore_deg2_nodes)
          while(node_of(v).is_suppressible())
            v = mstd::front(node_of(v).parents());
        if(++zero_fixed_children[v] == 1){
          available.erase(po_number.at(v));
          propagate_zero_up(node_of(v));
        }
      }
    }

    // propagate a change 0 -> 1/? of u upwards
    void propagate_nonzero_up(const auto& u_node) {
      //std::cout << "propagating non-0 to "<<u<<"\n";
      for(NodeDesc v: u_node.parents()){
        if constexpr (ignore_deg2_nodes)
          while(node_of(v).is_suppressible())
            v = mstd::front(node_of(v).parents());
        if(--zero_fixed_children[v] == 0){
          mstd::append(available, po_number.at(v), v);
          propagate_nonzero_up(node_of(v));
        }
      }
    }

    // set a node to 1 and propagate
    // note: no need to change availability since it's already non-available due to its 0-branch
    void branch_to_one(const NodeDesc u) {
      DEBUG4(std::cout << "switching branch of "<< u << " from "<<current_state(u)<<" ["<<mstd::test(available, po_number.at(u))<<"] to 1"<<std::endl);
      mstd::append(current, u);
      propagate_nonzero_up(node_of(u));
      DEBUG5(std::cout << "b1("<<u<<") -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
    }

    // first branch of a node u: mark u unavailable and propagate
    void branch_to_zero(const NodeDesc u) {
      DEBUG4(std::cout << "switching branch of "<< u << " from "<<current_state(u)<<" ["<<mstd::test(available, po_number.at(u))<<"] to 0"<<std::endl);
      available.erase(po_number.at(u));
      propagate_zero_up(node_of(u));
      branched.push(u);
      DEBUG5(std::cout << "b0("<<u<<") -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
    }

    void next_subset() {
      NodeDesc last_branched;
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
        while(!available.empty()) branch_to_zero(mstd::front(available).second);
        DEBUG5(std::cout << "++ -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
      }
    }

  public:

    NetworkConstraintSubsetIterator(const Network& N):
      NetworkConstraintSubsetIterator(N.root())
    {}

    NetworkConstraintSubsetIterator(const NodeDesc rt):
      root(rt)
    {
      if(rt != NoNode) {
        first_subset();
        DEBUG5(std::cout << "init -- current: "<<current<<" - available: "<<available<<" - branched: "<<branched<<std::endl);
      }
    }

    bool is_valid() const { return !branched.empty(); }
    //inline operator bool() { return is_valid(); }
 
    bool operator==(const NetworkConstraintSubsetIterator& it) const {
      if(!is_valid()) return !it.is_valid();
      if(!it.is_valid()) return false;
      return current == it.current;
    }

    const Container& operator*() { return current; }

    //! increment operator
    NetworkConstraintSubsetIterator& operator++() { next_subset(); return *this; }
    //! post-increment
    NetworkConstraintSubsetIterator operator++(int) { NetworkConstraintSubsetIterator tmp(*this); ++(*this); return tmp; }
  
  };

  template<class Network, class Container = NodeSet, bool ignore_deg_nodes = false>
  using NetworkConstraintSubsetFactory = mstd::IterFactory<NetworkConstraintSubsetIterator<Network, Container, ignore_deg_nodes>>;
 
}// namespace

