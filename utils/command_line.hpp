
#pragma once

#include <unistd.h>
#include <string>
#include <vector>
#include "utils.hpp"

namespace TC{

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
          const OptionDesc::const_iterator mm_iter = description.find(current_arg);
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
    // finally, check if everyone has its minimum number of parameters
    for(const auto& arg_para: options){
      const size_t num_paras = arg_para.second.size();
      const size_t min_paras = description.at(arg_para.first).first;
      if(num_paras < min_paras){
        std::cerr << "option \""<<arg_para.first<<"\" has only "<<num_paras<<" parameters (expected at least "<<min_paras<<")"<<std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

}
