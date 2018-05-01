
#include "io/newick.hpp"

#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/generator.hpp"

using namespace TC;
  

OptionMap options;

void parse_options(const int argc, const char** argv)
{
  OptionDesc description;
  description["-v"] = {0,0};
  description["-n"] = {1,1};
  description["-r"] = {1,1};
  description["-l"] = {1,1};
  description["-a"] = {0,0};
  description[""] = {0,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      generate a random binary network and write it to file in extended newick format\n\
      FLAGS:\n\
      \t-v\tverbose output, prints networks\n\
      \t-r\tnumber of reticulations in the network\n\
      \t-l\tnumber of leaves in the network\n\
      \t-n\tnumber of vertices in the network (this is ignored if -r and -l are present)\n\
      \t-a\tappend to file1 instead of replacing its contents\n\
      NOTE: if, of -n, -r, and -l, less than 2 are present, the network is assumed to have ~10% reticulations\n\
      \tn = 99 is assumed if none are present\n");

  parse_options(argc, argv, description, help_message, options);
}

long l_from_nr(const long n, const long r)
{
  if(n % 2 == 0) throw std::logic_error("binary networks must have an odd number of vertices");
  return (n - 2*r + 1) / 2;
}

long n_from_rl(const long r, const long l)
{
  return 2*r + 2*l - 1;
}

long r_from_nl(const long n, const long l)
{
  // this is symmetric to l from n & r
  return l_from_nr(n,l);
}

void get_node_numbers(long& num_vertices, long& num_retis, long& num_leaves)
{
  //NOTE: in a binary network, we have n = t + r + l, but also l + r - 1 = t (together, n = 2t + 1 and n = 2l + 2r - 1)
  const int total_input = contains(options, "-n") + contains(options, "-r") + contains(options, "-");
  try{
    if(total_input == 0){
      num_vertices = 99;
      num_retis = 10;
      num_leaves = l_from_nr(num_vertices, num_retis);
    } else if(total_input == 1){
      // if we only have one input, we assume that 10r = n and, thus, 9r = t + l and l + r - 1 = t (togeher 8r = 2l - 1)
      if(contains(options, "-n")){
        num_vertices = std::stol(options["-n"][0]);
        num_retis = num_vertices / 10;
        num_leaves = l_from_nr(num_vertices, num_retis);
      } else if(contains(options, "-r")){
        num_retis = std::stol(options["-r"][0]);
        num_vertices = 10 * num_retis + 1;
        num_leaves = l_from_nr(num_vertices, num_retis);
      } else {
        num_leaves = std::stol(options["-l"][0]);
        num_retis = (2 * num_leaves - 1) / 8;
        num_vertices = n_from_rl(num_retis, num_leaves);
      }
    } else if(total_input == 2){
      if(!contains(options, "-l")){
        num_retis = std::stol(options["-r"][0]);
        num_vertices = std::stol(options["-n"][0]);
        num_leaves = l_from_nr(num_vertices, num_retis);
      } else if(!contains(options, "-n")){
        num_retis = std::stol(options["-r"][0]);
        num_leaves = std::stol(options["-l"][0]);
        num_vertices = n_from_rl(num_retis, num_leaves);
      } else {
        num_vertices = std::stol(options["-n"][0]);
        num_leaves = std::stol(options["-l"][0]);
        num_retis  = r_from_nl(num_vertices, num_leaves);
      }
    } else {
      num_retis = std::stol(options["-r"][0]);
      num_vertices = std::stol(options["-n"][0]);
      num_leaves = std::stol(options["-l"][0]);
      if(l_from_nr(num_vertices,  num_retis) != num_leaves)
        throw std::logic_error("there is no binary network with "+std::to_string(num_vertices)+" vertices, "+std::to_string(num_retis)+" reticulations and "+std::to_string(num_leaves)+" leaves");
    }
  } catch(const std::invalid_argument& err){
    std::cerr << "problem converting argument to integer: "<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  } catch(const std::logic_error& err){
    std::cerr << "cannot generate such a network: "<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  long num_vertices, num_retis, num_leaves;
  get_node_numbers(num_vertices, num_retis, num_leaves);
  const long num_tree_nodes = num_vertices - num_retis - num_leaves;

  std::cout << "constructing network with "<<num_vertices<<" vertices: "<<num_tree_nodes<<" tree nodes, "<<num_retis<<" reticulations and "<<num_leaves<<" leaves"<<std::endl;

  Edgelist el;
  std::vector<std::string> names;
  generate_random_binary_edgelist(el, names, num_tree_nodes, num_retis, num_leaves, 0);

  DEBUG5(std::cout << "building N from "<<el<< std::endl);
  Network N(el, names);

  if(contains(options, "-v")) std::cout << N << std::endl;

  const std::string nw_string = get_extended_newick(N);
  if(!options[""].empty()){
    std::ofstream out(options[""][0], (contains(options,"-a") ? std::ios::app : std::ios::out));
    out << nw_string;
  } else std::cout << nw_string;

}

