
#pragma once

#include "utils/stl_concepts.hpp"
#include "utils/generic_data.hpp"

#include "utils/features.hpp"

#include "common.hpp"

namespace PT {

  template<std::invocable<const std::string&> Label2Features, mstd::ContainerType ForbiddenCols = std::vector<uint32_t>, class RES = std::invoke_result_t<Label2Features, const std::string&>>
    requires (std::is_reference_v<RES>)
  void read_features(std::istream& in, Label2Features&& label_to_features, ForbiddenCols&& forbidden = ForbiddenCols()) {
    for(const auto& line: RowIterFactory(in)) {
      std::stringstream line_in{line};
      size_t col = 0;
      std::string tmp;
      // we assume that each line starts with the label of the node whose features follow
      line_in >> tmp;
      auto& features = label_to_features(tmp);
      while(line_in.good()) {
        line_in >> tmp;
        if(not test(forbidden, ++col)) {
          const auto v = mstd::parse_variant(tmp);
          // if we read 0/1 then assume it's a binary character
          if(std::holds_alternative<int64_t>(v)) {
            const int64_t value = std::get<int64_t>(v);
            if constexpr (features.template occurs<bool>) {
              if(value < 2) {
                features.template get_by_type<bool>().emplace_back(value);
                continue;
              }
            } 
            if constexpr (features.template occurs<int32_t>) {
              features.template get_by_type<int32_t>().emplace_back(value);
            } else throw mstd::MalformedInput{"parsed integer/boolean feature, but no corresponding feature-list is present (bool or int32_t)"};
          } else if(std::holds_alternative<double>(v)) {
            if constexpr (features.template occurs<double>) {
              features.template get_by_type<double>().emplace_back(std::get<double>(v));
            } else throw mstd::MalformedInput{"parsed floating point feature, but no corresponding feature-list is present (double)"};
          } else if constexpr (features.template occurs<std::string>) {
            features.template get_by_type<std::string>().emplace_back(std::get<std::string_view>(v));
          }
        }
      }
      DEBUG5(std::cout << "parsed features: "<<features<<'\n');
    }
  }

}
