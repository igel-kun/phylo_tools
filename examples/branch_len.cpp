
#include "io/newick.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"

using namespace PT;

using MyNetwork = DefaultLabeledNetwork<void, float>;

OptionMap options;

void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file1>\n\
      \tfile contains a network (possibly with branchlengths) in extended newick format\n\
      FLAGS:\n\
      \t-v\tverbose output, prints networks\n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

const auto parse_branch_len = [](const NodeDesc d, const std::string_view s){ return typename MyNetwork::Adjacency(d, std::stof(s)); };

MyNetwork read_network(std::ifstream& in) {
  std::cout << "reading network..."<<std::endl;
  try{
    MyNetwork N(parse_newick<MyNetwork>(in, parse_branch_len));
    return N;
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  std::ifstream in(options[""][0]);
  MyNetwork N(read_network(in));

  if(mstd::test(options, "-v"))
    std::cout << N << std::endl;
 
  for(const auto uv: N.edges_preorder()){
    const auto& [u, v] = uv.as_pair();
    std::cout << "branch "<<u<<"["<<N.label(u)<<"] -> "<<uv.head()<<"["<<N.label(uv.head())<<"] has length "<<uv.data()<<std::endl;
  }
}

