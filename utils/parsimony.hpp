
// this file contains dynamic programming approaches for hardwired/softwired/parental parsimony on networks
// with respect to the node-scanwidth of a given layout
// NOTE: in the worst case, this is equal to the reticulation number of the network (for a trivial layout)

#pragma once

#include <array>
#include "tight_int.hpp"

namespace PT {
  
  // ==================== HARDWIRED PARSIMONY ========================
  // the DP is indexed by a mapping of character-states to incomplete reticulations
  template<size_t num_states>
  struct HW_DP_Entry {
  };

}
