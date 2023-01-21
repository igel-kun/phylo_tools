
/* given a sequene S of numbers, detect and throw away all numbers S[x] for which there are indexes i,j with
 * i <= x <= j and S[i] <= S[x] <= S[j] (or S[i] >= S[x] >= S[j])
 *
 * in other words: if, between index i and j all numbers are between S[i] and S[j], then throw those numbers out
 */

#pragma once

#include <cassert>
#include <tuple>
#include "stl_concepts.hpp"
#include "stl_utils.hpp"

namespace PT {

  struct SlopeReduction {

    // maintain a queue for the pareto-optimal max's and min's
    template<class Container>
    static void apply(Container& c) {
      DEBUG4(std::cout << "SLOPE reduction on " << c << "\n");
      if(c.size() > 1) {
        Container result;
        result.reserve(c.size());
        const auto end = c.end();
        auto iter = c.begin();
        auto max_q = result.emplace(result.end(), *(iter++));
        while((iter != end) && (*iter == *max_q)) iter++;
        if(iter != end) {
          auto min_q = result.emplace(result.end(), *(iter++));
          // go through the rest of c

          // if min > max, we should swap them; in this case, max is last, otherwise min is
          if(*min_q > *max_q) std::swap(min_q, max_q);
          while(iter != end) {
            const auto n = *(iter++);
            //cout << "min: "<<*min_q<<"  max: "<<*max_q<<"\tcurrent seq: "<< result << " + " << n << "\n";
            const auto min_cmp = (n <=> *min_q);
            if(min_cmp <= 0) {
              // if we found a new minimum, then pop back everything up to the current maximum
              result.erase(next(max_q), result.end());
              const auto it = result.emplace(result.end(), n);
              if(min_cmp < 0) min_q = it;
            } else {
              const auto max_cmp = (n <=> *max_q);
              if(max_cmp >= 0) {
                // if we found a new maximum, then pop back everything up to the parent minimum
                result.erase(next(min_q), result.end());
                const auto it = result.emplace(result.end(), n);
                if(max_cmp > 0) max_q = it;
              } else {
                // at this point min_q and max_q are different elements of c and both before current, so current-1 and current-2 exist!
                assert((*min_q < n) && (n < *max_q));
                auto parent = prev(result.end());
                auto grand_parent = prev(parent);
                while((n >= *parent) && (*parent > *grand_parent)) {
                  result.erase(parent);
                  parent = grand_parent;
                  grand_parent = prev(parent);
                }
                while((n <= *parent) && (*parent < *grand_parent)) {
                  result.erase(parent);
                  parent = grand_parent;
                  grand_parent = prev(parent);
                }
                if(n > *parent) {
                  while((n >= *grand_parent) && (parent != min_q)) {
                    result.erase(parent);
                    parent = prev(grand_parent);
                    result.erase(grand_parent);
                    grand_parent = prev(parent);
                  }
                } else {
                  while((n <= *grand_parent) && (parent != max_q)) {
                    result.erase(parent);
                    parent = prev(grand_parent);
                    result.erase(grand_parent);
                    grand_parent = prev(parent);
                  }
                }
                result.push_back(n);
              }
            }
          }
        }
        c = std::move(result);
      }
    }
  };
}


