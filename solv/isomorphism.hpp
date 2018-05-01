

#pragma once

#include "utils/utils.hpp"
#include "utils/network.hpp"
#include "utils/vector2d.hpp"
#include "utils/iter_bitset.hpp"

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


  typedef std::vector<std::iterable_bitset> MappingPossibility;

  class IsomorphismMapper
  {
    const Network& N1;
    const Network& N2;
    const LabelMap& lmap;

    const uint32_t size_N;


    MappingPossibility mapping;

    std::iterable_bitset updated;
    
    const unsigned char flags;

  public:

    IsomorphismMapper(const Network& _N1, const Network& _N2, const LabelMap& _lmap, const unsigned char _flags):
      N1(_N1), N2(_N2), lmap(_lmap), size_N(_N1.get_num_vertices()), mapping(), updated(size_N), flags(_flags)
    {
      mapping.reserve(size_N);
      for(uint32_t i = 0; i < size_N; ++i){
        mapping.emplace_back(size_N);
        mapping.back().set_all();
      }
    }

    IsomorphismMapper(const Network& _N1, const Network& _N2, const LabelMap& _lmap, const MappingPossibility& _mapping, const unsigned char _flags):
      N1(_N1), N2(_N2), lmap(_lmap), size_N(_N1.get_num_vertices()), mapping(_mapping), updated(size_N), flags(_flags)
    {
      assert(mapping.size() == size_N);
      assert((size_N == 0) || (mapping[0].size() >= size_N));
    }

    // check if mapping is now an isomorphism
    bool check_mapping() const
    {
      DEBUG3(std::cout << "checking the mapping" <<std::endl);
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        assert(mapping[u_idx].count() == 1);
        const uint32_t u2_idx = mapping[u_idx].front();
        
        const Network::Vertex& u(N1.get_vertex(u_idx));
        for(uint32_t j = 0; j < u.succ.count; ++j){
          if(!N2.is_edge(u2_idx, mapping[u.succ[j]].front()))
            return false;
        }
      }
      return true;
    }

    bool check_isomorph(const uint32_t update_someone = 0, const bool analyze_degrees = true)
    {
      if(N1.get_num_vertices() != N2.get_num_vertices()) return false;
      if(N1.get_num_edges() != N2.get_num_edges()) return false;

      try{
        
        if(!analyze_degrees){
          const uint32_t root1_idx(N1.get_root());
          if(mapping[root1_idx].count() > 1){
            mapping[root1_idx].clear_all();
            mapping[root1_idx].set(N2.get_root());
            updated.set(root1_idx);
          }
        } else restrict_degrees();

        if(flags == FLAG_MATCHING_LEAF_LABELS)
          mapping_from_labels(N1.get_leaves_labeled());
        else
          mapping_from_labels(N1.get_nodes_labeled());
        
        updated.set(update_someone);

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
        uint32_t min_idx;
        uint32_t min_poss = UINT32_MAX;
        for(uint32_t i = 0; i < size_N; ++i){
          const uint32_t num_poss = mapping[i].count();
          if((num_poss > 1) && (num_poss < min_poss)){
            min_poss = num_poss;
            min_idx = i;
          }
        }

        if(min_poss != UINT32_MAX) {
          DEBUG4(std::cout << "branching on vertex "<<min_idx<<std::endl);
          // spawn a new checker for each possibility
          for(uint32_t min2_idx : mapping[min_idx]){
            IsomorphismMapper child_im(N1, N2, lmap, mapping, flags);
            child_im.mapping[min_idx].clear_all();
            child_im.mapping[min_idx].set(min2_idx);
            if(child_im.check_isomorph(min_idx, false)) return true;
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
    void mapping_from_labels(const NodeFactory& fac){
      DEBUG3(std::cout << "updating from labels"<<std::endl);
      for(const LabeledVertex& lv: fac){
        const uint32_t i_idx = lv.first;
        const std::string& name = lv.second;
        if((name != "") && ((flags == FLAG_MATCHING_ALL_LABELS) || node_is_interesting(N1.get_vertex(i_idx)))){
          const uint32_t i2_idx = lmap.at(name).second;
          DEBUG4(std::cout << "treating "<<i_idx<<" with label "<<name<<" - it's counter part is "<<i2_idx<<std::endl);
          // if the label doesn't exist in N1, i2_idx will be NO_LABEL
          if(i2_idx != NO_LABEL) {
            if(update_poss(i_idx, i2_idx))
              updated.set(i_idx);
          } else throw NoPoss(name + '[' + std::to_string(i_idx) + ']');
        }
      }
    }


    void restrict_degrees()
    {
      DEBUG3(std::cout << "restricting degrees..."<<std::endl);
      std::unordered_map<uint64_t, std::iterable_bitset> degree_to_bitset;
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        const Network::Vertex& u(N2.get_vertex(u_idx));
        const uint64_t u_deg = (((uint64_t)u.pred.count) << 32) | u.succ.count;
        const auto deg_it = degree_to_bitset.find(u_deg);
        if(deg_it == degree_to_bitset.end())
          degree_to_bitset.emplace_hint(deg_it, u_deg, size_N)->second.set(u_idx);
        else
          deg_it->second.set(u_idx);
      }
      for(uint32_t u_idx = 0; u_idx < size_N; ++u_idx){
        const Network::Vertex& u(N1.get_vertex(u_idx));
        const uint64_t u_deg = (((uint64_t)u.pred.count) << 32) | u.succ.count;
        const auto deg_it = degree_to_bitset.find(u_deg);
        if(deg_it == degree_to_bitset.end())
          throw NoPoss(N1.get_name(u_idx) + '[' + std::to_string(u_idx) + ']');

        if(update_poss(u_idx, deg_it->second))
          updated.set(u_idx);
      }
    }

    void remove_from_everyone_except(const uint32_t idx, const uint32_t except)
    {
      DEBUG5(std::cout << "removing "<<idx<<" from everyone except "<<except<<std::endl);
      for(uint32_t u = 0; u < size_N; ++u)
        if((u != except) && (mapping[u].test(idx))){
          mapping[u].clear(idx);
          updated.set(u);
        }
    }

    void update_neighbors(const uint32_t x_idx)
    {
      DEBUG5(std::cout << "updating "<<x_idx<<" whose mapping is:\n"<<mapping[x_idx]<<std::endl);
      updated.clear(x_idx);
      
      // update parents and children
      std::iterable_bitset possible_children(size_N);
      std::iterable_bitset possible_parents(size_N);

      for(uint32_t p2_idx : mapping[x_idx]){
        const Network::Vertex& p2(N2.get_vertex(p2_idx));
        for(uint32_t i = 0; i < p2.succ.count; ++i)
          possible_children.set(p2.succ[i]);
        for(uint32_t i = 0; i < p2.pred.count; ++i)
          possible_parents.set(p2.pred[i]);
      }

      const Network::Vertex& x(N1.get_vertex(x_idx));
      for(uint32_t j = 0; j < x.succ.count; ++j)
        if(update_poss(x.succ[j], possible_children)){
            updated.set(x.succ[j]);
        }
      for(uint32_t j = 0; j < x.pred.count; ++j)
        if(update_poss(x.pred[j], possible_parents)){
            updated.set(x.pred[j]);
        }
    }
    // update possibilities, return whether the number of possibilities changed
    bool update_poss(const uint32_t x, const std::iterable_bitset& new_poss)
    {
      DEBUG5(std::cout << "updating possibilities of "<< x<<" to\n "<<mapping[x]<<" &\n "<<new_poss<<std::endl);
      const uint32_t old_count = mapping[x].count();
      mapping[x] &= new_poss;
      const uint32_t new_count = mapping[x].count();
      // if something changed, update all parents and children
      if(new_count == 0) throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
      if(new_count != old_count){
        // if we just fixed x, remove x from everyone else
        if(new_count == 1)
          remove_from_everyone_except(mapping[x].front(), x);
        return true;
      } else return false;
    }
    // fix mapping of x to y, return whether the number of possibilities changed
    bool update_poss(const uint32_t x, const uint32_t y)
    {
      DEBUG5(std::cout << "fixing "<< x<<" to "<< y <<std::endl);
      if(mapping[x].test(y)){
        if(mapping[x].count() > 1){
          mapping[x].clear_all();
          mapping[x].set(y);
          remove_from_everyone_except(y, x);
          return true;
        } else return false;
      } throw NoPoss(N1.get_name(x) + '[' + std::to_string(x) + ']');
    }

  };

}
