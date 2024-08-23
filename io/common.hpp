
#pragma once

#include "utils/types.hpp"

namespace PT {

  template<class T>
  concept NetworkParser = requires(T t) { t.parse() -> NodeContainer; };


  // build phylogeny from a string and, optionally, a set of initial parameters for creating an EdgeEmplacer
  template<PhylogenyType Phylo, template<class, bool, bool> class Parser, class Instream, class... Args>
    requires mstd::is_derived_from_template_v<std::remove_cvref_t<Instream>, std::basic_istream>
  Phylo parse_network(Instream&& in, Args&&... args) {
    Phylo N;
    Parser(in, EdgeEmplacers<true>::make_emplacer(N, std::forward<Args>(args)...)).parse();
    return N;
  }

  template<class Network,
           template<class ,bool, bool> class Parser,
           mstd::Stringlike STR,
           class... Args>
  Network parse_network(STR&& in, Args&&... args) {
    return parse_network<Network, Parser>(std::istringstream(std::forward<STR>(in)), std::forward<Args>(args)...);
  }

}
