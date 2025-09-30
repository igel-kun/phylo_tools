
#pragma once

#include "utils/stl_concepts.hpp"
#include "utils/generic_data.hpp" // for parse_variant

#include "common.hpp" // for RowIterator

namespace PT {

  // read features from a feature table line by line with the following arguments:
  // label_to_features: a functor that extracts features from a line of the table
  // forbidden: container of integers indicating columns to skip
  //    (f.ex. col 1 may contain some summary of the features, so we may want to ignore that)
  //    NOTE: col0 always contains the label of the species, so that is ALWAYS ignored and count starts at 1 after
  template<std::invocable<const std::string&> Label2Features,
           mstd::ContainerType ForbiddenCols = std::vector<uint32_t>,
           class RES = std::invoke_result_t<Label2Features, const std::string&>>
    requires (std::is_reference_v<RES>)
  void read_features(std::istream& in, Label2Features&& label_to_features, ForbiddenCols&& forbidden = ForbiddenCols()) {
    for(const auto& line: RowIterFactory(in)) {
      std::stringstream line_in{line};
      size_t col = 0;
      std::string tmp;

      line_in >> tmp; // read the taxon label
      auto& features = label_to_features(tmp); // make a new collection for that taxon
      using FeatList = std::remove_cvref_t<decltype(features)>;
      using Var = typename FeatList::CorrespondingVariant;
      
      // now, read all features described in the row
      while(line_in.good()) {
        line_in >> tmp;
        if(not test(forbidden, ++col)) {
          auto opt = mstd::generic_reader<Var>::parse(tmp);
          if(opt) {
            features.add_feature(*opt);
          } else throw mstd::MalformedInput{"Could not parse '" + tmp +"' into " + mstd::type_name<Var>()};
        }
      }
      DEBUG5(std::cout << "parsed features: "<<features<<'\n');
    }
  }

}
