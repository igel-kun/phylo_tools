#pragma once

// a set of config options to control the internal behavior of the library
namespace PT{
  namespace config{

    // when merging sorted vectors, switch from linear merge to iterator-queue merge when merging (strictly) more than x vectors
    uint8_t vector_queue_merge_threshold = 3;
}}
