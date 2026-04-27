# CLAUDE.md
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a C++20 header-only library (phylo_tools) for phylogenetic network algorithms. Requires a recent compiler (tested with GCC-11.3+).
The library uses heavy C++20 template metaprogramming with concepts, but it avoids virtual dispatch; everything is templates/concepts.

**Configure and build:**
```bash
cmake .                       # Configure with defaults
make <target>                 # Build specific executable
make all                      # Build all executables
```

**CMake options** (set via `-D<option>=ON/OFF` or `-D<option>=<value>`):
- `DEBUG=<0-9>` - Debug verbosity level (0=off, 9=max)
- `DFSCORO=ON` - Use coroutine-based DFS implementation (default: ON)
- `ASAN=ON` - Enable address sanitizer (default: ON)
- `STATISTICS=ON` - Collect runtime statistics
- `STATIC=ON` - Static linking
- `GRAPHITE=ON` - Graphite loop analysis flags
- `MAXERR=<n>` - Max errors shown in debug mode (default: 3)

Example: `cmake -DDEBUG=3 -DSTATISTICS=ON .`


## File Organization

```
examples/          - Executable source files (.cpp), each builds to root
utils/             - Library headers (110+ .hpp files, header-only)
io/                - Input/output
data/              - Test data (gitignored)
```

## mstd Library (Custom STL Extensions)

The library provides extensive STL-like utilities in the `mstd` namespace. Components are organized by dependency layers:

### Layer 0: Foundation

Core utilities with minimal dependencies.

| Class/Function | File | Description |
|----------------|------|-------------|
| `type_name<T>()` | `utils/debug_utils.hpp` | Compile-time type name (demangler) |
| `TR_ConstRefOK`, `TR_Strict`, `TR_PtrOK`, etc. | `utils/runes.hpp` | Type-level runes for const/ref transformations |
| `is_any_of<T, Us...>` / `AnyOf<T, Us...>` | `utils/stl_utils.hpp` | Type traits for variant-like matching |
| `value_type_of_t<T>` / `reference_of_t<T>` / `key_type_of_t<T>` | `utils/stl_utils.hpp` | Shortcut type traits |
| `FirstTypeOf<Ts...>` / `FirstNonVoid<T, Ts...>` | `utils/stl_utils.hpp` | Variadic type selection |
| `is_really_arithmetic_v<T>` | `utils/stl_concepts.hpp` | Arithmetic type trait excluding char/bool (includes pointers) |
| `iter_traits_from_reference<T>` | `utils/stl_utils.hpp` | Iterator traits builder from reference type |
| `GCC_VERSION` / `CLANG_VERSION` | `utils/platform.hpp` | Compiler version detection macros |

### Layer 1: Concepts and Generic Algorithms

C++20 concepts and fundamental algorithms.

**Concepts** (`utils/stl_concepts.hpp`):
- `ContainerType`, `IterableType`, `MapType`, `SetType`, `VectorType`
- `ArithmeticType`, `StrictlyArithmeticType`
- `PointerType` (distinguishes `int*` from `std::vector<int>::iterator`)
- `TupleType`, `PairType`
- `FindableType<C>`, `HashableType`

**Generic Algorithms**:

| Function | File | Description |
|----------|------|-------------|
| `append(container, items...)` | `utils/append.hpp` | Generic append/emplace (dispatches to emplace_back, try_emplace, insert, etc.) |
| `erase(container, value)` | `utils/erase.hpp` | Generic erase interface |
| `set_val(s, val)` | `utils/set_interface.hpp` | Generic set insert that discards return value for efficiency |
| `find(c, key)` / `find_reverse(c, key)` | `utils/set_interface.hpp` | Unified find for set/map and standard containers |
| `common_element(x, y)` | `utils/set_interface.hpp` | Find common element between two sets (favors smaller set) |

**Set Interface Unification** (`utils/set_interface.hpp`):
- `SettableType` concept for containers with `.set()` method
- Unifies `std::set`, `std::unordered_set`, `iterable_bitset` under common interface

**Hash Utils** (`utils/hash_utils.hpp`):
- `uint32_hash(x)` / `uint32_unhash(x)` - Bijective hash for uint32
- `uint64_hash(x)` / `uint64_unhash(x)` - Bijective hash for uint64
- `hash_combine(x, y)` - Boost-style hash combining
- `hash_combine_symmetric(x, y)` - Order-independent hash combining (for unordered containers)

**Integer Types** (`utils/tight_int.hpp`):
- `uint_tight<MaxValue>` - Smallest unsigned type that can hold MaxValue

**Tags** (`utils/tags.hpp`): Policy and direction tags used throughout:
- `owning_tag` / `non_owning_tag`
- `reverse_edge_tag`, `leaf_labels_only_tag`
- `policy_move_tag` / `policy_copy_tag` / `policy_inplace_tag` / `policy_noop_tag`
- `above_tag` / `below_tag` (direction tags)
- `roots_tag` / `leaves_tag`
- `partial_extension_tag`
- `Ex_node_label` / `Ex_node_data` / `Ex_edge_data` (data extraction tags)

### Layer 2: Utilities and Helpers

**Exceptions** (`utils/except.hpp`):
- `MalformedInput` - Input validation errors
- `StopIteration` - Iterator exhaustion signal (also in `generator.hpp`)

**Random** (`utils/random.hpp`):
- `toss_coin(probability)` - Weighted boolean
- `throw_die(sides)` - Integer in [0, sides-1]
- `throw_bw_die(good, sides)` - Boolean with good/sides probability
- `fisher_yates_choose(k, n, result)` - Draw k distinct integers from [0,n-1]
- `cardchoose(k, n, result)` - Alternative sampling when k is small

**Predicates** (`utils/predicates.hpp`):
- `TruePredicate` / `FalsePredicate`
- `BinaryEqualPredicate` / `BinaryUnequalPredicate`
- `NotPredicate<P>` - Negates a predicate
- `ContainmentPredicate<Container, invert>` - Tests if item is in container

**Character Pointer** (`utils/charp.hpp`):
- `charp<small_bytes>` - Null-terminated string with small-string optimization
- Stores short strings inline; handles allocation edge cases

**Empty Containers** (`utils/emptyset.hpp`):
- `empty_set<T>` - Set interface that contains nothing
- `empty_map<Key, Value>` - Map interface that contains nothing

**Optional Tuple** (`utils/optional_tuple.hpp`):
- `optional_tuple<Ts...>` - Tuple where each element can be individually null
- `as_optional<T>(t)` - Convert optional to optional_ref

**Iterator Helpers**:

| Class | File | Description |
|-------|------|-------------|
| `_auto_iter<Iterator, EndIterator>` | `utils/auto_iter.hpp` | Iterator pair that knows its end (converts to bool) |
| `TaggedPointer<T, Tag, Bits>` | `utils/tagged_pointer.hpp` | Pointer with embedded tag bits (used by splay_tree) |

### Layer 3: Containers

**Hash-Based Containers**:

| Class | File | Description |
|-------|------|-------------|
| `vector_hash<Key>` | `utils/vector_hash.hpp` | Flat hash set using open addressing with vector backend |

**Sorted Containers**:

| Class | File | Description |
|-------|------|-------------|
| `sorted_vector<Key, Compare>` | `utils/sorted_vector.hpp` | Set as sorted vector (O(log n) find, O(n) insert) |

**Map Variants**:

| Class | File | Description |
|-------|------|-------------|
| `vector_map<Key, T>` | `utils/vector_map.hpp` | Map with vector storage and optional value semantics |
| `raw_vector_map<Key, T>` | `utils/raw_vector_map.hpp` | Lower-level vector-based map |
| `IntegralBimap<ForwardMap, ReverseMap>` | `utils/bimap.hpp` | Bidirectional map for integral keys |

**Specialized Sets**:

| Class | File | Description |
|-------|------|-------------|
| `singleton_set<T>` | `utils/singleton.hpp` | Set holding at most one element |
| `counting_multiset<T>` | `utils/counting_multiset.hpp` | Multiset using hash map for counts |
| `iterable_bitset` / `ordered_bitset` | `utils/iter_bitset.hpp` | Sparse bitset backed by unordered_map |
| `empty_set<T>` / `empty_map<K,V>` | `utils/emptyset.hpp` | No-op containers with full interface |

**Vector Variants**:

| Class | File | Description |
|-------|------|-------------|
| `static_capacity_vector<T, N>` | `utils/static_capacity_vector.hpp` | Vector with fixed max capacity (stack buffer) |
| `small_vector<T, N>` | `utils/small_vector.hpp` | Small-string-optimized style vector |
| `Something2d<Element, Container, Symmetry>` | `utils/vector2d.hpp` | 2D container (matrix) optimized over vector<vector<T>> |
| `generator_iter<Item, IndexType>` | `utils/generator_iter.hpp` | Iterator generating k copies of an item |

**Balanced Trees**:

| Class | File | Description |
|-------|------|-------------|
| `STNode<Key, Payload, tag_size>` | `utils/splay_tree.hpp` | Splay tree node with optional tagging |
| `SplayTree<Key, Payload, tag_size>` | `utils/splay_tree.hpp` | Self-adjusting binary search tree |
| `LinkCutTree<Key, Payload>` | `utils/link_cut_tree.hpp` | Dynamic forest with link/cut/LCA operations (Sleator-Tarjan) |

**Union-Find**:

| Class | File | Description |
|-------|------|-------------|
| `DSet<Key, Payload>` | `utils/union_find.hpp` | Disjoint set element with optional payload |
| `DisjointSetForest<Key, Payload>` | `utils/union_find.hpp` | Union-find with union by size and path compression |

**Optional Storage**:

| Class | File | Description |
|-------|------|-------------|
| `optional_by_invalid<T, tombstone>` | `utils/optional.hpp` | Optional using tombstone value (saves space) |

### Layer 4: Iterator Factories and Advanced Iterators

| Class | File | Description |
|-------|------|-------------|
| `generator<T>` | `utils/generator.hpp` | C++20 coroutine-based generator |
| `_filtered_iterator<Iter, Pred, pass_iterator>` | `utils/filter.hpp` | Iterator skipping items where predicate is false |
| `transforming_iterator<Iter, Trans, pass_iterator>` | `utils/trans_iter.hpp` | Iterator applying transformation on dereference |
| `_concatenating_iterator<ContainerIter, ItemIter>` | `utils/concat_iter.hpp` | Iterator concatenating multiple iterables |
| `cyclic_iterator<Iter>` | `utils/cyclic_iterator.hpp` | Iterator cycling through container repeatedly |

**Iterator Factory Infrastructure** (`utils/iter_factory.hpp`):
- `IterFactory<Iter>` / `ProtoIterFactory` - Base classes for iterator factories

**Subset Iteration** (`utils/subsets.hpp`):
- `SubsetIterator<Container, partial, Output>` - iterate all subsets of size [low, high]
- `BoundedSubsetFactory` - factory for subset iteration
- `SubsetOfIter` - iterator over subsets

**Concatenation Factory** (`utils/concat_iter.hpp`):
- `ConcatenatingIterFactory` - Factory for concatenating iterators

### Layer 5: Algorithms and Accumulators

| Class/Function | File | Description |
|----------------|------|-------------|
| `brute_force(bounds, container, keep, score_fn)` | `utils/brute_force.hpp` | Exhaustive search over subsets meeting criteria |
| `SetSize` / `ContainerSize` | `utils/brute_force.hpp` | Scoring functions for subset enumeration |
| `SolutionAccumulator<T, Score, Cmp>` | `utils/solution_accu.hpp` | Keep top-k solutions by score |
| `linear_interval<T>` | `utils/linear_interval.hpp` | Interval of numbers [lo, hi) |
| `memoize_t<Sig, F>` / `memoize(func)` | `utils/memoize.hpp` | Automatic memoization wrapper |
| `SlopeReduction::apply(c)` | `utils/slope.hpp` | Remove points between local extrema in a sequence |


## PT Library (Basic algorithms for Phylogenetic networks)

The library provides phylogenetic network and tree algorithms in the `PT` namespace. Components are organized by dependency layers:

### Layer 0: Foundation Types and Concepts

Core types, storage policies, and type traits used throughout.

**Storage Policies** (`utils/types.hpp`):
- `vecS`, `sortvecS`, `setS`, `hashsetS`, `multisetS`, `vecsetS`, `singleS` - Storage backend selectors
- `StorageClass<storage, Element>` - Type selector for storage backends
- `is_inplace_modifyable<storage>` / `unique_elements<storage>` - Storage trait queries

**Node Descriptors** (`utils/types.hpp`):
- `NodeDesc` - Node identifier (uintptr_t, no vptr overhead)
- `NoNode` - Invalid node marker
- `OptionalNodeDesc` - Optional node descriptor with tombstone

**Type Concepts** (`utils/types.hpp`):
- `StrictNodeType` / `NodeType` - Node classes with parent/child access
- `StrictPhylogenyType` / `PhylogenyType` - Network/tree containers
- `StrictTreeType` / `TreeType` - Tree-only containers (single parent)
- `AdjacencyType` / `StrictAdjacencyType` - Edge endpoint types
- `NodeIterableType` / `NodeContainerType` / `NodeSetType` - Container concepts
- `NodeFunctionType` / `NodePredicateType` / `EdgePredicateType` - Callable concepts
- `DataExtracterType` - Data extraction from edges/nodes

**Core Types** (`utils/types.hpp`):
| Type | Description |
|------|-------------|
| `NodeVec` | `std::vector<NodeDesc>` |
| `NodeSet` | `HashSet<NodeDesc>` |
| `NodeMap<T>` | `HashMap<NodeDesc, T>` |
| `NodePair` | `std::pair<NodeDesc, NodeDesc>` |
| `NodeTranslation` | `NodeMap<NodeDesc>` - Node ID mapping |
| `Degree` / `Degrees` | Degree types for nodes |

### Layer 1: Core Data Structures

Fundamental edge and adjacency types.

| Class | File | Description |
|-------|------|-------------|
| `ProtoAdjacency` | `utils/adjacency.hpp` | Base adjacency with node descriptor only |
| `Adjacency<EdgeData>` | `utils/adjacency.hpp` | Adjacency with optional edge data (via shared_ptr) |
| `Adjacency<void>` | `utils/adjacency.hpp` | Adjacency without edge data (space-efficient) |
| `ProtoEdge<EdgeData>` | `utils/edge.hpp` | Base edge (node pair with adjacency) |
| `Edge<EdgeData>` | `utils/edge.hpp` | Edge with data access methods |
| `Edge<void>` | `utils/edge.hpp` | Edge without data |

**Concepts** (`utils/edge.hpp`):
- `StrictEdgeType` / `EdgeType` - Edge class constraints
- `LooseEdgeType` - Edge or adjacency pair
- `EdgeFunctionType` / `OptionalEdgeFunctionType` - Edge callables
- `EdgeIterableType` / `EdgeContainerType` - Edge collection concepts

### Layer 2: Node and Node Access

Node structures and unified node access interface.

**Node Classes** (`utils/node.hpp`):
| Class | Description |
|-------|-------------|
| `ProtoNode<PredStorage, SuccStorage, EdgeData>` | Base node with predecessor/successor containers |
| `Node_<PredStorage, SuccStorage, NodeData, EdgeData>` | Node with optional node data |
| `Node<PredStorage, SuccStorage, NodeData, EdgeData, LabelType>` | Node with optional label support |
| `DefaultNode` | Default node type alias |

**Node Access** (`utils/node.hpp`):
| Class | Description |
|-------|-------------|
| `NodeAccess<Node>` | Static interface to access node properties by descriptor |
| `InternalDataAccess<Net>` | Accessor for node/edge data stored in network |
| `ExternalDataAccess<NodeData, EdgeData>` | Hash-map based external data storage |

**Node Queries** (via `NodeAccess`):
- `parents(u)`, `children(u)`, `in_degree(u)`, `out_degree(u)`, `degree(u)`
- `is_root(u)`, `is_leaf(u)`, `is_reti(u)`, `is_tree_node(u)`, `is_suppressible(u)`
- `label(u)`, `data(u)` - Access to node data/labels

### Layer 3: Network and Tree Classes

The core phylogenetic network and tree data structures.

**Network/Tree Types** (`utils/phylogeny.hpp`, `utils/network.hpp`, `utils/tree.hpp`):

| Class | File | Description |
|-------|------|-------------|
| `ProtoPhylogeny<...>` | `phylogeny.hpp` | Base phylogeny with node counting and roots management |
| `Phylogeny<PredStorage, SuccStorage, NodeData, EdgeData, LabelType, RootStorage>` | `phylogeny.hpp` | Full network/tree with modification operations |
| `Network<...>` | `network.hpp` | Single-rooted network alias (`singleS` root storage) |
| `DAG<...>` | `network.hpp` | Multi-rooted network (directed acyclic graph) |
| `Tree<SuccStorage, ...>` | `tree.hpp` | Single-rooted tree (single parent per node) |
| `Forest<...>` | `tree.hpp` | Multi-rooted forest |
| `DefaultNetwork` / `DefaultTree` / `DefaultLabeledNetwork` / `DefaultLabeledTree` | Convenience aliases with default storage |

**Network Operations** (`utils/phylogeny.hpp`):
- `add_root()`, `add_child()`, `add_parent()`, `add_edge()` - Node/edge insertion
- `remove_edge()`, `remove_node()`, `remove_subtree()` - Removal with cleanup
- `contract_up()`, `contract_down()`, `suppress_node()` - Node contraction/suppression
- `transfer_child()`, `transfer_children()`, `transfer_parent()`, `transfer_parents()` - Edge reallocation
- `subdivide_edge()` - Edge subdivision
- `replace_parents()` - Parent replacement
- `reroot_no_cleanup()` - Network rerooting

**Network Queries** (`utils/phylogeny.hpp`):
- `num_nodes()`, `num_edges()`, `num_roots()`, `num_leaves()`, `reticulation_number()`
- `is_forest()`, `is_tree()`, `empty()`, `edgeless()`
- `root()`, `roots()`, `leaves()`
- `get_cycle()`, `has_cycle()` - Cycle detection

**Traversal Methods** (`utils/phylogeny.hpp`):
- `nodes<order>()`, `nodes_below<order>()`, `nodes_above<order>()` - Node traversals
- `nodes_preorder()`, `nodes_postorder()` - Convenience methods
- `nodes_with<pred>()` - Filtered traversals
- `leaves()`, `retis()` - Special node traversals
- `edges<order>()`, `edges_below<order>()`, `edges_above<order>()` - Edge traversals

### Layer 4: Cut Algorithms, Connectivity, and LCA/Ancestor Oracles

Biconnected components, bridges, cut nodes, and ancestor/LCA queries.

**Cut Algorithms** (`utils/cuts.hpp`):
| Class/Function | Description |
|----------------|-------------|
| `ChainInfo` | DFS chain decomposition metadata per node |
| `ChainDecomposition<Network, cut_object, output_root>` | Chain decomposition for cut detection |
| `get_cut_nodes<Network>()` | Iterator factory for cut nodes |
| `get_bridges<Network>()` | Iterator factory for bridges |
| `CutNodeIter` / `BridgeIter` / `BCCCutIter` | Cut-aware iterator adapters |

**Biconnected Components** (`utils/biconnected_comps.hpp`):
| Class | Description |
|-------|-------------|
| `BiconnectedComponent<Network, cut_object, track_roots>` | BCC extraction and management |
| `get_biconnected_components<Component>(N, node_extractor)` | Factory for BCC iteration |

**LCA and Ancestor Oracles** (`utils/lca.hpp`):
| Class | Description |
|-------|-------------|
| `NaiveTreeAncestorOracle<Tree>` | O(n) query, no preprocessing |
| `IntervalTreeAncestorOracle<Tree>` | O(1) query, O(n) preprocessing (preorder intervals) |
| `NaiveTreeLCAOracle<Tree>` | Naïve LCA, O(n) per query |
| `HeavyPathsLCAOracle<Tree>` | O(log n) query using heavy-path decomposition |
| `NaiveNetworkAncestorOracle<Net>` | Multi-path ancestor check for networks |
| `MetaNetworkLCAOracle<Net, Ancestors>` | LCA from ancestor sets |
| `NaiveNetworkLCAOracle<Net>` | Naïve network LCA |

**Defaults** (`utils/lca.hpp`):
- `DefaultStaticTreeAncestorOracle<Tree>` - `IntervalTreeAncestorOracle`
- `DefaultStaticTreeLCAOracle<Tree>` - `NaiveTreeLCAOracle` (currently)
- `DefaultStaticNetworkLCAOracle<Net>` - `NaiveNetworkLCAOracle`

### Layer 5: Switchings, Displayed Trees, Scanwidth, and Advanced Algorithms

Switching enumeration, extension trees, scanwidth, and high-level algorithms.

**Tree-Induced Subgraphs** (`utils/induced_tree.hpp`):
| Class/Function | Description |
|--------------|-------------|
| `InducedSubtreeInfo` / `SparseInducedSubtreeInfo` | Distance and order metadata |
| `get_induced_subtree_infos()` | Compute subtree info from root |
| `get_induced_edges()` | Compute edges of minimal subtree spanning leaf set |

**Switchings and Displayed Trees** (`utils/switching.hpp`, `utils/switching_iter.hpp`):
| Class | Description |
|-------|-------------|
| `Switching<Network>` | Maps each reticulation to selected parent |
| `SwitchingFactory<Network, tag, Nodes>` | Iterator factory for all switchings |
| `SwitchingIter` | Iterator over all switchings |
| `SwitchingType<T>` | Concept for switching types |

**Scanwidth and Extension Trees** (`utils/scanwidth.hpp`, etc.):
| Class/Function | Description |
|----------------|-------------|
| `SWconfig` enum | Configuration flags (`sw_low_mem_footprint`, `sw_no_preprocess`, `sw_bottom_up`) |
| `compute_min_sw_extension<config>(N, register_node)` | Main scanwidth algorithm |
| `ScanwidthPP<Network>` | Scanwidth preprocessing |
| `ScanwidthDP<low_mem, Network, EdgeWeightExtract, ignore_deg2>` | Dynamic programming for scanwidth |
| `ScanwidthDP2<...>` | Alternative DP implementation |
| `Extension` | Extension tree representation |

**Network Isomorphism** (`utils/isomorphism.hpp`):
| Class | Description |
|-------|-------------|
| `IsomorphismMapper<NetworkA, NetworkB, PossSet>` | Isomorphism checker with constraint propagation |
| `FLAG_MAP_LEAF_LABELS` / `FLAG_MAP_TREE_LABELS` / `FLAG_MAP_RETI_LABELS` / `FLAG_MAP_ALL_LABELS` | Label mapping flags |

**Tree Containment** (`utils/containment.hpp`, etc.):
| Class | Description |
|-------|-------------|
| `TreeInNetContainment<Host, Guest, HostLabelStorage, leaf_labels_only>` | Tree-in-network containment solver |
| `TreeComponents<Network, k>` | Tree component enumeration |
| `TreeComponentInfos<Network, k>` | Component metadata for containment |
| `TreeTreeContainment<Host, Guest>` | Tree-in-tree containment |
| `TreeCompContainment<Host, Guest>` | Component-based containment |
| `ReductionManager<Containment>` | Reduction rule application |

**Phylogenetic Diversity** (`utils/diversity.hpp`, `utils/diversity_avg_tree.hpp`):

**Edge Data** (`utils/diversity.hpp`):
| Struct | Description |
|--------|-------------|
| `pd_edge_data<Weight, Probability>` | Standard edge data with weight and inheritance probability |
| `GetEdgeData` | Default functor extracting data from edges |

**Score Utilities** (`utils/diversity.hpp`):
| Class | Description |
|-------|-------------|
| `pd_score_util_w<EdgeData, FuncWeight>` | Weight extraction utilities |
| `pd_score_util_p<EdgeData, FuncIProb>` | Inheritance probability extraction |
| `pd_score_util_wp<...>` | Combined weight and probability |

**Diversity Modules** (`utils/diversity.hpp`):
| Class | Description |
|-------|-------------|
| `pd_network_diversity<Network, FuncWeight, FuncIProb>` | Expected feature survival (NP-hard, brute-force) |
| `pd_average_tree<Network, FuncWeight, FuncIProb>` | Expected weight of random displayed tree |
| `pd_average_tree_DP<...>` | Average tree diversity with DP optimization |
| `pd_tree_diversity<Network, FuncWeight>` | Standard tree PD (sum of edge weights above leaves) |
| `pd_fair_proportion<Network, FuncWeight, FuncIProb>` | Network Fair Proportion index |
| `pd_tree_fair_proportion<Network, FuncWeight>` | Tree Fair Proportion (simplified) |
| `pd_average_fair_proportion<...>` | Expected FP over all switchings |
| `pd_tree_shapeley<Network, FuncWeight>` | Shapley index on trees |
| `pd_average_shapeley<...>` | Expected Shapley over switchings |
| `pd_subnet_diversity<Network, FuncWeight>` | Sum of weights on all root-leaf-paths |

**Path Length Modules** (`utils/diversity.hpp`):
| Class | Description |
|-------|-------------|
| `pd_ML_path_lengths<Network, ProbWeightGetter>` | Maximum-likelihood path lengths |
| `pd_expected_path_lengths<Network, ProbWeightGetter>` | Expected path lengths |
| `pd_ML<Network, FuncWeight, FuncIProb>` | Most-likely switching extractor |

**Average Tree Diversity Engine** (`utils/diversity_avg_tree.hpp`):
| Class | Description |
|-------|-------------|
| `TreeDiversity<Network, Util, LeafTable>` | Tree diversity optimization engine |


## Executables

Examples are in `examples/` and build to the root directory:

| Executable | Purpose | Usage |
|------------|---------|-------|
| `tests` | Unit tests for library components | `./tests -a` runs all tests |
| `iso` | Network isomorphism checker | `./iso [-v] <file1> [file2]` |
| `tc` | Tree-containment checker | `./tc [-v] <file1> [file2]` |
| `gen` | Generate random networks | `./gen [-v] [-n <num_nodes>] [-r <reticulations>] [-l <leaves>] [file]` |
| `scanwidth` | Compute minimum-width extension tree | `./scanwidth [-pp] <file>` |
| `diversity` | Compute diversity measures | `./diversity <options> <file>` |
| `cuts` | Cut-related algorithms | `./cuts <file>` |
| `node_data` | Node data extraction | `./node_data <file>` |
| `parsimony` | Parsimony calculations | `./parsimony <file>` |
| `undir` | Undirected network operations | `./undir <file>` |
| `branch_len` | Branch length computations | `./branch_len <file>` |

Run any executable with `-h` or `--help` for full usage information.

## Testing

Run unit tests: `./tests -a` (runs all tests)
Individual test flags: `-s` (singleton), `-h` (vector_hash), `-m` (vector_map), `-d` (DFS), `-b` (biconnected components), etc.

Scripts:
- `test_sw.sh` - Scanwidth testing with generated networks
- `test_tc.sh` - Tree containment testing


### Usage Patterns

```cpp
// Layer 1: Generic append - dispatches to appropriate method
std::vector<int> vec;
mstd::append(vec, 1, 2, 3); // calls emplace_back

std::unordered_map<int, std::string> map;
mstd::append(map, 1, "one"); // calls try_emplace

// Layer 3: vector_hash - flat hash set
mstd::vector_hash<int> my_set;
my_set.emplace(42);

// Layer 3: sorted_vector - set as sorted vector
mstd::sorted_vector<int> my_set;
my_set.emplace(42); // O(n) insert, O(log n) find

// Layer 3: static_capacity_vector - stack-allocated buffer
mstd::static_capacity_vector<int, 16> small_buf;

// Layer 4: Subset iteration
std::vector<int> items{1, 2, 3, 4, 5};
for (auto subset : mstd::BoundedSubsetFactory{items, 2, 3}) {
  // subset has 2 or 3 elements
}

// Layer 4: Transforming iterator
mstd::transforming_iterator it(base_iter, [](int x) { return x * 2; });

// Layer 4: Filtered iterator
mstd::_filtered_iterator filt_iter(base, [](int x) { return x > 0; });

// Layer 5: SolutionAccumulator - keep best 10 solutions
mstd::SolutionAccumulator<std::vector<int>, double> accu(10, std::greater{});
```
