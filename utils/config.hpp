#pragma once

// a set of config options to control the internal behavior of the library
namespace mstd {
  namespace config {
#warning "TODO: recheck those values in production!"
    // when merging sorted vectors, switch from linear merge to iterator-queue merge when merging (strictly) more than x vectors
    uint8_t vector_queue_merge_threshold = 3;

    // if a vector has more than this many items, we consider it slower to do linear search on it than a set-lookup
    uint16_t linear_search_threshold = 8;

    // when selecting k of n elements at random, choose the k log k "cardchoose" method of Paul Crowley if k is below this threshold
    uint32_t cardchoose_threshold = 10;

    // how many times to try to get memory when allocating for a charp
    uint32_t charp_allocation_timeout = 1000;

    // characters to use for displaying trees/networks on the console
    // the standard ASCII set is a bit daft but only uses ASCII < 128
    // we allow using a much nicer UTF8 set
    struct Locale {
      const char* char_reti = "H"; // Vincent aime mieux le 'H'
      const char* char_no_branch_hori = "-";
      const char* char_branch_low     = "+";
      const char* char_branch_right   = "|";
      const char* char_no_branch_vert = "|";
      const char* char_last_child     = "\\";
    };
    Locale UTF8_locale {
      "H",
      "─",
      "┬",
      "├",
      "│",
      "└"
    };

    Locale locale;
}}
