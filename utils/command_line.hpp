
#pragma once
#include <string>
#include "utils.hpp"

namespace TC{

  struct
  {
    std::string input_filename;
    std::string input_filename2;
  } my_options;

  void help_and_exit(const char* progname)
  {
    std::cerr << progname << " <file1> [file2]" << std::endl << "\tfile1 and file2 describe two networks (either file1 contains 2 lines of extended newick or both file1 and file2 describe a network in extended newick or edgelist format)" << std::endl;
    exit(EXIT_FAILURE);
  }

  void parse_args(int argc, const char** argv)
  {
    if(argc < 2) help_and_exit(argv[0]);
    if(!file_exists(argv[1])){
      std::cerr << argv[1] << " could not be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    } else my_options.input_filename = argv[1];

    if(argc > 2){
      if(!file_exists(argv[2]))
        std::cerr << "ignoring "<<argv[2] << " since it could not be opened for reading" << std::endl;
      else
        my_options.input_filename2 = argv[2];
    }
  }
}
