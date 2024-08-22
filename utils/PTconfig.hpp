
#pragma once


namespace PT {
  namespace config {
    // when applying reduction rules to network-containment instances,
    // apply the expensive extended cherry reduction only if N is at least x edges away from begin a tree
    uint8_t min_retis_to_apply_extended_cherry = 1;

    // write empty node/edge data (e.g. string of length 0)
    bool write_empty_node_data = false;
    bool write_empty_edge_data = false;

    // delimeter between the node name and its label in edgelist format files
    struct delimeters {
      char start_of_node_label = ':';
      char start_of_node_data  = ':';
      char start_of_edge_data  = ':';
    } EL_delimeters;

    delimeters NW_delimeters {
      0,   // char start_of_node_label
      '|', // char start_of_node_data
      ':' // char start_of_edge_data
    };
    char data_delimeters[] = ":,;";

    // if this is set to true, then generic_data will be printed even if all items produce the empty string when printed
    bool print_empty_generic_data = false;

    // newick needs an extra hybrid specifier
    char NW_start_of_hybrid_spec = '#';
}}
