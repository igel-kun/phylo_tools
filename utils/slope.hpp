
/* given a sequene S of numbers, detect and throw away all numbers S[x] for which there are indexes i,j with
 * i <= x <= j and S[i] <= S[x] <= S[j] (or S[i] >= S[x] >= S[j])
 *
 * in other words: if, between index i and j all numbers are between S[i] and S[j], then throw those numbers out
 */

#include <cassert>
#include <tuple>

namespace PT {

  struct SlopeReduction {
    // take the first and past-the-end iterators into the sequence
    // also take iterators to the minimum and maximum elements in the range (recalculate if they are invalid)
    // lets try and lift the restriction to lists - instead of erasing, we'll just not copy
    template<class Container>
    static void apply(Container& c, const auto first, const auto past_end, bool remove_front, bool pivot_on_max = false) {
      if(first != past_end) {
        if(next(first) != past_end) {
          //std::cout << "running on " << Seq(first, past_end) << " (pivot on max: "<<pivot_on_max<<")\n";
          // step 1: find minimum and maximum in the range
          // NOTE: when remove_front is true, then find min/max greedily (last min/max instead of first) by using *_equal comparison
          const auto pivot = pivot_on_max ?
            (remove_front ? std::max_element(first, past_end, std::less_equal()) : std::max_element(first, past_end)) :
            (remove_front ? std::min_element(first, past_end, std::less_equal()) : std::min_element(first, past_end));
          //cout << "found pivot: "<<*pivot<<"\n";
          // step 2: mark everything in between for deletion
          if(remove_front) {
            //std::cout << "removing (exclusively) " << Seq(first, next(pivot)) << "\n";
            c.push_back(*first);
            apply(c, pivot, past_end, true, !pivot_on_max);
          } else {
            assert(past_end != seq.end());
            //std::cout << "removing (exclusively) " << Seq(pivot, next(past_end)) << "\n";
            apply(c, first, pivot, false, !pivot_on_max);
            c.push_back(*pivot);
          }
        } else c.push_back(*first);
      }
    }

    template<class Container>
    static void apply(Container& c) {
      Container x;
      if constexpr (VectorType<Container>) x.reserve(c.size());
      const auto pivot = std::max_element(c.begin(), c.end());
      //cout << "first pivot: "<<*pivot<<"\n";
      treat_seq_n2(x, c.begin(), pivot, false);
      treat_seq_n2(x, pivot, c.end(), true);
      // in rare cases, our pivot was chosen unluckily: [0 10 1 10] --> the first 10 is selected as pivot and the center [10 1] is never removed
      // note that the minimum is always unique (since we're starting with a maximum pivot)
      const auto [unique_min, first_max] = std::minmax_element(x.begin(), x.end());
      const auto last_max = std::max_element(first_max, x.end(), std::less_equal());
      if(unique_min < first_max) {
        x.erase(next(unique_min), last_max);
      } else {
        x.erase(next(first_max), unique_min);
      }
      c = std::move(x);
    }
  };

}
