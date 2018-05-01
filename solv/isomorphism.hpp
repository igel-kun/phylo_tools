

#pragma once

#include "utils/utils.hpp"
#include "utils/network.hpp"
#include "utils/vector2d.hpp"
#include "utils/iter_bitset.hpp"
#include <unordered_set>

#define FLAG_MATCHING_LEAF_LABELS 0x01
#define FLAG_MATCHING_TREE_LABELS 0x02
#define FLAG_MATCHING_RETI_LABELS 0x04
#define FLAG_MATCHING_ALL_LABELS 0x07

namespace TC{
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



  template<class PossSet = std::iterable_bitset>
  class IsomorphismMapper
  {
    typedef std::vector<PossSet> MappingPossibility;

    const Network& N1;
    const Network& N2;
    const LabelMap& lmap;

    const uint32_t size_N;


    MappingPossibility mapping;
    std::vector<uint32_t> sizes;
    std::vector<uint32_t> unique_poss;

    std::iterable_bitset updated;
    
    const unsigned char flags;

    // use this to indicate a sanity check failure like:
    // vertices/edges mismatch
    // leaves/vertices unmappable
    // degree distributions don't match
    bool initial_fail; 

    inline bool test(const std::iterable_bitset& _set, const uint32_t index) const
    {
      return _set.test(index);
    }
    inline bool test(const std::unordered_set<uint32_t>& _set, const uint32_t index) const
    {
      return contains(_set, index);
    }
    inline std::iterable_bitset* make_new_possset(const std::iterable_bitset* sentinel) const
    {
      return new std::iterable_bitset(size_N);
    }
    inline std::unordered_set<uint32_t>* make_new_possset(const std::unordered_set<uint32_t>* sentinel) const
    {
      return new std::unordered_set<uint32_t>();
    }
    inline void intersect(std::iterable_bitset& target, const std::iterable_bitset& source)
    {
      target &= source;
    }
    inline void intersect(std::unordered_set<uint32_t>& target, const std::unordered_set<uint32_t>& source)
    {
      for(auto target_it = target.begin(); target_it != target.end();)
        if(!contains(source, *target_it))
          target_it = target.erase(target_it); 
        else ++target_it;
    }
    inline uint32_t front(const std::iterable_bitset& _set) const
    {
      return _set.front();
    }
    inline uint32_t front(const std::unordered_set<uint32_t>& _set) const
    {
      return *_set.begin();
    }

  public:

    IsomorphismMapper(const Network& _N1, const Network& _N2, const LabelMap& _lmap, const unsigned char _flags):
      N1(_N1), N2(_N2), lmap(_lmap), size_N(_N1.get_num_vertices()), mapping(), sizes(), unique_poss(), updated(size_N), flags(_flags), initial_fail(false)
    {
      if((N1.get_num_vertices() == N2.get_num_vertices()) && (N1.get_num_edges() == N2.get_num_edges())){
        mapping.reserve(size_N);
        sizes.reserve(size_N);
        unique_poss.reserve(size_N);
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
      N1(_mapper.N1),
      N2(_mapper.N2),
      lmap(_mapper.lmap),
      size_N(_mapper.size_N),
      mapping(_mapper.mapping),
      sizes(_mapper.sizes),
      unique_poss(_mapper.unique_poss),
      updated(size_N),
      flags(_mapper.flags)
    {
      assert(mapping.size() == size_N);
      assert((size_N == 0) || (mapping[0].size() >= size_N));
    }

    void initial_restrict()
    {
      DEBUG3(std::cout << "restricting degrees..."<<std::endl);
      std::unordered_map<uint64_t, PossSet> degree_to_possibilities;
      std::unordered_map<uint64_t, uint32_t> degree_distribution_N2;
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        const Network::Vertex& u(N2.get_vertex(u_idx));
        const uint64_t u_deg = (((uint64_t)u.pred.count) << 32) | u.succ.count;
        const auto deg_it = degree_to_possibilities.find(u_deg);
        if(deg_it == degree_to_possibilities.end())
          degree_to_possibilities.emplace_hint(deg_it, u_deg, size_N)->second.insert(u_idx);
        else
          deg_it->second.insert(u_idx);
        degree_distribution_N2[u_deg]++;
      }
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        const Network::Vertex& u(N1.get_vertex(u_idx));
        const uint64_t u_deg = (((uint64_t)u.pred.count) << 32) | u.succ.count;
        // find the set of vertices in N2 with this degree
        const auto deg_it = degree_to_possibilities.find(u_deg);
        if(deg_it == degree_to_possibilities.end())
          throw NoPoss(N1.get_name(u_idx) + '[' + std::to_string(u_idx) + ']');
        // set the mapping and its size
        mapping.emplace_back(deg_it->second);
        sizes.push_back(deg_it->second.size());
        unique_poss.push_back((sizes.back() == 1) ? front(deg_it->second) : 0);
        // decrement the count of vertices in N2 with this degree and fail if this would go below 0
        uint32_t& u_deg_in_N2 = degree_distribution_N2[u_deg];
        if(u_deg_in_N2 == 0)
          throw NoPoss("<vertices of indeg " + std::to_string(u.pred.count) + " & outdeg " + std::to_string(u.succ.count) + ">");
      }

      if(flags == FLAG_MATCHING_LEAF_LABELS)
        mapping_from_labels(N1.get_leaves_labeled());
      else
        mapping_from_labels(N1.get_nodes_labeled());

    }


    // check if mapping is now an isomorphism
    bool check_mapping() const
    {
      DEBUG3(std::cout << "checking the mapping" <<std::endl);
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        assert(mapping[u_idx].size() == 1);
        const uint32_t u2_idx = front(mapping[u_idx]);
        
        const Network::Vertex& u(N1.get_vertex(u_idx));
        for(uint32_t j = 0; j < u.succ.count; ++j){
          if(!N2.is_edge(u2_idx, front(mapping[u.succ[j]])))
            return false;
        }
      }
      return true;
    }

    bool check_isomorph()
    {
      // if we determined already that those will not be isomorphic, just fail here too
      if(initial_fail) return false;

      try{
        while(!updated.is_empty()){
          DEBUG4(std::cout << "updates pending:\n" << updated<<std::endl);
          update_neighbors(updated.front());
        }

        DEBUG3(
            std::cout << "no more pending updated, possibilities are:" << std::endl;
            for(uint32_t u = 0; u < size_N; ++u)
              std::cout << u << "\t"<<mapping[u]<<std::endl;
            )

        // find a vertex to branch on (minimum # possibilities)
        uint32_t min_idx = 0;
        uint32_t min_poss = UINT32_MAX;
        for(uint32_t i = 0; i < size_N; ++i){
          const uint32_t num_poss = mapping[i].size();
          if((num_poss > 1) && (num_poss < min_poss)){
            min_poss = num_poss;
            min_idx = i;
          }
        }

        if(min_poss != UINT32_MAX) {
          DEBUG4(std::cout << "branching on vertex "<<min_idx<<std::endl);
          // spawn a new checker for each possibility
          for(uint32_t min2_idx : mapping[min_idx]){
            IsomorphismMapper child_im(*this);
            child_im.mapping[min_idx].clear();
            child_im.mapping[min_idx].insert(min2_idx);
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

    bool node_is_interesting(const Network::Vertex& v) const
    {
      switch(v.get_type()){
        case NODE_TYPE_LEAF:
          return (flags & FLAG_MATCHING_LEAF_LABELS);
        case NODE_TYPE_TREE:
          return (flags & FLAG_MATCHING_TREE_LABELS);
        case NODE_TYPE_RETI:
          return (flags & FLAG_MATCHING_RETI_LABELS);
        default:
          return true;
      }
    }

    template<class NodeFactory>
    void mapping_from_labels(const NodeFactory& fac)
    {
      DEBUG3(std::cout << "updating from labels"<<std::endl);
      for(const LabeledVertex& lv: fac){
        const uint32_t i_idx = lv.first;
        const std::string& name = lv.second;
        if((name != "") && ((flags == FLAG_MATCHING_ALL_LABELS) || node_is_interesting(N1.get_vertex(i_idx)))){
          const uint32_t i2_idx = lmap.at(name).second;
          DEBUG4(std::cout << "treating "<<i_idx<<" with label "<<name<<" - it's counter part is "<<i2_idx<<std::endl);
          // if the label doesn't exist in N1, i2_idx will be NO_LABEL
          if(i2_idx != NO_LABEL) {
            update_poss(i_idx, i2_idx);
          } else throw NoPoss(name + '[' + std::to_string(i_idx) + ']');
        }
      }
    }

    void remove_from_everyone_except(const uint32_t idx, const uint32_t except)
    {
      DEBUG5(std::cout << "removing "<<idx<<" from everyone except "<<except<<std::endl);
      for(uint32_t u = 0; u < size_N; ++u)
        if((u != except) && (test(mapping[u], idx))){
          mapping[u].clear(idx);
          --sizes[u];
          updated.insert(u);
        }
    }

    // most of the time, we expect to find only a handful of possible parents/children
    // so it might be faster to do this with a couple of uint32_t's
    void update_neighbors_fixed(const uint32_t x_idx)
    {
      const uint32_t x2_idx = unique_poss[x_idx];
      const Network::Vertex& x(N1.get_vertex(x_idx));
      const Network::Vertex& x2(N2.get_vertex(x2_idx));
      // if we have a single child, then it should map to the single child of x2
      if(x.succ.count == 1){
        update_poss(x.succ[0], x2.succ[0]);
      } else {
      }
    }

    void update_neighbors(const uint32_t x_idx)
    {
      DEBUG5(std::cout << "updating "<<x_idx<<" whose mapping is:\n"<<mapping[x_idx]<<std::endl);
      updated.clear(x_idx);

      PossSet* possible_nodes = nullptr;
      const Network::Vertex& x(N1.get_vertex(x_idx));
      uint32_t y_idx;

      // TODO: use a functor to stop code duplication!
      if(x.succ.count > 0){
        // if x_idx maps uniquely and has a unique child, then we can use the cheaper version of update_poss()
        if((sizes[x_idx] == 1) && ((y_idx = x.succ.get_unique_item()) != UINT32_MAX)){
          update_poss(y_idx, N2[unique_poss[x_idx]].succ[0]);
        } else {
          // update children
          possible_nodes = make_new_possset((PossSet*)nullptr);

          for(uint32_t p2_idx : mapping[x_idx]){
            const Network::Vertex& p2(N2.get_vertex(p2_idx));
            for(uint32_t i = 0; i < p2.succ.count; ++i)
              possible_nodes->insert(p2.succ[i]);
          }
          for(uint32_t j = 0; j < x.succ.count; ++j)
            update_poss(x.succ[j], *possible_nodes);
        }
      }

      // if x_idx maps uniquely and has a unique parent, then we can use the cheaper version of update_poss()
      if((sizes[x_idx] == 1) && ((y_idx = x.pred.get_unique_item()) != UINT32_MAX)){
        update_poss(y_idx, N2[unique_poss[x_idx]].pred[0]);
      } else {
        if(possible_nodes == nullptr) {
          possible_nodes = make_new_possset((PossSet*)nullptr);
        } else possible_nodes->clear();

        for(uint32_t p2_idx : mapping[x_idx]){
          const Network::Vertex& p2(N2.get_vertex(p2_idx));     
          for(uint32_t i = 0; i < p2.pred.count; ++i)
            possible_nodes->insert(p2.pred[i]);
        }
        for(uint32_t j = 0; j < x.pred.count; ++j)
          update_poss(x.pred[j], *possible_nodes);
        delete possible_nodes;
      }
    }
    // update possibilities, return whether the number of possibilities changed
    void update_poss(const uint32_t x, const PossSet& new_poss)
    {
      DEBUG5(std::cout << "updating possibilities of "<< x<<" to\n "<<mapping[x]<<" &\n "<<new_poss<<std::endl);
      const uint32_t old_count = sizes[x];
      intersect(mapping[x], new_poss);
      const uint32_t new_count = mapping[x].size();
      // if something changed, update all parents and children
      if(new_count == 0) throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
      if(new_count != old_count){
        sizes[x] = new_count;
        if(new_count == 1) unique_poss[x] = front(mapping[x]);
        // if we just fixed x, remove x from everyone else
// NOTE: this might be too expensive without a mapping of N2 -> N1
//        if(new_count == 1)
//          remove_from_everyone_except(front(mapping[x]), x);
        updated.insert(x);
      }
    }
    // fix mapping of x to y, return whether the number of possibilities changed
    void update_poss(const uint32_t x, const uint32_t y)
    {
      DEBUG5(std::cout << "fixing "<< x<<" to "<< y <<std::endl);
      if(sizes[x] == 1){
        if(unique_poss[x] == y) return;
        else throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
      } else {
        if(test(mapping[x], y)){
          sizes[x] = 1;
          unique_poss[x] = y;
          mapping[x].clear();
          mapping[x].insert(y);
//          remove_from_everyone_except(y, x);
          updated.insert(x);
        } else throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
      }
    }

  };

}
