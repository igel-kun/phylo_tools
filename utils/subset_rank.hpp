
#pragma once

#include <vector>

/* the rank_subset and unrank_subset functions compute a bijection between
 * (a) the size-k subsets of a size-n set and
 * (b) the numbers {0,1,..,n-1}
 */

namespace mstd {
#warning "TODO: precompute double factorials and multiply by powers of two to get factorials"

#ifdef DEBUG
  constexpr int8_t binomials_max = 3;
#else
  constexpr int8_t binomials_max = 67;
#endif

  using BinomialsRow = std::array<uint64_t, binomials_max - 1>;

  // Precompute transposed binomial coefficients: BINOM[k][x] = C(x, k)
  constexpr auto compute_first_binoms() {
    std::array<BinomialsRow, binomials_max - 1> binom{};
    binom[0][0] = 1u; // (2 choose 2)
    for(uint8_t n = 3; n <= binomials_max; ++n)
      binom[0][n-2] = (n - 3) + binom[0][n-3]; // n choose 2 = (n-1 choose 1) + (n-1 choose 2)
    for(uint8_t k = 3; k <= binomials_max; ++k) {
      binom[k-2][k-2] = 1u;
      for(uint8_t n = k + 1; n <= binomials_max; ++n)
        binom[k-2][n-2] = binom[k-3][n-3] + binom[k-2][n-3];
    }
    return binom;
  }

  constexpr auto BINOM = compute_first_binoms();

  constexpr uint64_t compute_binom(const uint32_t n, const uint32_t k) {
    assert(n >= k);
    if(k < n/2) {
      switch(k) {
        case 0: return 1u;
        case 1: return n;
        default: {
          if(n > binomials_max) {
            return (((compute_binom(n - 2, k - 2) * (n - 1)) / (k-1)) * n) / k;
          } else return BINOM[k - 2][n - 2];
        }
      }
    } else return compute_binom(n, n - k);
  }
  
  
  // Rank a sorted subset
  uint64_t rank_subset(const auto& subset) {
    assert(std::ranges::is_sorted(subset));
    uint64_t rnk = 0;
    uint32_t i = 0;
    for(const auto& x: subset)
      rnk += compute_binom(x, ++i);
    return rnk;
  }

  // Unrank: given rank r, return sorted subset of size 'subset_size' of the set {0,1,...,n-1}
  std::vector<uint8_t> unrank_subset(uint64_t rnk, const uint32_t subset_size, uint32_t n) {
    assert(subset_size <= n);

    std::vector<uint8_t> subset(subset_size);
    for(uint32_t idx = subset_size; idx != 0; --idx) {
      uint32_t lo = 0, hi = n;
      while(lo < hi) {
        uint32_t mid = (lo + hi + 1) / 2;
        if(compute_binom(mid, idx) <= rnk) {
          lo = mid;
        } else hi = mid - 1;
      }
      subset[idx - 1] = lo;
      rnk -= compute_binom(lo, idx);
      n = lo - 1;
    }
    return subset;
  }

}
