
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
  std::cout << "testing mstd::singleton_set...\n";
  test_singleton_sub<mstd::singleton_set_by_invalid<int>>(12, -16);
  test_singleton_sub<mstd::singleton_set_by_invalid<std::string, str_invalid>>("bla", "blubb");
  test_singleton_sub<mstd::singleton_set<std::optional<int>>>(0, -1);
}

void test_vector_hash() {
  std::cout << "testing mstd::vector_hash...\n";
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

void test_vector_map() {
  std::cout << "testing mstd::vector_map...\n";
  std::unordered_map<uint32_t, uint32_t> um;
  UintVecMap vm;
  srand(144);
  for(size_t i = 0; i < 10e5; ++i) {
    const uint32_t p = rand();
    const uint32_t q = rand();
    um.emplace(p, q);
    vm.emplace(p, q);
    assert(um.size() == vm.size());
  }
  for(const auto& i: vm) assert(um.at(i.first) == i.second);
  for(const auto& i: um) assert(vm.at(i.first) == i.second);
  for(size_t i = 0; i < 10e4; ++i) {
    const uint32_t q = rand();
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
  std::cout << "testing mstd::vector_map...\n";

  const std::unordered_set<int> set1{10, -2, 15, 6, 108, 0, 1};
  test_subsets_sub(set1, 3, 5);
  test_subsets_sub(set1, 2, 2);
  test_subsets_sub(set1, -1, -1);


  const std::vector<std::string> set2{"one", "two", "three", "four", "five"};
  test_subsets_sub(set2, 0, 2);
}


constexpr auto mark_network = "(((a:3,(b:2)#H1:2;0.5):2,(#H1:2;0.5,c:3):2):8,x:100);";

int main(const int argc, const char** argv) {
  test_singleton();
  test_vector_hash();
  test_vector_map();
  test_subsets();

}


