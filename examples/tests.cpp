
#include <optional>
#include <cstdint>
#include <iostream>

// this runs different tests to see if the low-level stuff is working
// we're testing:
#include "utils/singleton.hpp" //    singleton_set
#include "utils/vector_hash.hpp" //    vector_hash
#include "utils/vector_map.hpp" //    vector_map
#include "utils/optional.hpp" //    optional
#include "utils/subsets.hpp" //    subset iterators

#include "utils/command_line.hpp"


PT::OptionMap options;

void parse_given_options(const int argc, const char** argv) {
  PT::OptionDesc description;
  description["-a"] = {0,0};
  description["-s"] = {0,0};
  description["-h"] = {0,0};
  description["-m"] = {0,0};
  description["-b"] = {0,0};
  description[""] = {0,0};
  const std::string help_message(std::string(argv[0]) + " [-a|<options>]\n\
      FLAGS:\n\
      \t-a\tperform all tests\n\
      \t-s\trun singleton_set test\n\
      \t-h\trun vector_hash test\n\
      \t-m\trun vector_map test\n\
      \t-b\trun bounded-subset test\n");

  PT::parse_options(argc, argv, description, help_message, options);

  if(test(options, "-a"))
    for(const auto& s: {"-s", "-h", "-m", "-b"})
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
}

void test_vector_hash() {
  std::cout << "======> testing mstd::vector_hash...\n";
  std::unordered_set<uint32_t> um;
  mstd::vector_hash<uint32_t> vh;
  srand(144);
  for(size_t i = 0; i < 10e5; ++i) {
    const uint32_t q = rand();
    um.emplace(q);
    vh.emplace(q);
    assert(um.size() == vh.size());
  }
  for(const auto& i: vh) assert(test(um, i));
  for(const auto& i: um) assert(test(vh, i));
  for(size_t i = 0; i < 10e4; ++i) {
    const uint32_t q = rand();
    um.erase(q);
    vh.erase(q);
    assert(um.size() == vh.size());
  }
  for(const auto& i: vh) assert(test(um, i));
  for(const auto& i: um) assert(test(vh, i));
}


using UintVecMap = mstd::vector_map<uint32_t, uint32_t>;
static_assert(mstd::ContainerType<UintVecMap>);
static_assert(mstd::MapType<UintVecMap>);

using T = mstd::raw_vector_map<size_t, int>;
using I = typename T::iterator;
using RT = typename I::reference;
using VT = typename I::value_type;
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
//[with _Tp = std::raw_vector_map_iterator<long unsigned int, int>; _Tp = std::raw_vector_map_iterator<long unsigned int, int>]
static_assert(std::common_reference_with<std::iter_rvalue_reference_t<I>&&, const VT&>);
static_assert(std::common_reference_with<
  std::iter_rvalue_reference_t<I>&&,
  const typename std::__detail::__iter_traits_impl<I, std::indirectly_readable_traits<I>>::type::value_type&
>);
//[with _In = std::raw_vector_map_iterator<long unsigned int, int>; _Tp = std::raw_vector_map_iterator<long unsigned int, int>
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



void test_vector_map() {
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
  std::cout << "\t\ttesting equality...\n";
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
}




size_t choose(const size_t n, const size_t k) {
  assert(n >= k);
  size_t result = 1;
  for(size_t i = n; i != k; --i)
    result *= i;
  return result;
}

template<mstd::ContainerType Container>
void test_subsets_sub(const Container& c, const size_t low, const size_t high) {
  const mstd::BoundedSubsetFactory<Container> fac{c, low, high};
  size_t count = 0;
  std::cout << "\nsubsets of "<< c << " with size between "<<low<<" & "<< high <<'\n';
  for(auto s: fac) {
    ++count;
    std::cout << s << '\n';
    assert(s.size() >= low);
    assert(s.size() <= high);
  }
  size_t expected_size = 0;
  for(size_t i = low; i < high; ++i)
    expected_size += choose(c.size(), i);
  
  assert(count == expected_size);
}

void test_subsets() {
  std::cout << "======> testing mstd::vector_map...\n";

  const std::unordered_set<int> set1{10, -2, 15, 6, 108, 0, 1};
  test_subsets_sub(set1, 3, 5);
  test_subsets_sub(set1, 2, 2);
  test_subsets_sub(set1, -1, -1);


  const std::vector<std::string> set2{"one", "two", "three", "four", "five"};
  test_subsets_sub(set2, 0, 2);
}


constexpr auto mark_network = "(((a:3,(b:2)#H1:2;0.5):2,(#H1:2;0.5,c:3):2):8,x:100);";

int main(const int argc, const char** argv) {
  parse_given_options(argc, argv);

  if(test(options, "-s")) test_singleton();
  if(test(options, "-h")) test_vector_hash();
  if(test(options, "-m")) test_vector_map();
  if(test(options, "-b")) test_subsets();

}


