
#include <ranges>

#include "io/newick.hpp"
#include "utils/command_line.hpp"
#include "utils/network.hpp"
#include "utils/token.hpp"
#include "utils/diversity.hpp"

namespace ra = std::ranges;
namespace rv = std::views;

using namespace PT;
using namespace std::literals;

// no node data, but edges are annotated with p, w, and gamma
struct EdgeData {
  float inheritance_prob = 1;
  float weight = 0;
  float gamma;

  // construct from a given string that's been read from the input file
  EdgeData(const std::string_view in) {
    auto iter = mstd::tokenize(in, ",;:"sv).begin();
    if(iter) {
      weight = std::stof(*iter);
      if(++iter)
        inheritance_prob = std::stof(*iter);
    } 
  }

  float score() const { return weight * gamma; }
};
using MyNetwork = DefaultLabeledNetwork<void, EdgeData>;
using MyNode = typename MyNetwork::Node;
using MyEdge = typename MyNetwork::Edge;
using NameToNode = std::unordered_map<std::string, NodeDesc>;

NameToNode name_to_node;

OptionMap options;

void parse_options(const int argc, const char** argv) {
  OptionDesc description;
  description["-v"] = {0,0};
  description["-l"] = {1,1};
  description[""] = {1,2};
  const std::string help_message(std::string(argv[0]) + " <file> <<k> | -l <leaf list>>\n\
      \tLet N be the network described in file, where each leaf is annotated with its taxon name, and each edge uv is annotated with 1-2 floating-point values\
      the first indicating the weight, the (possible) second indicating its inheritence probability p, if any.\n\
      This programm either computes the diversity score of the given leaves (if -l option is present),\
      or computes a set of k leaves maximizing the diversity score (add % to k in order to express a number relative to the total number of leaves, e.g. 25%).\
      See whitepaper [TODO] for definitions.\n\
      FLAGS:\n\
      \t-v\tverbose output, prints network\n\
      \t-l\tcompute the score diversity score for the given list of leaves (comma separated list of taxa, no spaces)\n");

  parse_options(argc, argv, description, help_message, options);

  const std::string filename = options[""].front();
  if(!file_exists(filename)) {
    std::cerr << filename << " cannot be opened for reading" << std::endl;
    exit(EXIT_FAILURE);
  }

  if(!test(options, "-l") && (options[""].size() == 1)) {
    std::cerr << "If you want me to compute a leaf-set maximizing the diversity score,\
      you'll have to give me an upper bound k on the size of said leaf-set.\
      Otherwise, I'll just take all the leaves and that's not what you want is it?\n";
  }
}

float parse_k(const MyNetwork& N, const std::string_view k_str) {
  const char perc = k_str.back();
  if(perc == '%') {
    return stof(k_str) * N.num_leaves() / 100.0f;
  } else {
    return stof(k_str);
  }
}

NameVec parse_leaves(const std::string_view in) {
  NameVec result;
  for(const auto& s: mstd::tokenize(in, ','))
    result.emplace_back(s);
  return result;
}

MyNetwork read_network(const std::string& in) {
  try{
    std::ifstream in_stream{std::string{in}};
    // use the node-creation functor to store the Label-->NodeDesc mapping in name_to_node
    return parse_newick<MyNetwork>(in_stream,
                                    [&](const std::string_view s){
                                      const NodeDesc x = MyNetwork::create_node(std::piecewise_construct_t{}, std::tuple{s});
                                      if(!s.empty()) name_to_node.emplace(s, x);
                                      return x; });
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<in<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}


int main(const int argc, const char** argv) {
  std::cout << "parsing options...\n";
  parse_options(argc, argv);

  std::cout << "reading network...\n";
  MyNetwork N(read_network(options[""][0]));

  if(mstd::test(options, "-v"))
    std::cout << "N: " << std::endl << ExtendedDisplay(N) << std::endl;

  if(test(options, "-l")) {
    const NameVec leaf_names = parse_leaves(options["-l"][0]);
    const auto leaves_range = leaf_names | rv::transform([&](const std::string& lname){ return name_to_node.at(lname); });
    const NodeVec leaves{leaves_range.begin(), leaves_range.end()};
    std::cout << "computing diversity score of leaves " << leaves << '\n';
    // fill the gamma-values in the network using a DiversityCalculator
    calculate_diversity(leaves, N,
        [](const MyEdge& e) -> float& {return e.data().gamma;},
        [](const MyEdge& e) -> float& {return e.data().inheritance_prob;});
    float D;
    for(const auto e: N.edges())
      D += e.data().score();
    std::cout << "score = "<<D<<'\n';
  } else {
    const size_t k = stol(options[""][1]);
    std::cout << "computing optimal diversity score rachable with " << k << " leaves\n";

#warning "TODO: write me"
    throw(std::logic_error{"unimplemented"});
  }
}



