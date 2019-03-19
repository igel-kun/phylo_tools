
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "solv/mul_tree.hpp"
#include "solv/mul_network.hpp"
#include "solv/network.hpp"
#include "utils/command_line.hpp"

using namespace PT;

// read a network from an input stream 
void read_newick_from_stream(std::ifstream& in, EdgeVec& el, std::vector<std::string>& names)
{
  std::string in_line;
  std::getline(in, in_line);

  NewickParser parser(in_line, names);
  parser.read_tree(el);
}

void read_edgelist_from_stream(std::ifstream& in, EdgeVec& el, std::vector<std::string>& names)
{
  EdgeVecParser parser(in, names);
  parser.read_tree(el);
}

bool read_from_stream(std::ifstream& in, EdgeVec& el, std::vector<std::string>& names)
{
  try{
    DEBUG3(std::cout << "trying to read newick..." <<std::endl);
    read_newick_from_stream(in, el, names);
  } catch(const MalformedNewick& err){
    try{
      in.seekg(0);
      DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
      read_edgelist_from_stream(in, el, names);
    } catch(const MalformedEdgeVec& err){
      return false;
    }
  }
  return true;
}


// check containment for a Network or MUL-Tree
bool check_display(Network& N, Tree& T)
{
  if(N.is_multi_labeled()){
    std::cout << "running MUL-mapper"<<std::endl;
    MULNetworkMapper<Network> mapper(N, T);
    std::cout << "constructed mapper, now doing the hard work" << std::endl;
    return mapper.verify_display();
  } else {
    std::cout << "running network mapper"<<std::endl;
    NetworkMapper<Network> mapper(N, T);
    std::cout << "constructed mapper, now doing the hard work" << std::endl;
    return mapper.verify_display();
  }
}

OptionMap options;

void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file1> [file2]\n\tfile1 and file2 describe two networks (either file1 contains 2 lines of extended newick or both file1 and file2 describe a network in extended newick or edgelist format)");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}


int main(const int argc, const char** argv)
{
  parse_options(argc, argv);
  std::ifstream in(options[""][0]);

  EdgeVec el[2];
  std::vector<std::string> names[2];

  if(!read_from_stream(in, el[0], names[0])){
    std::cerr << "could not read any network from "<<options[""][0]<<std::endl;
    exit(EXIT_FAILURE);
  } else {
    if((in.bad() || in.eof() || !read_from_stream(in, el[1], names[1]))){
      if(options[""].size() > 1){
        in.close();
        in.open(options[""][1]);
        if(!read_from_stream(in, el[1], names[1])){
          std::cerr << "could not read any network from "<<options[""][1]<<std::endl;
          exit(EXIT_FAILURE);
        }
      } else {
        std::cerr << "could not read 2 networks from "<<options[""][0]<<" but no other useable source was found"<<std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  DEBUG5(std::cout << "building N0 from "<<el[0]<< std::endl << "building N1 from "<<el[1]<<std::endl);

  // choose which index corresponds to the tree
  const bool tree_index = (el[1].size() == names[1].size() - 1);
  Network N(el[1 - tree_index], names[1 - tree_index]);
  Tree T(el[tree_index], names[tree_index]);

  if(contains(options, "-v"))
    std::cout << N << std::endl << T << std::endl;

  if(check_display(N, T))
    std::cout << "display!" << std::endl;
  else
    std::cout << "no display!" << std::endl;
}

