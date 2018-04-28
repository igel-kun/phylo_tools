
#include "io/newick.hpp"
#include "io/edgelist.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/label_map.hpp"
#include "solv/isomorphism.hpp"

using namespace TC;
  

// read a network from an input stream 
void read_newick_from_stream(std::ifstream& in, Edgelist& el, std::vector<std::string>& names)
{
  std::string in_line;
  std::getline(in, in_line);

  NewickParser parser(in_line, names);
  parser.read_tree(el);
}

void read_edgelist_from_stream(std::ifstream& in, Edgelist& el, std::vector<std::string>& names)
{
  EdgelistParser parser(in, names);
  parser.read_tree(el);
}

bool read_from_stream(std::ifstream& in, Edgelist& el, std::vector<std::string>& names)
{
  try{
    DEBUG3(std::cout << "trying to read newick..." <<std::endl);
    read_newick_from_stream(in, el, names);
  } catch(const MalformedNewick& err){
    try{
      in.seekg(0);
      DEBUG3(std::cout << "trying to read edgelist..." <<std::endl);
      read_edgelist_from_stream(in, el, names);
    } catch(const MalformedEdgelist& err){
      return false;
    }
  }
  return true;
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

  Edgelist el[2];
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

  Network N0(el[0], names[0]);
  Network N1(el[1], names[1]);

  if(contains(options, "-v"))
    std::cout << N0 << std::endl << N1 << std::endl;

  LabelMap *lmap = nullptr;
  lmap = build_labelmap(N0, N1, lmap);
  IsomorphismMapper M(N0, N1, *lmap);
  if(M.check_isomorph())
    std::cout << "isomorph!" << std::endl;
  else
    std::cout << "not isomorph!" << std::endl;

  delete lmap;
}

