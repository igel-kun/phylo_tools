
#include <cstdint> // uint32_t

#include "utils/command_line.hpp"
#include "utils/random.hpp"
#include "utils/union_find.hpp"

#include "io/newick.hpp"
#include "utils/network.hpp"

using namespace PT;

// float with custom default value
template<float _default = -1.0f>
struct MyFloat {
  float _data;

  MyFloat(): _data{_default} {}
  MyFloat(const float init): _data{init} {}
  operator float() const { return _data; }
};


using MyNetwork = DefaultLabeledNetwork<void, MyFloat<-1.0f>>;

mstd::OptionMap options;

void parse_options(const int argc, const char** argv)
{
  mstd::OptionDesc description;
  description["-v"] = {0,0};
  description["-i"] = {0,0};
  description["-z"] = {0,0};
  description["-u"] = {0,0};
  description["-h"] = {1,1};
  description[""] = {1,1};
  const std::string help_message(std::string(argv[0]) + " <file>\n\
      This program will add (random) branch-lengths to an input network given in extended newick format in <file>.\n\
      The input network may already contain some branch-lengths, in which case those are kept (unless -i is specified).\n\n\
      FLAGS:\n\
      \t-i\tignore branch-lengths in the input\n\
      \t-h <float>\tspecify the total length of the longest root-leaf path (height of the network) [default=1]\n\
      \t\t\tNOTE: if the network already determines its height and -i is not present, then -h is ignored!\n\
      \t-z\tall reticulation-edges shall have length zero (unless otherwise specified in the input and -i is not present)\n\
      \t-u\tmake sure the result is ultrametric (all root-leaf-paths have the same lengths)\n\
      \t-v\tverbose output, prints networks\n");

  mstd::parse_options(argc, argv, description, help_message, options);

  for(const std::string& filename: options[""])
    if(!file_exists(filename)) {
      std::cerr << filename << " cannot be opened for reading" << std::endl;
      exit(EXIT_FAILURE);
    }
}

MyNetwork read_network(const std::string& in) {
  // The Newick parser will only pass a single string_view to the construction, from which we can extract what we wwant by hand.
  // Here, we could just as easily add a constructor taking std::string_view to MyFloat, but this is more pedagogical.
  const auto parse_branch_len = [&](const std::string_view s){ return (mstd::test(options, "-i")) ? -1.0f : std::stof(s); };

  DEBUG1(std::cout << "reading network..."<<std::endl);
  try{
    std::ifstream in_stream{std::string{in}};
    MyNetwork N(parse_newick<MyNetwork>(in_stream, Ex_edge_data{}, parse_branch_len));
    return N;
  } catch(const std::exception& err){
    std::cerr << "could not read a network from "<<options[""][0]<<":\n"<<err.what()<<std::endl;
    exit(EXIT_FAILURE);
  }
}

float get_length(const auto& uv) { return uv.data(); }
bool has_length(const auto& uv) { return get_length(uv) != -1; }

using FloatInterval = mstd::linear_interval<float>;

// We allow some branches to have prescribed lengths, all others are called 'free'.
// Let N' be the result of removing all free branches.
// We will consider the (weakly) connected components in N' seperately, checking if they are realizable.
// Since all free branches are elastic, we can just go bottom-up on the DAG of connected components.
struct WeakComponent {
  // we use (b-a)/x as the std. deviation of a normal distribution on the interval [a,b] where x has to be chosen;
  //    x = 6 seems reasonable, but maybe make this configurable?
  static constexpr float normal_distribution_deviation_quotient = 6;

  // store heights relative to an arbitrary point (first node added to the component)
  NodeMap<float> relative_heights;
  uint32_t max_num_comps_above = 0;
  FloatInterval possible_reference_heights = {0,1};

  WeakComponent() = default;
  WeakComponent(const NodeDesc x): relative_heights{{x, 0}} {};

  // for a given node v, return the interval of possible heights (determined from the possible heights of the component)
  auto get_node_height_bounds(const NodeDesc v) const {
    return possible_reference_heights + relative_heights.at(v);
  }
  void update_node_height_bounds(const NodeDesc v, const FloatInterval new_bounds) {
    const auto new_reference_bounds = new_bounds - relative_heights.at(v);
    possible_reference_heights.intersect(new_reference_bounds);
    if(possible_reference_heights.empty())
      throw std::logic_error{"the input network contains conflicting branch-lengths"};
  }
  // to fix a height, we divide the range of possible positions into (#comps-above + 1) parts
  //    and pick the height from a normal distribution within the lowest part
  void fix_random_height() {
    auto interval = possible_reference_heights;
    if(not interval.empty()) {
      assert(max_num_comps_above != 0); // don't call this for the root component -- it's height is already fixed!
      // divide the range
      interval.high() = interval.low() + interval.size() / max_num_comps_above;
      DEBUG3(std::cout << "blob "<<relative_heights<<" has "<<max_num_comps_above<<" components above us and our height-range is "<<possible_reference_heights<<" so we're picking from "<<interval<<'\n');
      // pick random value around the average of the divided range
      std::normal_distribution<float> bell_curve(interval.average(),  interval.size() / normal_distribution_deviation_quotient);
      const float comp_height = std::clamp(bell_curve(mstd::rand_engine), interval.low(), interval.high());
      // set height of v's component
      possible_reference_heights = comp_height;
    } else throw std::logic_error{"the input network contains conflicting branch-lengths"};
  }
  
  void clear() { relative_heights.clear(); }
};
// keep track of which connected component a node is in, using a union-find structure of indices into the components vector
mstd::DisjointSetForest<NodeDesc, WeakComponent> components;

// merge two components along an edge uv
// NOTE: if we're scoring ultrametrics, then the leaves all get fixed height 0
void make_components(const MyNetwork& N) {
  for(const auto uv: N.edges_postorder()) {
    const auto [u, v] = uv.as_pair();
    // create the component of u and v, if necessary
    const auto [u_iter, u_emplaced] = components.emplace_set(u, u);
    const auto [v_iter, v_emplaced] = components.emplace_set(v, v);
    auto& v_comp = v_iter->second.payload;
    // if v's component was just emplaced, then it's a leaf-component (edges are considered in post-order)
    //    so if we're giving ultrametric lengths, then v gets height 0
    if(v_emplaced) v_comp.update_node_height_bounds(v, {0,0});
    // if uv is a fixed branch (has a length), then merge u's component into v's
    if(has_length(uv)) {
      auto& u_comp = u_iter->second.payload;
      // replace the relative heights in u's component using the relative heights in v's components and the length of uv
      const float offset = u_comp.relative_heights.at(v) - v_comp.relative_heights.at(u) + get_length(uv);
      for(const auto& [x, height]: u_comp.relative_heights)
        v_comp.relative_heights.emplace(x, height + offset);
      // merge the component of u into the component of v in the DSF
      components.merge_sets_keep_order(v, u);
      // clean up
      u_comp.clear();
    }
  }
}

// assign random lengths to branches of the network such as the result is ultrametric, if possible
void add_ultrametric_lengths(const MyNetwork& N, float net_height) {
  const auto different_comps = [&](const auto& xy){ return components.in_different_sets(xy.head(), xy.tail()); };

  // step 1: make components
  make_components(N);

  // from here on, the DSF doesn't change (but the component bounds may), so we can get a stable reference to the root component  
  auto& root_comp = components.set_of(N.root()).payload;

  // step 2: if we have a leaf in the component of the root, then the height of the network is fixed to the height of the root
  bool height_fixed = false;
  const float relative_root_height = root_comp.relative_heights.at(N.root());
  for(const auto& [x, rel_x_height]: root_comp.relative_heights) {
    if(MyNetwork::is_leaf(x)) {
      const float root_height = relative_root_height - rel_x_height;
      // if the height of the root is determined to different values, then we must fail
      if(height_fixed and (root_height != net_height))
        throw std::logic_error{"ultrametric input network implies conflicting heights of the root"};
      height_fixed = true;
      net_height = root_height;
    }
  }
  // finally, fix the height of the root component
  root_comp.update_node_height_bounds(N.root(), {net_height, net_height});

  // step 3: Top-Down: compute (a) upper bounds on heights and (b) maximum number of free edges on paths from and to each component
  for(const auto uv: N.edges_with_preorder(different_comps)) {
    const auto [u, v] = uv.as_pair();
    const auto& u_comp = components.set_of(u).payload;
    auto& v_comp = components.set_of(v).payload;
    // (b) update number of free edges for v
    if(v_comp.max_num_comps_above < u_comp.max_num_comps_above + 1)
      v_comp.max_num_comps_above = u_comp.max_num_comps_above + 1;
    // (a) now, v cannot be higher than u
    const float new_v_upper_bound = u_comp.get_node_height_bounds(u).high();
    v_comp.update_node_height_bounds(v, {0, new_v_upper_bound});
  }

  // step 3: Bottom-Up: (a) compute lower bounds on heights and (b) decide on a random concrete height of each component reference point
  for(const auto uv: N.edges_with_postorder(different_comps)) {
    const auto [u, v] = uv.as_pair();
    auto& u_comp = components.set_of(u).payload;
    auto& v_comp = components.set_of(v).payload;
    // (b) decide on a height for v's component:
    v_comp.fix_random_height();
    // (a) update lower bounds for u's component (u cannot be below v)
    const float new_u_lower_bound = v_comp.get_node_height_bounds(v).low();
    u_comp.update_node_height_bounds(u, {new_u_lower_bound, std::numeric_limits<float>::infinity()});
  }

  // step 4: translate the node-heights into edge-lengths
  for(const auto uv: N.edges()) {
    const auto [u, v] = uv.as_pair();
    const auto u_height = components.set_of(u).payload.get_node_height_bounds(u);
    assert(u_height.low() == u_height.high());
    const auto v_height = components.set_of(v).payload.get_node_height_bounds(v);
    assert(v_height.low() == v_height.high());
    uv.data() = u_height.low() - v_height.high();
  }
}





int main(const int argc, const char** argv)
{
  parse_options(argc, argv);

  const MyNetwork N(read_network(options[""][0]));

  if(mstd::test(options, "-v")) {
    std::cout << N << std::endl;

    std::cout << "input network:\n";
    for(const auto uv: N.edges_preorder()){
      const auto& [u, v] = uv.as_pair();
      std::cout << "branch "<<u<<"["<<N.label(u)<<"] -> "<<uv.head()<<"["<<N.label(uv.head())<<"] has length "<<uv.data()<<std::endl;
    }
  }

  add_ultrametric_lengths(N, mstd::test(options, "-h") ? std::stof(options["h"][0]) : 1.0f);

  std::cout << get_extended_newick(N) << '\n';
}

