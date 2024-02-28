
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
  description["-k"] = {1,1};
  description[""] = {1,1};
  const std::string help_message(std::string(argv[0]) + " <file1>\n\
      \tfile contains a network (possibly with branchlengths) in extended newick format\n\
      FLAGS:\n\
      \t-k\tbudget for saving species (add a '%' to express relative to number of leaves) [default: 50%]\n\
      \t-v\tverbose output, prints networks\n");

  parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

// inheritence probabilities are stored for a reticulation r; each parent x of r is mapped to the inheritence probability of xr
using InheritenceProbs = std::unordered_map<NodeDesc, std::unordered_map<NodeDesc, float>>;
using Gamma = InheritenceProbs;

float get_p(const InheritenceProbs& p, const NodeDesc u, const NodeDesc v){
  const auto it = p.find(v);
  if(it != p.end()){
    const auto p_of_uv = it->second.find(u);
    if(p_of_uv != it->second.end()) return p_of_uv->second;
  }
  return 1.0f;
}
float pd_score(const MyNetwork& N, const InheritenceProbs& p, const auto& nodes_to_save){
  float result = 0.0f;
  Gamma gamma;
  for(const auto uv: N.edges_postorder()) {
    const auto& [u, v] = uv.as_pair();
    float& current_gamma = gamma[v][u];
    float p_val = 1.0f;
    float tmp = 1.0f;
    if(N.is_reti(v)){
      p_val = get_p(p, u, v);
    } else if(N.is_leaf(v)) {
      p_val = test(nodes_to_save, v) ? get_p(p, u, v) : 0.0f;
    }
    if(!N.is_leaf(v)) {
      for(const NodeDesc w: N.children(v)) {
        tmp *= (1.0f - gamma[w][v]);
        DEBUG2(std::cout << "gamma("<<u<<"->"<<v<<")..."<<tmp<<" (from gamma("<<v<<"->"<<w<<")="<<gamma[w][v]<<")\n");
      }
      tmp = 1 - tmp;
    } 
    current_gamma = tmp * p_val;
    result += current_gamma * uv.data();
    DEBUG2(std::cout << "gamma("<<u<<"->"<<v<<") = "<<current_gamma<<"\t& weight = "<<uv.data()<<'\n');
  }
  std::cout << "PD-score for set "<<nodes_to_save<<": " << result<<'\n';
  return result;
}




InheritenceProbs inheritence_probs;

// this parses the newick properties of a given edge:
// either it's a simple number giving the weight of the edge, or
// it's <weight>;<inheritence probability>
const auto parse_branch_len = [](const NodeDesc u, const NodeDesc v, const std::string_view s){
  // everything before ';' is the branch length
  const auto delim = s.find(';');
  float branch_len;
  if(delim != std::string::npos) {
    const std::string_view br(s.begin(), s.begin() + delim);
    const std::string_view ip(s.begin() + delim + 1, s.end());
    branch_len = std::stof(br);
    DEBUG2(std::cout << "inheritence prob of "<<u<<"->"<<v<<" is "<<ip<<"\n");
    inheritence_probs[v][u] = stof(ip);
  } else branch_len = std::stof(s);
  return typename MyNetwork::Adjacency(v, branch_len);
};

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

float get_k(const unsigned int num_leaves, const std::string_view k_str) {
  const char perc = options["-k"][0].back();
  if(perc == '%') {
    return stof(k_str) * num_leaves / 100.0f;
  } else {
    return stof(k_str);
  }
}


void apply_for_all_subsets(auto it, const auto end_it, NodeSet& S, const unsigned int subset_size, const auto& f) {
  while(it != end_it){
    const NodeDesc u = *it;
    append(S, u);
    if(subset_size > 1)
      apply_for_all_subsets(std::next(it), end_it, S, subset_size - 1, f);
    else f(S);
    erase(S, u);
    ++it;
  }
}

void apply_for_all_subsets(const auto& leaves, const unsigned int subset_size, const auto& f) {
  NodeSet S;
  apply_for_all_subsets(leaves.begin(), leaves.end(), S, subset_size, f);
}

int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  std::ifstream in(options[""][0]);
  MyNetwork N(read_network(in));

  const auto L = N.leaves().to_container();
  const float k = mstd::test(options, "-k") ? get_k(L.size(), options["-k"][0]) : 0.5f * L.size();

  if(mstd::test(options, "-v"))
    std::cout << N << std::endl;
 
  DEBUG2(std::cout << "inheritence probs:\n"; for(auto i: inheritence_probs) std::cout << i.first << ": "<<i.second<<"\n");

#warning "TODO: make a subset-iterator"
  float max_score = 0;
  NodeSet max_set;
  apply_for_all_subsets(L, k, [&](const auto& S){
      const float score = pd_score(N, inheritence_probs, S);
      if(score > max_score) { max_score = score; max_set = S; }; });
  std::cout << "PD-score for "<<k<<" leaves: " << max_score << ", achieved first by "<<max_set<<"\n";
}

