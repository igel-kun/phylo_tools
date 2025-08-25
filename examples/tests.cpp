
#include <optional>
#include <cstdint>
#include <iostream>
#include <utility>

// this runs different tests to see if the low-level stuff is working
// we're testing:
#include "utils/singleton.hpp" //    singleton_set
#include "utils/vector_hash.hpp" //    vector_hash
#include "utils/vector_map.hpp" //    vector_map
#include "utils/optional.hpp" //    optional
#include "utils/subsets.hpp" //    subset iterators
#include "utils/brute_force.hpp" // brute_force
#include "utils/concat_iter.hpp" // concatenating_iterator
#include "utils/generic_data.hpp" // generic data
#include "utils/command_line.hpp"

#include "utils/network.hpp"
#include "utils/tree.hpp"
#include "utils/types.hpp"
#include "utils/biconnected_comps.hpp"
#include "io/newick.hpp"

#ifdef DFSCORO
#include "utils/dfs_coro.hpp"
#else
#include "utils/dfs.hpp"
#endif

#include "utils/dominators.hpp"


constexpr auto mark_network = "(((a:3,(b:2)#H1:2;0.5):2,(#H1:2;0.5,c:3):2):8,x:100);";
constexpr auto mark_network2 = "(((a:3,(b:2)#H1:2;0.5):2,(#H1:2;0.5,(c:3)#H2:10:.9):2):8,(x:100:bla,#H2:8:.1));";
constexpr auto random_net = "((((((((((a:0)#H3:1,(b:2)#H4:2):3)#H2:4,c:4):5)#H1:6)#H0:7,(((#H3:2,d:3):8,(#H4:1,e:9):5):1)#H5:6):1,(#H5:1,(#H1:4,#H2:2):4):1):8,((#H0:1,(((((f:4,g:2):1)#H7:4,(((h:1,i:2):5,(j:1,k:2):4):8,l:1):6):3,(m:2,n:4):5):1,#H7:5):2):4)#H6:7),#H6:2);";
constexpr auto random_net_poly = "((((((((((a:0)#H3:1,(b:2)#H4:2):3)#H2:4,c:4):5)#H1:6)#H0:7,(((#H3:2,d:3):8,(#H4:1,e:9):5):1)#H5:6):1,p:3,q:9,(#H5:1,(#H1:4,#H2:2):4):1):8,((#H0:1,(((((f:4,g:2):1)#H7:4,(((h:1,i:2):5,(j:1,k:2):4):8,l:1):6):3,(m:2,n:4):5):1,#H7:5):2):4)#H6:7),#H6:2);";

// some static tests

static_assert(std::is_default_constructible_v<mstd::optional_by_invalid<int>>);
static_assert(std::is_trivially_destructible_v<mstd::optional_by_invalid<int>>);
static_assert(std::is_trivially_copyable_v<mstd::optional_by_invalid<int>>);
static_assert(std::is_trivially_copy_assignable_v<mstd::optional_by_invalid<int>>);
static_assert(std::is_trivially_move_assignable_v<mstd::optional_by_invalid<int>>);

using namespace PT;

mstd::OptionMap options;

void parse_given_options(const int argc, const char** argv) {
  mstd::OptionDesc description;
  description["-a"] = {0,0};
  description["-s"] = {0,0};
  description["-S"] = {0,0};
  description["-v"] = {0,0};
  description["-h"] = {0,0};
  description["-m"] = {0,0};
  description["-b"] = {0,0};
  description["-f"] = {0,0};
  description["-d"] = {0,0};
  description["-o"] = {0,0};
  description[""] = {0,0};
  const std::string help_message(std::string(argv[0]) + " [-a|<options>]\n\
      FLAGS:\n\
      \t-a\tperform all tests\n\
      \t-s\trun singleton_set test\n\
      \t-v\trun sorted_vector test\n\
      \t-h\trun vector_hash test\n\
      \t-m\trun vector_map test\n\
      \t-f\trun brute-force abstraction test\n\
      \t-S\trun bounded-subset test\n\
      \t-o\trun dominators test\n\
      \t-b\trun Biconnected Components test\n\
      \t-d\trun DFS test\n");

  mstd::parse_options(argc, argv, description, help_message, options);

  if(test(options, "-a"))
    for(const auto& s: {"-s", "-h", "-m", "-b", "-d", "-v"})
      append(options, s);
}

template<class SingletonSet>
void test_singleton_sub(auto item1, auto item2) {
  assert(item1 != item2);

  SingletonSet set1{item1};
  assert(!set1.empty());
  assert(set1.size() == 1);
  assert(set1.front() == item1);

  const bool result1 = set1.erase(item2);
  assert(!result1);
  
  const bool result2 = set1.erase(item1);
  assert(result2);
  assert(set1.empty());


  SingletonSet set2;
  assert(set2.empty());
  assert(set2.size() == 0);

  append(set2, item2);
  assert(set2.size() == 1);
  assert(set2.front() == item2);

  set2.clear();
  assert(set2.empty());
  assert(set2.size() == 0);

  const auto [iter, success] = set1.emplace(item2);
  assert(success);
  assert(iter == set1.begin());

  try {
    append(set1, item1);
    assert(false);
  } catch(std::out_of_range& e) {}
}

void test_singleton() {
  static constexpr char str_invalid[3] = "xx";
  std::cout << "======> testing mstd::singleton_set...\n";
  test_singleton_sub<mstd::singleton_set_by_invalid<int>>(12, -16);
  test_singleton_sub<mstd::singleton_set_by_invalid<std::string, str_invalid>>("bla", "blubb");
  test_singleton_sub<mstd::singleton_set<std::optional<int>>>(0, -1);
  std::cout << "======> mstd::singleton_set test passed\n";
}

void test_vector_hash() {
  std::cout << "======> testing mstd::vector_hash...\n";
  std::unordered_set<uint32_t> um;
  mstd::vector_hash<uint32_t> vh;
  srand(144);
  for(size_t i = 0; i < 1e4; ++i) {
    const uint32_t q = rand() % uint32_t(1e5);
    um.emplace(q);
    vh.emplace(q);
    assert(um.size() == vh.size());
  }
  for(const auto& i: vh) assert(test(um, i));
  for(const auto& i: um) assert(test(vh, i));
  for(size_t i = 0; i < 1e4; ++i) {
    const uint32_t q = rand() % uint32_t(1e5);
    um.erase(q);
    vh.erase(q);
    assert(um.size() == vh.size());
  }
  for(const auto& i: vh) assert(test(um, i));
  for(const auto& i: um) assert(test(vh, i));
  std::cout << "======> mstd::vector_hash test passed\n";
}


void test_sorted_vector() {
  std::cout << "======> testing mstd::sorted_vector...\n";
  mstd::sorted_vector<int, std::less<>> sv;
  for(int i: {-1, 4, -2, 10, 0, 100})
    sv.emplace(i);

  std::vector<int> other{-2, -1, 0, 4, 10, 100};
  assert(sv == other);
  std::cout << "======> mstd::sorted_vector test passed\n";
}


void test_vector_map() {
  using UintVecMap = mstd::vector_map<uint32_t, uint32_t>;
  static_assert(mstd::ContainerType<UintVecMap>);
  static_assert(mstd::MapType<UintVecMap>);

  using T = mstd::raw_vector_map<size_t, int>;
  using I = typename T::iterator;
  using RT = typename I::reference;
  using VT = typename I::value_type;

  static_assert(std::is_same_v<typename T::mapped_type, int>);
  static_assert(std::is_same_v<mstd::mapped_type_of_t<T>, int>);

  static_assert(std::copy_constructible<T>);
  static_assert(std::is_object_v<T>);
  static_assert(std::move_constructible<T>);
  static_assert(std::is_lvalue_reference_v<T&>);
  static_assert(std::common_reference_with<const std::remove_reference_t<T&>&, const std::remove_reference_t<T>&>);
  static_assert(std::assignable_from<T&, T>);
  static_assert(std::swappable<T>);
  static_assert(std::assignable_from<T&, T&>);
  static_assert(std::assignable_from<T&, const T&>);
  static_assert(std::assignable_from<T&, const T>);
  static_assert(std::regular<T>);
  static_assert(std::swappable<T>);
  static_assert(std::common_reference_with<RT&&, VT&>);
  //[with Tp_ = std::raw_vector_map_iterator<long unsigned int, int>; Tp_ = std::raw_vector_map_iterator<long unsigned int, int>]
  static_assert(std::common_reference_with<std::iter_rvalue_reference_t<I>&&, const VT&>);
  static_assert(std::common_reference_with<
    std::iter_rvalue_reference_t<I>&&,
    const typename std::__detail::__iter_traits_impl<I, std::indirectly_readable_traits<I>>::type::value_type&
  >);
  //[with In_ = std::raw_vector_map_iterator<long unsigned int, int>; Tp_ = std::raw_vector_map_iterator<long unsigned int, int>
  static_assert(std::forward_iterator<I>);

  using CI = typename T::const_iterator;
  static_assert(std::input_or_output_iterator<CI>);
  using T1 = typename CI::reference&&;
  using U1 = typename CI::value_type&;
  //static_assert(std::same_as<std::common_reference_t<T1, U1>, std::common_reference_t<U1, T1>>);
  //static_assert(std::convertible_to<T1, std::common_reference_t<T1, U1>>);
  //static_assert(std::convertible_to<U1, std::common_reference_t<T1, U1>>);
  static_assert(std::common_reference_with<T1, U1>);
  static_assert(std::common_reference_with<std::iter_reference_t<CI>&&, std::iter_value_t<CI>&>);
  static_assert(std::common_reference_with<std::iter_reference_t<CI>&&, std::iter_rvalue_reference_t<CI>&&>);
  static_assert(std::common_reference_with<std::iter_rvalue_reference_t<CI>&&, const std::iter_value_t<CI>&>);
  static_assert(std::indirectly_readable<CI>);
  static_assert(std::input_iterator<CI>);
  static_assert(std::derived_from<std::random_access_iterator_tag, std::forward_iterator_tag>);
  static_assert(std::incrementable<CI>);
  static_assert(std::sentinel_for<CI, CI>);
  static_assert(std::forward_iterator<typename T::const_iterator>);
  static_assert(mstd::IterableType<T>);
  static_assert(mstd::ContainerType<T>);
  static_assert(mstd::MapType<T>);
  static_assert(mstd::HasIterTraits<mstd::iterator_of_t<UintVecMap>>);

  std::cout << "======> testing mstd::vector_map...\n";
  std::unordered_map<uint32_t, uint32_t> um;
  UintVecMap vm;
  srand(144);
  for(size_t i = 0; i < 1e4; ++i) {
    const uint32_t p = rand() % static_cast<int>(1e4);
    const uint32_t q = rand();
    DEBUG4(std::cout << "\t\tadding ("<<p<<", "<<q<<")\n");
    um.emplace(p, q);
    vm.emplace(p, q);
    assert(um.size() == vm.size());
  }
  for(const auto& i: vm) assert(um.at(i.first) == i.second);
  for(const auto& i: um) assert(vm.at(i.first) == i.second);
  
  std::cout << "\t\ttesting erase...\n";
  for(size_t i = 0; i < 1e4; ++i) {
    const uint32_t q = rand() % static_cast<int>(1e4);
    um.erase(q);
    vm.erase(q);
    assert(um.size() == vm.size());
  }
  for(const auto& i: vm) assert(um.at(i.first) == i.second);
  for(const auto& i: um) assert(vm.at(i.first) == i.second);
  std::cout << "======> mstd::vector_map test passed\n";
}


size_t choose(const size_t n, const size_t k) {
  assert(n >= k);
  size_t result = 1;
  for(size_t i = 0; i < k; ++i) result *= (n-i);
  for(size_t i = 2; i <= k; ++i) result /= i;
  return result;
}

template<mstd::StrictContainerType Container>
void test_subsets_sub(const Container& c, const ssize_t low, const ssize_t high) {
  std::cout << "\nsubsets of "<< c << " with size between "<<low<<" & "<< high <<'\n';
  const mstd::BoundedSubsetFactory<const Container> fac{c, low, high};
  size_t count = 0;
  for(auto s: fac) {
    ++count;
    std::cout << s << '\n';
    assert(static_cast<ssize_t>(s.size()) >= low);
    assert(static_cast<ssize_t>(s.size()) <= high);
  }
  size_t expected_size = 0;
  if((low >= 0) && (high >= 0)) {
    for(ssize_t i = low; i <= high; ++i)
      expected_size += choose(c.size(), i);
  }

  std::cout << "count = "<<count << " vs. expected = "<<expected_size<<'\n';
  assert(count == expected_size);
}

void test_subsets() {
  std::cout << "======> testing subset iterators and factories...\n";

  const std::unordered_set<int> set1{10, -2, 15, 6, 108, 0, 1};
  test_subsets_sub(set1, 3, 5);
  test_subsets_sub(set1, 2, 2);
  test_subsets_sub(set1, -1, -1);

  const std::vector<std::string> set2{"one", "two", "three", "four", "five"};
  test_subsets_sub(set2, 0, 2);

  std::vector<int> set3;
  for(int i = 0; i < 70; ++i) append(set3, 2*i);
  test_subsets_sub(set3, 3, 3);
  std::cout << "======> subset iterators and factories test passed\n";
}

void test_brute_force() {
  std::cout << "======> testing brute-force infrastructure ...\n";
  // see if nums has a subset summing up to 5 -- spoiler: it does :)
  std::unordered_set<int> nums{12, -3, 6, 10, -4, 100};
  const auto solutions = mstd::brute_force(nums, 2, [](const auto& S){ return (std::ranges::fold_left(S, 0) == 5);} ).solutions;
  assert(solutions.size() == 1);
  assert(solutions[0].second == 1);
  assert(solutions[0].first.size() == 3);
  
  // find the set of at most 3 items whose combined length is maximum
  std::vector<std::string> strings{"hello", "my", "name", "is", "George"};
  const auto [result2, score2] = mstd::brute_force(3, strings, 1, [](const auto& S){
      return std::ranges::fold_left(S | std::ranges::views::transform(mstd::SetSize{}), 0u);
  }).solutions[0];
  assert(score2 == 15);
  assert(result2.size() == 3);
  assert(test(result2, "hello"));
  assert(test(result2, "name"));
  assert(test(result2, "George"));
  std::cout << "======> brute-force infrastructure test passed\n";
}


void test_concat_iter() {
  std::cout << "======> testing mstd::concatenating_iterator ...\n";
  std::vector<std::vector<int>> ints{{1,2,3}, {4,5,6}, {7,8,9}};
  std::array<int, 3> indices{0,1,2};
  auto trans = [&](int i)->std::vector<int>& {return ints[i];};
  std::vector<int> result;
  std::append(result, mstd::get_concatenating(indices, trans));
  assert(result.size() == 9);
  assert(result[5] == 6);
  using MyNetwork = DefaultLabeledNetwork<mstd::DefaultDataVec, mstd::DefaultDataVec>;
  const MyNetwork N = parse_newick<MyNetwork>(mark_network2);
  const NodeVec retis = static_cast<NodeVec>(N.retis());
  std::cout << "got retis: "<<retis<<'\n';
  for(const auto uv: N.edges_above(retis))
    std::cout << uv << '\n';
  std::cout << "======> mstd::concatenating_iterator test passed ...\n";
}


void test_dfs1() {
  using MyNetwork = DefaultLabeledNetwork<void, size_t>;
  const MyNetwork N = parse_newick<MyNetwork>(random_net, Ex_edge_data{}, mstd::AnythingFromString<>{});

  NodeVec nodes;    
  nodes.clear();
  N.nodes_postorder().append_to(nodes);
  std::cout << "nodes in postorder: " << nodes << '\n';

  {
    assert(nodes.back() == N.root());
    NodeSet po_nums;
    for(const auto v: nodes) {
      assert(std::ranges::all_of(MyNetwork::children(v), [&](const auto x){return mstd::test(po_nums, x);}));
      mstd::append(po_nums, v);
    }
  }


  auto traversal = N.nodes_preorder(NodeVec{nodes[3], nodes[15], nodes[37]}); // 5, 6, 29 are now forbidden
  auto& fpred = traversal.get_forbidden();
  nodes.clear();
  std::move(traversal).append_to(nodes);
  std::cout << "nodes in preorder with forbidden "<<fpred<<": " << nodes << '\n';
  assert(nodes.size()==24);

  {
    assert(nodes.front() == N.root());
    NodeDesc old = NoNode;
    NodeMap<size_t> po_nums;
    for(const auto v: nodes) {
      po_nums[v] = po_nums.size();
      if(old != NoNode) {
        // if old->v is not an edge, then all children of old must have been seen before old itself
        assert((std::ranges::all_of(MyNetwork::children(old), [&](const auto x){ return po_nums[x] < po_nums[old];})) or N.is_edge(old, v));
      }
      old = v;
    }
  }


  NetEdgeVec<MyNetwork> edges;    
  N.edges_preorder().append_to(edges);
  std::cout << "edges in preorder: " << edges << "\n\n";

  {
    assert(edges.front().tail() == N.root());
    NodeDesc old = NoNode;
    NodeMap<size_t> po_nums;
    for(const auto uv: edges) {
      std::cout << uv << " with data: "<<uv.data()<<'\n';
      const NodeDesc v = uv.head();
      po_nums[v] = po_nums.size();
      if(old != NoNode) {
        assert((std::ranges::all_of(MyNetwork::children(old), [&](const auto x){ return po_nums[x] < po_nums[old];})) or N.is_edge(old, v));
      }
      old = v;
    }
  }
  N.print_subtree(std::cout);
}

void test_dfs2() {
#ifdef DFSCORO
  using MyNetwork = DefaultLabeledNetwork<>;
  const MyNetwork N = parse_newick<MyNetwork>(random_net);

  N.print_subtree(std::cout);

  NodeVec nodes;    
  N.nodes_postorder().append_to(nodes);
  std::cout << nodes << '\n';

  const NodeVec roots{nodes[29], nodes[21]};
  const NodeSet forbidden{nodes[15], nodes[37], nodes[3]}; 
  //DFSIterator<postorder, MyNetwork, NodeVec, NodeSet, void> it{NodeVec{nodes[29], nodes[21]}, NodeSet{nodes[15], nodes[37]}};
  
  using MyTraversal = Traversal<postorder | edge_traversal, MyNetwork, NodeVec, NodeSet, NodeSet>;

  MyTraversal trav(roots, forbidden);
  auto edges = trav.to_container();
  assert(edges.size() == 15);

  std::cout << "roots: "<<roots<<'\n';
  std::cout << "forbidden: "<< forbidden << '\n';
  size_t count = 0;
  for(auto it = std::move(trav).begin(); it.is_valid(); ++it, ++count) {
    assert(count < edges.size());
    DEBUG5(std::cout << "--- emit " << *it << '\n');
    assert(edges[count] == *it);
  }
  std::cout << edges << '\n';

  Traversal<preorder | all_edge_traversal, MyNetwork> trav_all{N};
  const auto all_edges = trav_all.to_container();

  std::cout << "network has "<<all_edges.size()<<" ("<< N.num_edges()<<") edges: "<<all_edges<<'\n';
  assert(all_edges.size() == N.num_edges()); 
#endif
}


void test_dfs3() {
  using MyNetwork = DefaultLabeledNetwork<>;
  const MyNetwork N = parse_newick<MyNetwork>(random_net_poly);

  N.print_subtree(std::cout);

  NodeVec nodes;    
  N.nodes_postorder().append_to(nodes);
  std::cout << nodes << '\n';

  const NodeVec roots{nodes[43], nodes[21]};
  const NodeSet forbidden{nodes[15], nodes[37]}; 
  //DFSIterator<postorder, MyNetwork, NodeVec, NodeSet, void> it{NodeVec{nodes[29], nodes[21]}, NodeSet{nodes[15], nodes[37]}};
  
  using MyTraversal = Traversal<inorder, MyNetwork, NodeVec, NodeSet, NodeSet>;

  N.print_subtree(std::cout);
  std::cout << "roots: "<<roots<<'\n';
  std::cout << "forbidden: "<< forbidden << '\n';
  MyTraversal trav(roots, forbidden);
  NodeVec inorder_nodes;
  for(const NodeDesc x: trav) {
    DEBUG5(std::cout << " -- emit: "<<x<<'\n');
    append(inorder_nodes, x);
  }
  std::cout << "inorder nodes: "<<inorder_nodes<<'\n';
}

void test_dls() {
#ifdef DFSCORO
  using MyNetwork = DefaultLabeledNetwork<>;
  const MyNetwork N = parse_newick<MyNetwork>(random_net);


  {
    NodeVec nodes;
    N.print_subtree(std::cout);
    std::cout << "new DLS traversal\n";
    Traversal<postorder | depth_last_traversal, MyNetwork> node_trav{N};
    node_trav.append_to(nodes);
    std::cout << "DLS postorder: "<<nodes<<'\n';
    assert(nodes.size() == N.num_nodes());
    
    const std::vector<char> descript{'n','m',0,'l','k','j',0,'i','h',0,0,0,'g','f',0,1,0,0,0,0,1,0,0,'e',0,'d',0,0,1,'c'};
    for(size_t i = 0; i < descript.size(); ++i) {
      switch(descript[i]) {
        case 0: assert(not MyNetwork::is_reti(nodes[i]) && not MyNetwork::is_leaf(nodes[i])); break;
        case 1: assert(MyNetwork::is_reti(nodes[i])); break;
        default:
          assert(mstd::front(MyNetwork::label(nodes[i])) == descript[i]);
      }
    }

  }

  {
    N.print_subtree(std::cout);
    Traversal<preorder | all_edge_traversal | depth_last_traversal, MyNetwork> trav_all{N};
    std::cout << "DLS all-edge-preorder:\n";
    const auto all_edges = trav_all.to_container();
    std::cout << all_edges << '\n';
    assert(all_edges.size() == N.num_edges());

    const std::vector<char> descript{1,0,1,0,0,1,0,0,'n','m',0,0,'l',0,0,'k','j',0,'i','h',1,0,'g','f',1,0,0,0,1,1,1};
    for(size_t i = 0; i < descript.size(); ++i) {
      switch(descript[i]) {
        case 0: assert(not MyNetwork::is_reti(all_edges[i].head()) && not MyNetwork::is_leaf(all_edges[i].head())); break;
        case 1: assert(MyNetwork::is_reti(all_edges[i].head())); break;
        default:
          assert(mstd::front(MyNetwork::label(all_edges[i].head())) == descript[i]);
      }
    }

  }

#endif
}

void test_dfs() {
  std::cout << "======> testing DFS infrastructure ...\n";
  test_dfs1();
  test_dfs2();
  test_dls();
  test_dfs3();
  std::cout << "======> DFS infrastructure test passed\n";
}


void test_bcc() {
  std::cout << "======> testing Biconnected Component infrastructure ...\n";
  static std::string diversity_network_006 = "((((((l32:0.02050164852,#H47:0::0.5):1.068140708,((l25:0.2527897357,l26:0.2527897357):0.1716547064,(t63:0.1403079247,t57:0.1403079247):0.2841365174):0.6641979148):1.41922525,((l22:0.5358774596,((l28:0.1688628659,(l29:0.1688628659)#H45:0::0.5):0.1005080127,l24:0.2693708785):0.2665065811):1.299955857,#H28:0.1347631415::0.5):0.6720342898):0.08386657447,((l21:0.6305476259,(l27:0.2235551151,((t17:0.05132528232,l30:0.05132528232):0.1175375835,#H45:0::0.5):0.05469224922):0.4069925108):0.05791050211,(l31:0.02050164852,((l33:0.006991814916,l34:0.006991814916):0.0135098336)#H47:0::0.5):0.6679564794):1.903276053):1.18153951,((((l23:0.3008441365,t137:0.3008441365):0.01128473765,(t58:0.3121288741)#H41:0::0.5):0.3613027167,(t128:0.3121288741,#H41:0::0.5):0.3613027167):1.027638584)#H28:2.072203516::0.5):1.225375063);";
  using Net = DefaultLabeledNetwork<>;
  const Net N = parse_newick<Net>(diversity_network_006);

    using NodeData = NodeDesc;
    using EdgeData = typename Net::EdgeData;
    using BCComponent = CompatibleNetwork<Net, NodeData, EdgeData, void>; // no edge-data or labels necessary
    using BCDataExtracter = DataExtracter<Net, mstd::IdentityFunction<NodeDesc>>;
    using BCEmplacementHelper = EdgeEmplacementHelper<BCComponent, false>; // no need to track roots, the BCCIterator does it automatically
    using BCEmplacer = EdgeEmplacer<BCEmplacementHelper, BCDataExtracter>;
    using BCCIter = BCCIterator<Net, BCComponent, false, BCEmplacer>; // NOTE: no trivial components
  {
    BCCChainDecomposition<Net> chains{N};
    auto Niter = N.nodes_postorder().begin();
      auto a = BasicBCCIter<Net>(std::move(Niter), std::move(chains));
      auto b = CutNodeChildContainerIterator<Net>(std::piecewise_construct, std::tuple{std::move(a)}, std::tuple{});
      auto c = CutNodeChildrenIterator<Net>(std::move(b));
      auto d = BCCStartingCutNodeChildIterator<Net, false>(std::piecewise_construct, std::tuple{std::move(c)}, std::tuple{});  
      auto bcc_maker = BCCmaker<Net, BCComponent, BCEmplacer>(mstd::IdentityFunction<NodeDesc>());
      assert(bcc_maker.output_emplacer.helper.N == &(bcc_maker.output));
      auto iter = BCCIter(std::piecewise_construct, std::forward_as_tuple(std::move(d)), std::forward_as_tuple(std::move(bcc_maker)));
    
    assert(iter->num_edges() == 9);
  }
  /*
    BCCChainDecomposition<Net> chains{N};
    std::cout << "done making chain decomposition\n\n";

    using DFSIter = typename NodeTraversal<postorder, Net, NodeDesc>::OwningIter;
    std::cout << "making "<<mstd::type_name<DFSIter>()<<"\n";
    DFSIter dfs_it{N};
    std::cout << "got verifyable iter\n";

    BasicBCCIter<Net> bbi{N, std::move(chains)};
    std::cout << "got Basic BCC iter\n";

    using AutoIter = mstd::auto_iter<BasicBCCIter<Net>>;
    AutoIter ai{make_from_tuple<AutoIter>(std::forward_as_tuple(N, N))};
    std::cout << "got autoiter\n";

    using BeginEnd = BCCBeginEnd<Net, BCComponent, false, NodeTranslation, BCDataExtracter>;
    BeginEnd be{make_from_tuple<BeginEnd>(std::forward_as_tuple(N, mstd::IdentityFunction<NodeDesc>()))};
  */
  {
    std::cout << "getting Factory\n";
    using Fac = mstd::IterFactoryWithBeginEnd<typename BasicBCCIter<Net>::Iterator, BCCBeginEnd<Net, BCComponent, false, NodeTranslation, BCDataExtracter>>;
    Fac factory{std::piecewise_construct, std::forward_as_tuple(N, mstd::IdentityFunction<NodeDesc>()), std::forward_as_tuple(N)};
    
    std::cout << "got factory, now starting iteration\n";
    auto iter = factory.begin();

    assert(iter->num_edges() == 9);
  }
  std::cout << "======> Biconnected Component infrastructure test passed\n";
}

void test_dominators() {
  std::cout << "======> testing Dominator Tree Algorithm ...\n";
  using MyNetwork = DefaultLabeledNetwork<void, uint32_t>; // random_net has edge-lengths that we try to shove into the dominator tree
  using DomTree = CompatibleTree<MyNetwork, NodeDesc>; // the dominator tree stores the corresponding nodes of the input network as NodeData
  
  {
    const MyNetwork N = parse_newick<MyNetwork>(random_net, Ex_edge_data{}, mstd::AnythingFromString<uint32_t>{});
    N.print_summary(std::cout);
    std::cout << ExtendedDisplay(N) << '\n';

    NaiveDominatorOracle dom_oracle(N);
    // NOTE: remember to extract the NodeDesc in the Network as NodeData
    DomTree dom_tree = dom_oracle.template make_dominator_tree<DomTree>(Ex_node_data{}, mstd::IdentityFunction<>{});

    std::cout << ExtendedDisplay(dom_tree) << '\n';
  }
  std::cout << "======> Dominator Tree Algorithm test passed\n";
}


int main(const int argc, const char** argv) {
  parse_given_options(argc, argv);

  if(test(options, "-s")) test_singleton();
  if(test(options, "-v")) test_sorted_vector();
  if(test(options, "-h")) test_vector_hash();
  if(test(options, "-m")) test_vector_map();
  if(test(options, "-S")) test_subsets();
  if(test(options, "-f")) test_brute_force();
  if(test(options, "-c")) test_concat_iter();
  if(test(options, "-d")) test_dfs();
  if(test(options, "-b")) test_bcc();
  if(test(options, "-o")) test_dominators();

}


