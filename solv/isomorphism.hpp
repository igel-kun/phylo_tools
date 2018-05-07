

#pragma once

#include "utils/utils.hpp"
#include "utils/network.hpp"
#include "utils/vector2d.hpp"
#include "utils/iter_bitset.hpp"
#include "utils/set_interface.hpp"
#include <unordered_set>

#define FLAG_MAPTHING_LEAF_LABELS 0x01
#define FLAG_MAPTHING_TREE_LABELS 0x02
#define FLAG_MAPTHING_RETI_LABELS 0x04
#define FLAG_MAPTHING_ALL_LABELS 0x07


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


  template<class _Network = Network, class PossSet = std::iterable_bitset>
  class IsomorphismMapper
  {
    typedef PossSet** MappingPossibility;

    const Network& N1;
    const Network& N2;
    const LabelMap& lmap;

    const uint32_t size_N;


    MappingPossibility mapping;
    uint32_t* sizes;
    uint32_t *unique_poss;

    std::iterable_bitset updated;
    
    const unsigned char flags;

    // use this to indicate a sanity check failure like:
    // vertices/edges mismatch
    // leaves/vertices unmappable
    // degree distributions don't match
    bool initial_fail; 

    inline std::iterable_bitset* make_new_possset(const std::iterable_bitset* sentinel) const
    {
      return new std::iterable_bitset(size_N);
    }
    inline std::unordered_set<uint32_t>* make_new_possset(const std::unordered_set<uint32_t>* sentinel) const
    {
      return new std::unordered_set<uint32_t>();
    }

    void print_mapping(const uint32_t x) const
    {
      if(sizes[x] == 1) std::cout << unique_poss[x]; else std::cout << *mapping[x];
    }

  public:

    IsomorphismMapper(const Network& _N1,
                      const Network& _N2,
                      const uint32_t& _size_N,
                      const LabelMap& _lmap,
                      const unsigned char _flags):
      N1(_N1),
      N2(_N2),
      lmap(_lmap),
      size_N(_size_N),
      mapping((PossSet**)calloc(size_N, sizeof(PossSet*))),
      sizes((uint32_t*)calloc(size_N, sizeof(uint32_t))),
      unique_poss((uint32_t*)malloc(size_N * sizeof(uint32_t))),
      updated(size_N), // <--- be aware that updated is cleared on copy-construct!!!
      flags(_flags),
      initial_fail(false)
    {}                

    IsomorphismMapper(const Network& _N1, const Network& _N2, const LabelMap& _lmap, const unsigned char _flags):
      IsomorphismMapper(_N1, _N2, _N1.num_nodes(), _lmap, _flags)
    {
      if((N1.num_nodes() == N2.num_nodes()) && (N1.num_edges() == N2.num_edges())){
        try{
          initial_restrict();
          updated.set_all();
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
      IsomorphismMapper(_mapper.N1, _mapper.N2, _mapper.size_N, _mapper.lmap, _mapper.flags)
    {
      memcpy(sizes, _mapper.sizes, size_N*sizeof(uint32_t));
      memcpy(unique_poss, _mapper.unique_poss, size_N*sizeof(uint32_t));
      for(uint32_t i = 0; i < size_N; ++i)
        if(_mapper.mapping[i]) mapping[i] = new PossSet(*_mapper.mapping[i]);
    }

    ~IsomorphismMapper()
    {
      free(sizes);
      free(unique_poss);
      for(uint32_t i = 0; i < size_N; ++i)
        if(mapping[i]) delete mapping[i];
      free(mapping);
    }

    bool node_is_interesting(const Network::Node& v) const
    {
      switch(v.get_type()){
        case NODE_TYPE_LEAF:
          return (flags & FLAG_MAPTHING_LEAF_LABELS);
        case NODE_TYPE_TREE:
          return (flags & FLAG_MAPTHING_TREE_LABELS);
        case NODE_TYPE_RETI:
          return (flags & FLAG_MAPTHING_RETI_LABELS);
        default:
          return true;
      }
    }

    // update unique possibilities to the parent recursively
    void update_rising(const uint32_t i, const uint32_t i2, std::iterable_bitset& unfixed_N1, std::iterable_bitset& unfixed_N2)
    {
      sizes[i] = 1;
      unique_poss[i] = i2;
      unfixed_N1.erase(i);
      unfixed_N2.erase(i2);
      
      const Network::Node& u = N1[i];
      const uint32_t parent = get_unique_tail(u.in);
      if((parent != UINT32_MAX) && test(unfixed_N1, parent)){
        const Network::Node& u2 = N2[i2];
        const uint32_t parent2 = get_unique_tail(u2.in);
        if(parent2 != UINT32_MAX)
          update_rising(parent, parent2, unfixed_N1, unfixed_N2);
        else 
          throw NoPoss(N1.get_name(i) + '[' + std::to_string(i) + ']');
      }

    }
    // NOTE: this assumes to be called before any deduction about the mapping is made!
    template<class NodeFactory>
    void mapping_from_labels(const NodeFactory& fac, std::iterable_bitset& unfixed_N1, std::iterable_bitset& unfixed_N2)
    {
      for(const LabeledNode& lv: fac){
        const uint32_t i_idx = lv.first;
        const std::string& name = lv.second;
        if((name != "") && ((flags == FLAG_MAPTHING_ALL_LABELS) || node_is_interesting(N1[i_idx]))){
          const uint32_t i2_idx = lmap.at(name).second;
          DEBUG4(std::cout << "treating "<<i_idx<<" with label "<<name<<" - it's counter part is "<<i2_idx<<std::endl);
          // if the label doesn't exist in N1, i2_idx will be NO_LABEL
          if(i2_idx != NO_LABEL)
            update_rising(i_idx, i2_idx, unfixed_N1, unfixed_N2);
          else
            throw NoPoss(name + '[' + std::to_string(i_idx) + ']');
        }
      }
    }

    void initial_restrict()
    {
      DEBUG3(std::cout << "mapping from labels..."<<std::endl);
      // save which nodes we didn't treat in N2
      std::iterable_bitset unfixed_N2(size_N);
      std::iterable_bitset unfixed_N1(size_N);
      unfixed_N1.set_all();
      unfixed_N2.set_all();
      if(flags == FLAG_MAPTHING_LEAF_LABELS)
        mapping_from_labels(N1.get_leaves_labeled(), unfixed_N1, unfixed_N2);
      else
        mapping_from_labels(N1.get_nodes_labeled(), unfixed_N1, unfixed_N2);

      if(!unfixed_N1.empty()){
        DEBUG3(std::cout << "restricting degrees..."<<std::endl);
        std::unordered_map<uint64_t, PossSet> degree_to_possibilities;
        std::unordered_map<uint64_t, uint32_t> degree_distribution_N2;
        for(uint32_t i: unfixed_N2){
          const Network::Node& u2(N2[i]);
          const uint64_t u2_deg = (((uint64_t)u2.in.size()) << 32) | u2.out.size();
          const auto deg_it = degree_to_possibilities.find(u2_deg);
          if(deg_it == degree_to_possibilities.end())
            degree_to_possibilities.emplace_hint(deg_it, u2_deg, size_N)->second.insert(i);
          else
            deg_it->second.insert(i);
          degree_distribution_N2[u2_deg]++;
        }
        for(uint32_t i: unfixed_N1){
          const Network::Node& u(N1[i]);
          const uint64_t u_deg = (((uint64_t)u.in.size()) << 32) | u.out.size();
          // find the set of vertices in N2 with this degree
          const auto deg_it = degree_to_possibilities.find(u_deg);
          if(deg_it == degree_to_possibilities.end())
            throw NoPoss(N1.get_name(i) + '[' + std::to_string(i) + ']');
          // set the mapping and its size
          sizes[i] = deg_it->second.size();
          if(sizes[i] == 1)
            unique_poss[i] = front(deg_it->second);
          else
            mapping[i] = new PossSet(deg_it->second);
          
          // decrement the count of vertices in N2 with this degree and fail if this would go below 0
          uint32_t& u_deg_in_N2 = degree_distribution_N2[u_deg];
          if(u_deg_in_N2 == 0)
            throw NoPoss("<vertices of indeg " + std::to_string(u.in.size()) + " & outdeg " + std::to_string(u.out.size()) + ">");
          else
            --u_deg_in_N2;
        }
      }
      DEBUG3(std::cout << "done initializing, now working on the updates..."<<std::endl);
      updated.set_all();
    }

/*
    // check if mapping is now an isomorphism
    bool check_mapping() const
    {
      DEBUG3(std::cout << "checking the mapping" <<std::endl);
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        assert(sizes[u_idx] == 1);
        const uint32_t u2_idx = unique_poss[u_idx];
        
        const Network::Node& u(N1.get_vertex(u_idx));
        for(uint32_t j = 0; j < u.out.size(); ++j){
          if(!N2.is_edge(u2_idx, unique_poss[u.out[j]]))
            return false;
        }
      }
      return true;
    }
*/

    bool check_isomorph()
    {
      // if we determined already that those will not be isomorphic, just fail here too
      if(initial_fail) return false;

      try{
        while(!updated.empty()){
          DEBUG4(std::cout << "updates pending:\n" << updated<<std::endl);
          update_neighbors(updated.front());
        }

        DEBUG3(std::cout << "no more pending updated"<<std::endl);
        DEBUG5(std::cout << "possibilities are:" << std::endl;
            for(uint32_t u = 0; u < size_N; ++u){
              std::cout << u << "\t"; print_mapping(u); std::cout<<std::endl;
            })

        // find a vertex to branch on (minimum # possibilities)
        uint32_t min_idx = 0;
        uint32_t min_poss = UINT32_MAX;
        for(uint32_t i = 0; i < size_N; ++i){
          const uint32_t num_poss = sizes[i];
          if((num_poss > 1) && (num_poss < min_poss)){
            min_poss = num_poss;
            min_idx = i;
          }
        }

        if(min_poss != UINT32_MAX) {
          DEBUG4(std::cout << "branching on vertex "<<min_idx<<std::endl);
          // spawn a new checker for each possibility
          for(uint32_t min2_idx : *mapping[min_idx]){
            IsomorphismMapper child_im(*this);
            child_im.sizes[min_idx] = 1;
            child_im.unique_poss[min_idx] = min2_idx;
            child_im.updated.insert(min_idx);
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
      } catch(const std::exception& err){
        std::cout << "unhandled exception: "<<err.what()<<std::endl;
        exit(EXIT_FAILURE);
      }
    }

  protected:

    /*
    void remove_from_everyone_except(const uint32_t idx, const uint32_t except)
    {
      DEBUG5(std::cout << "removing "<<idx<<" from everyone except "<<except<<std::endl);
      for(uint32_t u = 0; u < size_N; ++u)
        if(u != except){
          if(sizes[u] == 1){
            if(unique_poss[u] == idx) throw NoPoss(N1.get_name(u) + '[' + std::to_string(u) + ']');
          } else if(test(mapping[u], idx)){
            mapping[u].clear(idx);
            --sizes[u];
            updated.insert(u);
          }
        }
    }
    */

    void update_children_fixed(const uint32_t x_idx, const Network::Node& x, PossSet*& possible_nodes)
    {
      const uint32_t y_idx = get_unique_head(x.out);
      if(y_idx == UINT32_MAX){
        // update children
        if(possible_nodes) possible_nodes->clear(); else possible_nodes = make_new_possset((PossSet*)nullptr);

        for(uint32_t i: N2[unique_poss[x_idx]].children())
          possible_nodes->insert(i);
        for(uint32_t i: x.children())
          update_poss(i, *possible_nodes);
      } else update_poss(y_idx, N2[unique_poss[x_idx]].out[0].head());
    }

    void update_children_non_fixed(const uint32_t x_idx, const Network::Node& x, PossSet*& possible_nodes)
    {
      for(uint32_t p2 : *mapping[x_idx]){
        for(uint32_t i: N2[p2].children())
          possible_nodes->insert(i);
      }
      for(uint32_t i: x.children())
        update_poss(i, *possible_nodes);
    }

    // TODO: the following functions are the same as the ones before, except that they are using "in" instead of "out" - AVOID THIS CODE DOUBLING
    void update_parents_fixed(const uint32_t x_idx, const Network::Node& x, PossSet*& possible_nodes)
    {
      const uint32_t y_idx = get_unique_tail(x.in);
      if(y_idx == UINT32_MAX){
        if(possible_nodes) possible_nodes->clear(); else possible_nodes = make_new_possset((PossSet*)nullptr);

        for(uint32_t i: N2[unique_poss[x_idx]].parents())
          possible_nodes->insert(i);
        for(uint32_t i: x.parents())
          update_poss(i, *possible_nodes);
      } else {
        const Network::Node& x2 = N2[unique_poss[x_idx]];
        const uint32_t only_parent = tail(x2.in[0]);
        update_poss(y_idx, only_parent);
      }
    }

    void update_parents_non_fixed(const uint32_t x_idx, const Network::Node& x, PossSet*& possible_nodes)
    {
      for(uint32_t p2: *mapping[x_idx]){
        for(uint32_t i: N2[p2].parents())
          possible_nodes->insert(i);
      }
      for(uint32_t i: x.parents())
        update_poss(i, *possible_nodes);
    }

    void update_neighbors(const uint32_t x_idx)
    {
      DEBUG5(std::cout << "updating "<<x_idx<<" whose mapping is: "; print_mapping(x_idx); std::cout << std::endl);
      updated.clear(x_idx);

      PossSet* possible_nodes = nullptr;
      const Network::Node& x(N1[x_idx]);

      // TODO: use a functor to stop code duplication!
      if(sizes[x_idx] == 1){
        // if x_idx maps uniquely and has a unique child, then we can use the cheaper version of update_poss()
        if(!x.out.empty()) update_children_fixed(x_idx, x, possible_nodes);
        update_parents_fixed(x_idx, x, possible_nodes);
        if(possible_nodes) delete possible_nodes;
      } else {
        possible_nodes = make_new_possset((PossSet*)nullptr);
        if(!x.out.empty()) update_children_non_fixed(x_idx, x, possible_nodes);
        update_parents_non_fixed(x_idx, x, possible_nodes);
        delete possible_nodes;
      }
    }

    // update possibilities, return whether the number of possibilities changed
    void update_poss(const uint32_t x, const PossSet& new_poss)
    {
      DEBUG5(std::cout << "updating possibilities of "<< x<<" to\n "; print_mapping(x); std::cout <<" &\n "<<new_poss<<std::endl);
      const uint32_t old_count = sizes[x];
      if(old_count > 1){
        intersect(*mapping[x], new_poss);
        const uint32_t new_count = mapping[x]->size();
        // if something changed, update all parents and children
        if(new_count == 0) throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
        if(new_count != old_count){
          sizes[x] = new_count;
          if(new_count == 1) unique_poss[x] = front(*mapping[x]);
          // if we just fixed x, remove x from everyone else
  // NOTE: this might be too expensive without a mapping of N2 -> N1
  //        if(new_count == 1)
  //          remove_from_everyone_except(front(mapping[x]), x);
          updated.insert(x);
        }
      } else if(!test(new_poss, unique_poss[x])) throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
    }
    // fix mapping of x to y, return whether the number of possibilities changed
    void update_poss(const uint32_t x, const uint32_t y)
    {
      DEBUG5(std::cout << "fixing "<< x<<" to "<< y <<std::endl);
      if(sizes[x] > 1){
        if(test(*mapping[x], y)){
          sizes[x] = 1;
          unique_poss[x] = y;
//          remove_from_everyone_except(y, x);
          updated.insert(x);
        } else throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
      } else if(unique_poss[x] != y) throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
    }

  };

}
