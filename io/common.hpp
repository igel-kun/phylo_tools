
#pragma once

#include <iostream>

#include "utils/stl_utils.hpp"

#include "utils/types.hpp"
#include "utils/edge_emplacement.hpp"

namespace PT {

  // RowIterators iterate through the lines in a given std::string using std::getline()
  // NOTE: we'll not give out copies of our buffer, just references; if you need to keep it, make your own copy
  struct RowIterator: 
    public mstd::iter_traits_from_reference<const std::string&> 
  {
    using Traits = mstd::iter_traits_from_reference<const std::string&>;
    std::istream* in = nullptr;
    std::string buffer;

    bool is_valid() const { return (in != nullptr) && (in->good()); }

    RowIterator() = default;
    RowIterator(std::istream& is): in{&is} { std::getline(*in, buffer); }

    auto& operator++() { std::getline(*in, buffer); return *this; }
    auto operator++(int) { auto result{*this}; ++(*this); return result; }

    reference operator*() const { return buffer; }
  };

  using RowIterFactory = mstd::IterFactory<RowIterator>;





  template<class T>
  concept NetworkParser = requires(T t) { t.parse() -> NodeContainer; };

  // build phylogeny from a stringlike or istream and, optionally, a set of initial parameters for creating an EdgeEmplacer
  template<PhylogenyType Phylo, template<class> class Parser, class Instream, class... Args>
    requires mstd::is_derived_from_template_v<std::remove_cvref_t<Instream>, std::basic_istream>
  Phylo parse_network(Instream&& in, Args&&... args) {
    Phylo N;
    Parser(in, EdgeEmplacers<true>::make_emplacer(N, std::forward<Args>(args)...)).parse();
    return N;
  }

  template<class Network,
           template<class> class Parser,
           mstd::Stringlike STR,
           class... Args>
  Network parse_network(STR&& in, Args&&... args) {
    return parse_network<Network, Parser>(std::istringstream(std::forward<STR>(in)), std::forward<Args>(args)...);
  }

}
