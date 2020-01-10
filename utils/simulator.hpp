
#pragma once

#include "rw_tree.hpp"
#include "random.hpp"

namespace PT{

  // simulate tree-like evolution with a fixed rate of expected split-off events per unit of time
  // return the root inside the tree and the branch-length of the arc incoming to the root
  std::pair<const MutableTreeWithBranchLengths::Node&, const uint32_t> simulate_tree(
      const double& time_limit,
      const double& events_per_time,
      MutableTreeWithBranchLengths& tree,
      const char& first_species = 'A',
      const uint32_t& experiments_per_time = 100)
  {
    tree.clear();
    if(events_per_time > 0){
      const double events_per_experiment = events_per_time / experiments_per_time;
      const uint32_t experiments_limit = time_limit * experiments_per_time;
      uint32_t experiments = 1;
      do{
        if(toss_coin(events_per_experiment){
          const double time_left = time_limit - ((double)experiments) / experiments_per_time;
          const auto T1 = simulate_tree(time_left, events_per_time, tree, first_species, experiments_per_time);
          const auto T2 = simulate_tree(time_left, events_per_time, tree, first_species + tree.num_leaves(), experiments_per_time);
        } else ++experiments;
      } while(experiments < experiments_limit);
    } else {
      tree.add_leaf(first_species);
    }
  }



}
