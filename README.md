# phylo_tools
Library to work with phylogenetic networks aiming at pure performance

# Preliminaries

`extended Newick` refers to [this format](https://www.ncbi.nlm.nih.gov/pubmed/19077301)  
`edgelist` refers to a list of pairs of non-negative integers (one pair per line) separated by any positive number of whitespaces where each integer is smaller than the total number of integers used

# Examples

For each example `x`, `x -h` or `x --help` will give usage information.

### iso
`iso` is a network isomorphism checker. Invoke `iso [-v] <file1> [file2]` where either `file1` describes 2 networks (in extended Newick, 1 per line), or `file1` and `file2` both describe a network (either in extended Newick or as an edgelist); `-v` shows a representation of the two Newick trees.

### tc
`tc` is a tree-containment checker. Invoke `tc [-v] <file1> [file2]` where either `file1` describes a network and a tree (both in extended Newick, 1 per line), or one of `file1` and `file2` describes a network and the other a tree (either in extended Newick or as an edgelist); `-v` shows a representation of the two Newick trees.

### gen
`gen` is a generator for binary phylogenetic networks. Invoke `gen [-v] [-n <num nodes>] [-r <num reticulations>] [-l <num leaves>] [file]`
to write a random network with `<num nodes>` nodes (`num reticulations` and `num leaves` of them being reticulations and leaves, respectively) to `file` (or standard out if omitted) in extended Newick format. `-v` shows a representation of the network.


