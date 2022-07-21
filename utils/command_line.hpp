
#pragma once

#include <unistd.h>
#include <string>
#include <vector>
#include "utils.hpp"
#include "linear_interval.hpp"

namespace PT{

  // for each option, tell me the min and max number of option parameters
  typedef std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> OptionDesc;
  // for each option, a vector of option parameters, the empty string collects all non-option parameters (arguments)
  typedef std::unordered_map<std::string, std::vector<std::string>> OptionMap;

  void parse_options(const int &argc, const char **argv, const OptionDesc& description, const std::string& help_message, OptionMap& options)
  {
    std::vector<std::string>* current_option_vec = &(options[""]);
    uint32_t current_max = UINT32_MAX;

    for(int i=1; i < argc; ++i){
      const std::string current_arg(argv[i]);
      if((current_arg == "-h") || (current_arg == "--help")){
        std::cout << help_message << std::endl;
        exit(EXIT_SUCCESS);
      } else {
        if(current_arg[0] == '-'){
          const auto mm_iter = description.find(current_arg);
          if(mm_iter != description.end()){
            current_option_vec = &(options[current_arg]);
            current_max = mm_iter->second.second;
          } else {
            std::cerr << "unrecognized option: "<<current_arg<<std::endl;
            std::cerr << help_message << std::endl;
            exit(EXIT_FAILURE);
          }
        } else {
          // if the current option has already gotten all its parameters, add the next parameter to the global list
          if(current_option_vec->size() == current_max)
            current_option_vec = &(options[""]);
          current_option_vec->push_back(current_arg);
        }
      }
    }
    // finally, check if everyone has the right number of parameters
    for(const auto& arg_para: options){
      const size_t num_paras = arg_para.second.size();
      const std::pair<uint32_t, uint32_t>& para_bounds = description.at(arg_para.first);
      if((num_paras < para_bounds.first) || (num_paras > para_bounds.second)){
        std::cerr << "option \""<<arg_para.first<<"\" has "<<num_paras<<" parameters (expected between "<<para_bounds.first<<" & "<<para_bounds.second<<")"<<std::endl;
        std::cerr << help_message << std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  // parses an argument with only a subset of valid values, throwing std::invalid_argument if the argument doesn't pass the test(s)
  template<class _ExtractArgFromString, class _ValidityChecker>
  struct ConstraintArgumentParser {
    using ExtractArgFromString = _ExtractArgFromString;
    using ValidityChecker = _ValidityChecker;
    const std::vector<std::string>& arguments;
    ExtractArgFromString extract;
    ValidityChecker check_valid;
    size_t arg_index = 0;

    using T = decltype(extract(std::string{}));

    T parse_next_argument() {
      if(arguments.size() > arg_index) {
        T result = extract(arguments.at(arg_index));
        if(!check_valid(result)) {
          std::cerr << "unexpected argument '"<< arguments.at(arg_index) << "', please see the help screen (--help)\n";
          exit(1);
        } else return result;
      } else {
        std::cerr << "not enough arguments for an option, please see the help screen (--help)\n";
        exit(1);
      }
    }
  };

  struct StringToInt { constexpr auto operator()(const auto& x) const {return std::stoi(x);} };

  template<class T>
  using ProtoConstraintIntParser = ConstraintArgumentParser<StringToInt, mstd::linear_interval<int>>;


  struct ConstraintIntParser: public ProtoConstraintIntParser<mstd::linear_interval<int>> {
    using Parent = ProtoConstraintIntParser<mstd::linear_interval<int>>;
    using Parent::ExtractArgFromString;
    ConstraintIntParser(const std::vector<std::string>& _arguments, const int lo, const int hi):
      Parent{_arguments, ExtractArgFromString{}, mstd::linear_interval<int>{lo, hi}, 0}
    {}
  };

}
