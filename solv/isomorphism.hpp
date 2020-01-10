

#pragma once

#include "utils/utils.hpp"
#include "utils/network.hpp"
#include "utils/vector2d.hpp"
#include "utils/iter_bitset.hpp"
#include "utils/set_interface.hpp"
#include <unordered_set>

#define FLAG_MAP_LEAF_LABELS 0x01
#define FLAG_MAP_TREE_LABELS 0x02
#define FLAG_MAP_RETI_LABELS 0x04
#define FLAG_MAP_ALL_LABELS 0x07


namespace PT{
  struct NoPoss : public std::exception
  {
    const std::string msg;

    NoPoss(const std::string& unmappable_name):
      msg(unmappable_name + " is unmappable\n")
    {}
    const char* what() const throw() {
      return msg.c_str();
    }
  };

  template<class NHList>
  inline uint32_t get_unique_head(const NHList& nh)
  {
    return nh.size() == 1 ? head(nh[0]) : UINT32_MAX;
  }
  template<class NHList>
  inline uint32_t get_unique_tail(const NHList& nh)
  {
    return nh.size() == 1 ? tail(nh[0]) : UINT32_MAX;
  }


  template<class PossSet = std::unordered_bitset, class NetworkA = Network<>, class NetworkB = Network<>>
  class IsomorphismMapper
  {
    using MappingPossibility = std::unordered_map<uint32_t, PossSet>;
    using UpdateSet = std::unordered_bitset;
    using UpdateOrder = std::priority_queue<IndexPair, std::vector<IndexPair>, std::greater<IndexPair> >;

    const NetworkA& N1;
    const NetworkB& N2;
    const LabelMap& lmap;

    const uint32_t size_N;
    uint32_t nr_fix = 0;

    MappingPossibility mapping;

    UpdateSet* update_set;
    UpdateOrder* update_order;
   
    const unsigned char flags;

    // use this to indicate a sanity check failure like:
    // vertices/edges mismatch
    // leaves/vertices unmappable
    // degree distributions don't match
    bool initial_fail; 

    inline std::unordered_bitset* make_new_possset(const std::unordered_bitset* sentinel) const
    {
      return new std::unordered_bitset(size_N);
    }
    inline std::ordered_bitset* make_new_possset(const std::ordered_bitset* sentinel) const
    {
      return new std::ordered_bitset(size_N);
    }
    inline std::unordered_set<uint32_t>* make_new_possset(const std::unordered_set<uint32_t>* sentinel) const
    {
      return new std::unordered_set<uint32_t>();
    }

    inline void mark_update(const uint32_t x, const uint32_t nr_poss)
    {
      if(nr_poss == 1) ++nr_fix;
      if(update_set->insert(x))
        update_order->emplace(nr_poss, x);
    }
    
    // set the unique mapping possibility of x1 to x2; fail if x1 has already been determined to not map to x2
    void set_unique_poss(const uint32_t x1, const uint32_t x2)
    {
      // there's still a bug in the STL that constructs the mapping element, even if it's thrown away because the key already exists
      //const <-- lol C++
      auto hint = mapping.find(x1);
      if(hint == mapping.end()){
        // TODO: WTF C++ why is the hint for emplace_hint not freaking const???
        hint = mapping.emplace_hint(hint, x1, size_N);
        mark_update(x1, 1);
      } else if(contains(hint->second, x2)){
        if(hint->second.size() > 1) mark_update(x1, 1);
        hint->second.clear();
      } else throw NoPoss(N1.get_name(x1) + '[' + std::to_string(x1) + ']');
      hint->second.insert(x2);
    }

    inline const size_t num_poss(const uint32_t x)
    {
      return mapping.at(x).size();
    }


  public:

    ~IsomorphismMapper() { delete update_set; delete update_order; }

    IsomorphismMapper(const NetworkA& _N1,
                      const NetworkB& _N2,
                      const uint32_t _size_N,
                      const LabelMap& _lmap,
                      const unsigned char _flags,
                      const MappingPossibility& _mapping = MappingPossibility()):
      N1(_N1),
      N2(_N2),
      lmap(_lmap),
      size_N(_size_N),
      mapping(_mapping),
      update_set(new UpdateSet(size_N)), // <--- be aware that updated is cleared on copy-construct!!!
      update_order(new UpdateOrder()),
      flags(_flags),
      initial_fail(false)
    {}

    IsomorphismMapper(const NetworkA& _N1,
                      const NetworkB& _N2,
                      const LabelMap& _lmap,
                      const unsigned char _flags):
      IsomorphismMapper(_N1, _N2, _N1.num_nodes(), _lmap, _flags)
    {
      DEBUG3(std::cout << "#nodes: "<<N1.num_nodes()<<" & "<<N2.num_nodes()<<"\t\t#edges: "<<N1.num_edges()<<" & "<<N2.num_edges()<<std::endl;);
      if((N1.num_nodes() == N2.num_nodes()) && (N1.num_edges() == N2.num_edges())){
        try{
          initial_restrict();
          DEBUG3(std::cout << "done initializing, now working on the updates..."<<std::endl);
        } catch(const NoPoss& np){
          DEBUG3(std::cout << np.msg << std::endl);
          initial_fail = true;
        } catch(const std::out_of_range& oor){
          DEBUG3(std::cout << "unable to map a leaf of N0 to N1: "<< oor.what() << std::endl);
          initial_fail = true;
        }
      } else initial_fail = true;        
    }

    IsomorphismMapper(const IsomorphismMapper& _mapper):
      IsomorphismMapper(_mapper.N1, _mapper.N2, _mapper.size_N, _mapper.lmap, _mapper.flags, _mapper.mapping)
    {
    }

    bool node_is_interesting(const uint32_t v) const
    {
      switch(N1.type_of(v)){
        case NODE_TYPE_LEAF:
          return (flags & FLAG_MAP_LEAF_LABELS);
        case NODE_TYPE_TREE:
          return (flags & FLAG_MAP_TREE_LABELS);
        case NODE_TYPE_RETI:
          return (flags & FLAG_MAP_RETI_LABELS);
        default:
          return true;
      }
    }

    // NOTE: this assumes to be called before any deduction about the mapping is made!
    template<class NodeFactory>
    void mapping_from_labels(const NodeFactory& fac)
    {
      for(const LabeledNode& lv: fac){
        const uint32_t v1 = lv.first;
        const std::string& name = lv.second;
        if((name != "") && ((flags == FLAG_MAP_ALL_LABELS) || node_is_interesting(v1))){
          const uint32_t v2 = lmap.at(name).second;
          DEBUG4(std::cout << "node "<<v1<<" with label "<<name<<" (same as "<<v2<<")"<<std::endl);
          if(v2 != NO_LABEL)
            set_unique_poss(v1, v2);
          else
            throw NoPoss(name + '[' + std::to_string(v1) + ']');
        }
      }
    }

    void initial_restrict()
    {
      DEBUG3(std::cout << "mapping from labels..."<<std::endl);
      if(flags == FLAG_MAP_LEAF_LABELS)
        mapping_from_labels(N1.get_leaves_labeled());
      else
        mapping_from_labels(N1.get_nodes_labeled());

      // all updated nodes have been fixed
      treat_updated();
      
      if(nr_fix < size_N){
        std::unordered_map<uint64_t, PossSet> degree_to_possibilities;
        std::unordered_map<uint64_t, uint32_t> degree_distribution_N2;

        DEBUG3(std::cout << "restricting degrees..."<<std::endl);
        for(uint32_t u2 = 0; u2 < size_N; ++u2){
          const uint64_t u2_deg = (((uint64_t)N2.in_degree(u2)) << 32) | N2.out_degree(u2);          
          degree_to_possibilities.emplace(u2_deg, size_N).first->second.insert(u2);
          ++degree_distribution_N2[u2_deg];
        }
        for(uint32_t u = 0; u < size_N; ++u){
          const uint64_t u_deg = (((uint64_t)N1.in_degree(u)) << 32) | N1.out_degree(u);
          // find the set of vertices in N2 with this degree
          const auto deg_it = degree_to_possibilities.find(u_deg);
          if(deg_it == degree_to_possibilities.end())
            throw NoPoss(N1.get_name(u) + '[' + std::to_string(u) + ']');
          update_poss(u, deg_it->second);
          
          // decrement the count of vertices in N2 with this degree and fail if this would go below 0
          uint32_t& u_deg_in_N2 = degree_distribution_N2[u_deg];
          if(u_deg_in_N2 == 0)
            throw NoPoss("<vertices of indeg " + std::to_string(N1.in_degree(u)) + " & outdeg " + std::to_string(N1.out_degree(u)) + ">");
          else
            --u_deg_in_N2;
        }
      }
    }

    void treat_updated()
    {
      DEBUG4(std::cout << update_set->size() <<" updates pending:"<<std::endl);
      DEBUG4(for(const auto& p: *update_set) std::cout << p << " "; std::cout<<std::endl);
      while(!update_order->empty()){
        const uint32_t x = update_order->top().second;
        update_order->pop();
        update_set->erase(x);
        update_neighbors(x);
      }
    }

    bool check_isomorph()
    {
      // if we determined already that those will not be isomorphic, just fail here too
      if(initial_fail) return false;

      try{
        treat_updated();

        DEBUG5(std::cout << "possibilities are:" << std::endl;
            for(uint32_t u = 0; u < size_N; ++u){
              std::cout << u << "\t"<< to_set(mapping.at(u)) << std::endl;
            })
        DEBUG3(std::cout << "no more pending updated"<<std::endl);

        // find a vertex to branch on (minimum # possibilities)
        uint32_t min = 0;
        uint32_t min_poss = UINT32_MAX;
        for(uint32_t i = 0; i < size_N; ++i){
          const uint32_t np = num_poss(i);
          if((np > 1) && (np < min_poss)){
            min_poss = np;
            min = i;
          }
        }

        if(min_poss != UINT32_MAX) {
          DEBUG4(std::cout << "branching on vertex "<<min<<std::endl);
          // spawn a new checker for each possibility
          for(uint32_t min2 : mapping.at(min)){
            IsomorphismMapper child_im(*this);
            child_im.set_unique_poss(min, min2);
            child_im.mark_update(min, min_poss);
            if(child_im.check_isomorph()) return true;
          }
          // if none of the branches led to an isomorphism, then there is none
          return false;
        } else
          return true;
          //return check_mapping();
          //NOTE: check_mapping() is unneccessary since forall x, deg(x) == deg(mapping[x]) & each child of x is mapped to a child of mapping[x]
          
      } catch(const NoPoss& np){
        DEBUG3(std::cout << np.msg << std::endl);
        return false;
      } catch(const std::out_of_range& oor){
        DEBUG3(std::cout << "unable to map a leaf of N0 to N1: "<< oor.what() << std::endl);
        return false;
      }
    }

  protected:

    void update_fixed(const uint32_t x1)
    {
      const uint32_t x2 = mapping.at(x1).front();
      PossSet* const possible_nodes = make_new_possset((PossSet*)nullptr);
      // update children
      if(!N1.is_leaf(x1)){
        for(uint32_t i: N2.children(x2)) possible_nodes->insert(i);
        for(uint32_t i: N1.children(x1)) update_poss(i, *possible_nodes);
      }
      // update parents
      possible_nodes->clear();
      for(uint32_t i: N2.parents(x2)) possible_nodes->insert(i);
      for(uint32_t i: N1.parents(x1)) update_poss(i, *possible_nodes);
      delete possible_nodes;
    }

    void update_non_fixed(const uint32_t x1)
    {
      PossSet* const possible_nodes = make_new_possset((PossSet*)nullptr);
      // update children
      if(!N1.is_leaf(x1)){
        for(uint32_t x2 : mapping.at(x1))
          for(uint32_t i: N2.children(x2))
            possible_nodes->insert(i);
        for(uint32_t i: N1.children(x1))
          update_poss(i, *possible_nodes);
      }
      // update parents
      if(!N1.is_root(x1)){
        possible_nodes->clear();
        for(uint32_t x2: mapping.at(x1)){
          for(uint32_t i: N2.parents(x2))
            possible_nodes->insert(i);
        }
        for(uint32_t i: N1.parents(x1))
          update_poss(i, *possible_nodes);
      }
      delete possible_nodes;
    }

    void update_neighbors(const uint32_t x)
    {
      DEBUG5(std::cout << "updating "<<x<<" whose mapping is ("<<num_poss(x)<<" possibilities):\n " << to_set(mapping.at(x)) << std::endl);

      // TODO: use a functor to stop code duplication!
      if(num_poss(x) == 1) // if x maps uniquely and has a unique child, then we can use the cheaper version of update_poss()
        update_fixed(x);
      else
        update_non_fixed(x);
    }

    // update possibilities, return whether the number of possibilities changed
    void update_poss(const uint32_t x, const PossSet& new_poss)
    {
      auto x_poss = mapping.find(x);
      if(x_poss == mapping.end()){
        DEBUG5(std::cout << "setting poss's of "<< x<<" to\n "<< to_set(new_poss)<<std::endl);
        x_poss = mapping.emplace_hint(x_poss, x, new_poss);
        mark_update(x, new_poss.size());
      } else {
        const uint32_t old_count = num_poss(x);
        if(old_count > 1){
          DEBUG5(std::cout << "updating poss's of "<< x<<" ("<<num_poss(x)<<" poss) from\n " << to_set(mapping.at(x)) << " with\n "<< to_set(new_poss)<<std::endl);
          intersect(x_poss->second, new_poss);
          const uint32_t new_count = num_poss(x);
          // if something changed, update all parents and children
          if(new_count > 0){
            if(new_count != old_count)
              mark_update(x, new_count);
          } else throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
        } else if(!contains(new_poss, front(x_poss->second)))
          throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
        DEBUG5(std::cout << "done updating "<<x<<" (now "<<num_poss(x)<<" poss: "<< to_set(mapping.at(x))<<")"<<std::endl);
      }
    }
  };


  template<class PossSet = std::unordered_bitset, class NetworkA = Network<>, class NetworkB = Network<>>
  IsomorphismMapper<PossSet, NetworkA, NetworkB>
  make_iso_mapper(
                      const NetworkA& _N1,
                      const NetworkB& _N2,
                      const LabelMap& _lmap,
                      const unsigned char _flags)
  {return IsomorphismMapper<PossSet, NetworkA, NetworkB>(_N1, _N2, _lmap, _flags); }
}
