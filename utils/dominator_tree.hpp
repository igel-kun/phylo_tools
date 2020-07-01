
#pragma once

/* This computes the dominator-tree of a given network
 * using a slight adaptation of the algorithm presented here:
 * https://cs.stackexchange.com/questions/43105/dominator-tree-for-dag
 * loosely speaking, the immediate dominator of a reticulaion is the
 * lowest stable ancestor (LSA) of its parents, which can be pre-computed
 * in a top-down pass
 *
 * The sparse-matrix LCA-approach can be used (on the growing(!) LSA-tree)
 * to answer LCA queries in log(|T|) time
 * (see https://www.geeksforgeeks.org/lca-for-general-or-n-ary-trees-sparse-matrix-dp-approach-onlogn-ologn/)
 */

#include "tree.hpp"

namespace PT{
#warning TODO: writeme

}
