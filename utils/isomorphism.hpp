

#pragma once

#include "utils.hpp"
#include "network.hpp"
#include "vector2d.hpp"
#include "iter_bitset.hpp"
#include "set_interface.hpp"
#include "label_matching.hpp"
#include <unordered_set>
#include <queue>

#define FLAG_MAP_LEAF_LABELS 0x01
#define FLAG_MAP_TREE_LABELS 0x02
#define FLAG_MAP_RETI_LABELS 0x04
#define FLAG_MAP_ALL_LABELS 0x07


namespace PT{
  struct NoPoss : public std::exception
  {
    const std::string msg;

    template<class Net>
    NoPoss(const Net& N, const NodeDesc u):
      msg(N.label(u) + '[' + std::to_string(u) + "] is unmappable\n")
    {}
    NoPoss(const std::string& _msg): msg(_msg) {}
    const char* what() const noexcept { return msg.c_str(); }
  };


  //NOTE: you may customize the possibility-set type to your needs:
  //    for example, for single-labeled trees, we recommend a singleton_set,
  //    for low-multiply labeled trees & low-level networks an unordered_set<NodeDesc> should be good
  template<class NetworkA, class NetworkB, class _PossSet = mstd::unordered_bitset>
  class IsomorphismMapper
  {
    using PossSet     = _PossSet;
    using MappingPossibility = NodeMap<PossSet>;
    using UpdateSet   = mstd::unordered_bitset;
    using UpdateOrder = std::priority_queue<NodePair, std::vector<NodePair>, std::greater<NodePair> >;
    using LabelMatch  = LabelMatching<NetworkA, NetworkB>;
    using LabelType   = typename LabelMatch::LabelType;

    const NetworkA& N1;
    const NetworkB& N2;
    const LabelMatch& lmatch; // a label-matching matching nodes of N1 with nodes of N2 if they all have the same label

    const size_t size_N;  // nodes in each of the input networks
    size_t nr_fix = 0;    // number of fixed nodes

    MappingPossibility mapping; // indicates for each node of N1 to which nodes of N2 it may map in an isomorphism

    UpdateSet update_set; // set containing all nodes of N1 for which updates are pending
    UpdateOrder update_order; // a priority-queue containing all pending updates sorted by number of possibilities
   
    const unsigned char flags;

    // use this to indicate a sanity check failure like:
    // vertices/edges mismatch
    // leaves/vertices unmappable
    // degree distributions don't match
    bool initial_fail; 

    inline size_t num_poss(const NodeDesc x) const { return mapping.at(x).size(); }

    bool node_is_interesting(const NodeDesc v) const {
      switch(NetworkA::type_of(v)){
        case NODE_TYPE_LEAF:
          return (flags & FLAG_MAP_LEAF_LABELS);
        case NODE_TYPE_INTERNAL_TREE:
          return (flags & FLAG_MAP_TREE_LABELS);
        case NODE_TYPE_INTERNAL_RETI:
          return (flags & FLAG_MAP_RETI_LABELS);
        default:
          return true;
      }
    }



    inline void mark_update(const NodeDesc x, const size_t nr_poss)
    {
      if(nr_poss == 1) ++nr_fix;
      if(update_set.set(x))
        update_order.emplace(nr_poss, x);
      DEBUG5(std::cout << "after update of "<<x<<": update set is now: "<< to_set<NodeSet>(update_set) <<"\n");
    }

    // set the unique mapping possibility of x1 to x2; fail if x1 has already been determined to not map to x2
    void set_unique_poss(const NodeDesc x1, const NodeDesc x2)
    {
      auto [iter, success] = mapping.try_emplace(x1);
      if(!success){
        PossSet& poss_in_N2 = iter->second;
        if(test(poss_in_N2, x2)){
          if(poss_in_N2.size() > 1) mark_update(x1, 1);
          poss_in_N2.clear();
        } else throw NoPoss(N1, x1);
        poss_in_N2.insert(x2);
      } else mark_update(x1, 1);
    }


    template<class NetAndNodeFunc>
    void restrict_by_something(const auto& N1_nodes, const auto& N2_nodes, NetAndNodeFunc&& f) {
      using Something = std::remove_reference_t<std::invoke_result_t<NetAndNodeFunc, NetworkA, NodeDesc>>;
      // keep track of nodes in N2 mapping to each something
      // also keep a history of how many of each Something we've seen (this should be equal between N1 & N2)
      using PossAndHist = std::pair<PossSet, size_t>;
      HashMap<Something, PossAndHist> poss_and_hist;

      DEBUG5(std::cout << "comparing nodes\n"<<N2_nodes<<"\nto\n"<<N1_nodes<<"\n");
      for(const NodeDesc u: N2_nodes){
        DEBUG5(std::cout << "registering "<<u<<": "<<f(N2,u)<<"\n");
        PossAndHist& ph = poss_and_hist.try_emplace(f(N2,u), size_N, 0).first->second;
        ph.first.set(u);
        ph.second++;
      }
      for(const NodeDesc u: N1_nodes){
        const auto it = poss_and_hist.find(f(N1, u));
        if(it != poss_and_hist.end()) {
          DEBUG5(std::cout << "checking "<<u<<": "<<f(N1,u)<<"\n");
          update_poss(u, it->second.first);
          if((it->second.second)-- == 0) throw NoPoss("node histograms differ");
        } else throw NoPoss(N1, u);
      }
    }

    // use degrees and labels to restrict possibilities
    void degree_and_label_restrict()
    {
      constexpr auto get_label = [](const auto& Net, const NodeDesc u){ return Net.label(u); };
      constexpr auto get_degree = [](const auto& Net, const NodeDesc u){ return Net.degrees(u); };

      if(flags == FLAG_MAP_LEAF_LABELS)
        restrict_by_something(N1.leaves(), N2.leaves(), get_label);
      else
        restrict_by_something(N1.nodes(), N2.nodes(), get_label);

      // all updated nodes have been fixed
      if(nr_fix < size_N){
        treat_pending_updates();
        restrict_by_something(N1.nodes(), N2.nodes(), get_degree);
      }
    }



    IsomorphismMapper(const NetworkA& _N1,
                      const NetworkB& _N2,
                      const uint32_t _size_N,
                      const LabelMatch& _lmatch,
                      const unsigned char _flags,
                      const MappingPossibility& _mapping = MappingPossibility()):
      N1(_N1),
      N2(_N2),
      lmatch(_lmatch),
      size_N(_size_N),
      mapping(_mapping),
      update_set(size_N), // <--- be aware that update_set and update_order are cleared on construction, even copy-construct!!!
      update_order(),
      flags(_flags),
      initial_fail(false)
    {}

    // note: copy construct clears certain sets, so needs to be explicit
    IsomorphismMapper(const IsomorphismMapper& _mapper):
      IsomorphismMapper(_mapper.N1, _mapper.N2, _mapper.size_N, _mapper.lmatch, _mapper.flags, _mapper.mapping)
    {}

    IsomorphismMapper(IsomorphismMapper&& _mapper) = default;

  public:
    IsomorphismMapper(const NetworkA& _N1,
                      const NetworkB& _N2,
                      const LabelMatch& _lmatch,
                      const unsigned char _flags):
      IsomorphismMapper(_N1, _N2, _N1.num_nodes(), _lmatch, _flags)
    {
      DEBUG3(std::cout << "#nodes: "<<N1.num_nodes()<<" & "<<N2.num_nodes()<<"\t\t#edges: "<<N1.num_edges()<<" & "<<N2.num_edges()<<std::endl;);
      if((N1.num_nodes() == N2.num_nodes()) && (N1.num_edges() == N2.num_edges())){
        try{
          degree_and_label_restrict();
          DEBUG3(std::cout << "done initializing mapper"<<std::endl);
        } catch(const NoPoss& np){
          DEBUG3(std::cout << np.msg << std::endl);
          initial_fail = true;
        } catch(const std::out_of_range& oor){
          DEBUG3(std::cout << "unable to map a leaf of N0 to N1: "<< oor.what() << std::endl);
          initial_fail = true;
        }
      } else initial_fail = true;        
    }

    bool check_isomorph()
    {
      // if we determined already that those will not be isomorphic, just fail here too
      if(initial_fail) return false;
      if(nr_fix == size_N) return true;

      try{
        treat_pending_updates();

        DEBUG5(std::cout << "possibilities are:" << std::endl; for(NodeDesc u: N1.nodes()) std::cout << u << "\t"<< to_set(mapping.at(u)) << std::endl);
        DEBUG3(std::cout << "no more pending updated"<<std::endl);

        // find a vertex to branch on (minimum # possibilities)
        NodeDesc min = NoNode;
        size_t min_poss = size_N;
        for(NodeDesc u: N1.nodes()){
          const size_t np = num_poss(u);
          if((np != 1) && (np < min_poss)){
            min_poss = np;
            min = u;
          }
        }

        if(min_poss != 1) {
          DEBUG4(std::cout << "branching on vertex "<<min<<std::endl);
          // spawn a new checker for each possibility
          for(const NodeDesc min2: mapping.at(min)){
            IsomorphismMapper child_im(*this);
            child_im.set_unique_poss(min, min2);
            child_im.mark_update(min, min_poss);
            if(child_im.check_isomorph()) return true;
          }
          // if none of the branches led to an isomorphism, then there is none
          return false;
        } else return true;
          
      } catch(const NoPoss& np){
        DEBUG3(std::cout << np.msg << std::endl);
        return false;
      } catch(const std::out_of_range& oor){
        DEBUG3(std::cout << "unable to map a leaf of N0 to N1: "<< oor.what() << std::endl);
        return false;
      }
    }

  protected:
    void treat_pending_updates()
    {
      DEBUG4(std::cout << update_set.size() <<" updates pending:"<<std::endl);
      DEBUG4(for(const NodeDesc p: update_set) std::cout << p << " "; std::cout<<std::endl);
      while(!update_order.empty()){
        const NodeDesc x = update_order.top().second;
        update_order.pop();
        update_set.erase(x);
        update_poss(x);
      }
    }
    void update_poss(const NodeDesc x1)
    {
      DEBUG5(std::cout << "updating "<<x1<<" whose mapping is ("<<num_poss(x1)<<" possibilities):\n " << to_set<NodeSet>(mapping.at(x1)) << std::endl);
      PossSet possible_nodes(size_N);
      // update children
      if(!N1.is_leaf(x1)){
        for(const NodeDesc x2 : mapping.at(x1))
          for(const NodeDesc i: N2.children(x2)) possible_nodes.set(i);
        for(const NodeDesc i: N1.children(x1)) update_poss(i, possible_nodes);
      }
      // update parents
      possible_nodes.clear();
      for(const NodeDesc x2: mapping.at(x1))
        for(const NodeDesc i: N2.parents(x2)) possible_nodes.set(i);
      for(const NodeDesc i: N1.parents(x1)) update_poss(i, possible_nodes);
    }

    // update possibilities, return whether the number of possibilities changed
    bool update_poss(const NodeDesc x, const PossSet& new_poss)
    {
      const auto [iter, success] = mapping.try_emplace(x, new_poss);
      if(!success){
        // if x had a possibility set before, get this PossSet and intersect new_poss with it
        auto& x_poss = iter->second;
        const size_t old_count = x_poss.size();
        if(old_count != 1){
          DEBUG5(std::cout << "updating poss's of "<< x<<" ("<<old_count<<" poss) from\n " << to_set<NodeSet>(x_poss) << " with\n "<< to_set<NodeSet>(new_poss)<<std::endl);
          intersect(x_poss, new_poss);
          DEBUG5(std::cout << "new possibilities: "<<to_set<NodeSet>(x_poss)<<"\n");
          const size_t new_count = x_poss.size();
          // if something changed, update all parents and children
          if(new_count != old_count){
            if(new_count == 0) throw NoPoss(N1, x);
            mark_update(x, new_count);
            return true;
          } else return false;
        } else {
          DEBUG5(std::cout << "not updating possibilities of "<<x<<" (already unique to "<< to_set<NodeSet>(x_poss)<<")\n");
          if(test(new_poss, front(x_poss)))
            return false;
          else throw NoPoss(N1, x);
        }
      } else {
        // if x did not have a possibility set before, default to "size changed"
        DEBUG5(std::cout << "marking "<<x<<" for update\n");
        mark_update(x, new_poss.size());
        return true;
      }
    }
  };


  template<StrictPhylogenyType NetworkA, StrictPhylogenyType NetworkB>
  IsomorphismMapper<NetworkA, NetworkB>
  make_iso_mapper(const NetworkA& _N1,
                  const NetworkB& _N2,
                  const unsigned char _flags,
                  const LabelMatching<NetworkA, NetworkB>* _lmatch = nullptr)
  {
    if(_lmatch){
      return IsomorphismMapper<NetworkA, NetworkB>(_N1, _N2, *_lmatch, _flags);
    } else return IsomorphismMapper<NetworkA, NetworkB>(_N1, _N2, get_label_matching(_N1, _N2), _flags);
    //} else return IsomorphismMapper<NetworkA, NetworkB>(_N1, _N2, (_lmatch ? *_lmatch : get_label_matching(_N1, _N2)), _flags);
  }
}
