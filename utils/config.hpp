#pragma once

// a set of config options to control the internal behavior of the library
namespace PT{
  namespace config{
#warning "TODO: recheck those values in production!"
    // when merging sorted vectors, switch from linear merge to iterator-queue merge when merging (strictly) more than x vectors
    uint8_t vector_queue_merge_threshold = 3;

    // when applying reduction rules to network-containment instances,
    // apply the expensive extended cherry reduction only if N is at least x edges away from begin a tree
    uint8_t min_retis_to_apply_extended_cherry = 1;

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
