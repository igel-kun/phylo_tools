
#pragma once

#include "utils/types.hpp"

namespace PT {
  //! an exception for problems with the input string
  struct MalformedInput : public std::logic_error {
    using Parent = std::logic_error;
    using Parent::Parent;

    MalformedInput(const std::string_view context, const ssize_t _pos, const std::string _msg = "unknown error"):
      Parent(_msg + " (position " + std::to_string(_pos) + ")" + DEBUG3(" - relevant substring: " + context.substr(_pos)) + "") {}

    MalformedInput(const ssize_t _pos, const std::string _msg = "unknown error"):
      Parent(_msg + " (position " + std::to_string(_pos) + ")") {}
  };

  template<class T>
  concept NetworkParser = requires(T t) { t.parse() -> NodeContainer; };


  // build phylogeny from a string and, optionally, a node- and edge- creation functions (adjacency creation with 1 node!)
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
