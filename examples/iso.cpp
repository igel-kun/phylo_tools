
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


int main(int argc, const char** argv)
{
  parse_args(argc, argv);
  std::ifstream in(my_options.input_filename);

  Edgelist el[2];
  std::vector<std::string> names[2];

  if(!read_from_stream(in, el[0], names[0])){
    std::cerr << "could not read any network from "<<my_options.input_filename<<std::endl;
    exit(EXIT_FAILURE);
  } else {
    if((in.bad() || in.eof() || !read_from_stream(in, el[1], names[1]))){
      if(my_options.input_filename2 != ""){
        in.close();
        in.open(my_options.input_filename2);
        if(!read_from_stream(in, el[1], names[1])){
          std::cerr << "could not read any network from "<<my_options.input_filename2<<std::endl;
          exit(EXIT_FAILURE);
        }
      } else {
        std::cerr << "could not read 2 networks from "<<my_options.input_filename<<" but no other useable source was found"<<std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  DEBUG5(std::cout << "building N0 from "<<el[0]<< std::endl << "building N1 from "<<el[1]<<std::endl);

  Network N0(el[0], names[0]);
  Network N1(el[1], names[1]);
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

