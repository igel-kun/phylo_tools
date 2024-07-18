
#include "io/io.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"

using namespace PT;

OptionMap options;
void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description[""] = {0,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      \tThis program converts from extended newick to edgelist format.\
      \tThe edge-direction will be preserved and leaf-labels will be appended to the leaf-names.\n\
      \t'file' contains a tree or network in extended newick\n");

  parse_options(argc, argv, description, help_message, options);


  if(options[""].empty()) {
    std::cerr << help_message << std::endl;
    exit(EXIT_FAILURE);
  }
  const std::string& filename = options[""].front();
  if(!file_exists(filename)) {
    std::cerr << filename << " cannot be opened for reading" << std::endl;
    exit(EXIT_FAILURE);
  }
}

using MyNet = DefaultLabeledNetwork<>;

MyNet read_network(){
  assert(!options[""].empty());
  const auto& input_file = options[""].front();

  try{
    std::ifstream in_stream(input_file);
    return parse_newick<MyNet>(in_stream);
  } catch(const std::exception& err){
    std::cerr << "could not read network: "<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}


int main(const int argc, const char** argv) {
  parse_options(argc, argv);

  const MyNet N = read_network();

  std::cout << N << '\n';

  write_edgelist(std::cout, N);
}
