
#include "io/newick.hpp"

#include "solv/mul_tree.hpp"
#include "solv/network.hpp"
#include "utils/command_line.hpp"
#include "utils/network.hpp"

using namespace TC;

// read a network from an input stream 
void read_from_stream(std::ifstream& in, NewickParser*& parser, Edgelist& el)
{
  std::string in_line;
  std::getline(in, in_line);

  parser = new NewickParser(in_line, true);
  parser->read_tree(el);
}


// check containment for a Network
bool check_display(const Network& N, const Tree& T)
{
  NetworkMapper mapper(N, T);
  std::cout << "constructed mapper, now doing the hard work" << std::endl;
  return mapper.verify_display();
}

// check containment for a MUL-Tree
bool check_display(Tree& N, Tree& T)
{
  MULTreeMapper<Tree> mapper(N, T);
  std::cout << "constructed mapper, now doing the hard work" << std::endl;
  return mapper.verify_display();
}

int main(int argc, const char** argv)
{
  parse_args(argc, argv);
  std::ifstream in(my_options.input_filename);

  NewickParser* Nparse;
  NewickParser* Tparse;

  Edgelist Nel, Tel;
  read_from_stream(in, Nparse, Nel);
  read_from_stream(in, Tparse, Tel);
  assert(Tparse->is_tree()); // T should always be a tree

  std::cout << "building N from "<<Nel<< std::endl << "building T from "<<Tel<<std::endl;
  if(Nparse->is_tree()){
    Tree N(Nel, Nparse->get_names());
    Tree T(Tel, Tparse->get_names());
    std::cout << N << std::endl << T << std::endl;
    if(check_display(N, T)) std::cout << "DISPLAYS!" << std::endl;
  } else {
    Network N(Nel, Nparse->get_names());
    Tree T(Tel, Tparse->get_names());
    std::cout << N << std::endl << T << std::endl;
    if(check_display(N, T)) std::cout << "DISPLAYS!" << std::endl;
  }

  delete Nparse;
  delete Tparse;
}

