
#pragma once

#include "set_interface.hpp"

namespace PT {


  // apply a TBR-move to a network N
  // the move consists in 
  //  1. removing the edge uv
  //    NOTE: if this removal disconnects the network, then e and f must be in different components,
  //          otherwise the result remains disconnected (THIS IS NOT VERIFIED!)
  //    NOTE: e must not be below f, otherwise the result is cyclic (THIS IS NOT VERIFIED)
  //  2. subdividing e with a new node x
  //  3. subdividing f with a new node y
  //  4. adding the edge xy
  //  5. if suppress_deg2 is set and u and/or v has degree 2 now, then suppress it
  // NOTE: the node data of both x and y are initialized using the arguments passed after the edges (f.ex. a DataMaker function, see Phylogeny::subdivide_edge)
  template<bool suppress_deg2, PhylogenyType Phylo, class... Args>
  void apply_TBR_move(Phylo& N, const Edge<Phylo>& uv, const Edge<Phylo>& e, const Edge<Phylo>& f, Args&&... args) {
    // remove uv
    const NodeDesc u = uv.tail();
    const NodeDesc v = uv.head();
    assert((N.degree(u) != 1) && (N.degree(v) != 1)); // if u or v has degree 1, we won't be able to reconnect it
    N.remove_edge_no_cleanup(u, v);

    // subdivide e and f with new vertices x and y
    const NodeDesc x = N.subdivide_edge(e, args...);
    const NodeDesc y = N.subdivide_edge(f, args...);

    // reroot the network containing y at y
    N.reroot_no_cleanup(y, true);

    // add the edge x->y
    N.add_edge(x, y, args...);
#error "continue here"
  }

}
