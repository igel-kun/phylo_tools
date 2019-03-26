
#pragma once

#include <cstdint>
#include <string.h>
#include "utils.hpp"


#define BITSET_FULL_BUCKET ULONG_MAX
#define BITSET_BUCKET_TYPE unsigned long
#define BITSET_BYTES_IN_BUCKET (sizeof(BITSET_BUCKET_TYPE))
#define BITSET_BITS_IN_BUCKET (CHAR_BIT * BITSET_BYTES_IN_BUCKET)
#define BUCKET_OF(x) (x / BITSET_BITS_IN_BUCKET)

namespace std {
      
  class bitset_iterator;

  // ATTENTION: this does not do error checking if NDEBUG is on (except front())
  class iterable_bitset 
  {
    typedef BITSET_BUCKET_TYPE storage_type;
    
    uint64_t num_bits;
    storage_type* storage;

    inline uint64_t num_buckets() const
    {
      return 1ul + BUCKET_OF(num_bits);
    }

    public:
      iterable_bitset(const iterable_bitset& bs):
        num_bits(bs.num_bits),
        storage(num_bits ? (storage_type*)malloc(num_buckets() * BITSET_BYTES_IN_BUCKET) : nullptr)
      {
        if(storage) memcpy(storage, bs.storage, num_buckets() * BITSET_BYTES_IN_BUCKET);
      }

      iterable_bitset(const uint64_t _num_bits, const bool _set_all):
        num_bits(_num_bits),
        storage(num_bits ? (storage_type*)calloc(num_buckets(), BITSET_BYTES_IN_BUCKET) : nullptr)
      {
        if(_set_all) set_all();
      }
      iterable_bitset(const uint64_t _num_bits = 0):
        iterable_bitset(_num_bits, 0)
      {
      }

      ~iterable_bitset()
      {
        if(storage) free(storage);
      }

      void insert(const uint64_t x)
      {
        set(x);
      }

      void erase(const uint64_t x)
      {
        clear(x);
      }

      bool test(const uint64_t x) const
      {
        assert(x < num_bits);
        return (storage[BUCKET_OF(x)] >> (x % BITSET_BITS_IN_BUCKET)) & 1;
      }

      void set(const uint64_t x, const bool value)
      {
        if(value) set(x); else clear(x);
      }

      void set(const uint64_t x)
      {
        assert(x < num_bits);
        storage[BUCKET_OF(x)] |= (1ul << (x % BITSET_BITS_IN_BUCKET));
      }

      void clear(const uint64_t x)
      {
        assert(x < num_bits);
        storage[BUCKET_OF(x)] &= ~(1ul << (x % BITSET_BITS_IN_BUCKET));
      }

      void flip(const uint64_t x)
      {
        assert(x < num_bits);
        storage[BUCKET_OF(x)] ^= (1ul << (x % BITSET_BITS_IN_BUCKET));
      }

      void set_all()
      {
        uint64_t bits = num_bits;
        uint64_t i = 0;
        while(bits > BITSET_BITS_IN_BUCKET){
          storage[i++] = BITSET_FULL_BUCKET;
          bits -= BITSET_BITS_IN_BUCKET;
        }
        storage[i] = BITSET_FULL_BUCKET >> (BITSET_BITS_IN_BUCKET - bits);
      }

      //! set the k'th unset bit (k = 0 corresponding to the first unset bit), and return its index
      uint64_t set_kth_unset(uint64_t k)
      {
        uint64_t result= get_index_of_kth_zero(k);
        set(result);
        return result;
        
      }

      uint64_t clear_kth_set(uint64_t k)
      {
        uint64_t result= get_index_of_kth_one(k);
        clear(result);
        return result;
      }
      
      uint64_t get_index_of_kth_zero(uint64_t k) const
      {
        uint64_t i = 0;
        uint64_t z;
        while(1){
          if(i == num_buckets()) throw std::out_of_range("not enough unset bits");
          z = NUM_ZEROS_INL(storage[i]);
          if(k >= z){
            k -= z;
            ++i;
          } else break;
        }
        BITSET_BUCKET_TYPE& buffer = storage[i];
        uint32_t width = BITSET_BITS_IN_BUCKET / 2;
        uint32_t j = width;
        // using binary seach, find the index j such that the k'th unset bit is at position j
        while(width > 1){
          width /= 2;
          j = (NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
        }
        // j might still be off by one in the end
        if(NUM_ZEROS_IN_LOWEST_K_BITL(j, buffer) > k) --j;
        return BITSET_BITS_IN_BUCKET * i + j;
      }
      
      uint64_t get_index_of_kth_one(uint64_t k) const
      {
        uint64_t i = 0;
        uint64_t z;
        while(1){
          if(i == num_buckets()) throw std::out_of_range("not enough set bits");
          z = NUM_ONES_INL(storage[i]);
          if(k >= z){
            k -= z;
            ++i;
          } else break;
        }
        BITSET_BUCKET_TYPE& buffer = storage[i];
        uint32_t width = BITSET_BITS_IN_BUCKET / 2;
        uint32_t j = width;
        // using binary seach, find the index j such that the k'th unset bit is at position j
        while(width > 1){
          width /= 2;
          j = (NUM_ONES_IN_LOWEST_K_BITL(j, buffer) > k) ? j - width : j + width;
        }
        // j might still be off by one in the end
        if(NUM_ONES_IN_LOWEST_K_BITL(j, buffer) > k) --j;
        return BITSET_BITS_IN_BUCKET * i + j;
      }
      
      void clear()
      {
        clear_all();
      }
      
      void clear_all()
      {
        explicit_bzero(storage, num_buckets() * BITSET_BYTES_IN_BUCKET);
      }
      
      void flip_all()
      {
        uint64_t bits = num_bits;
        uint64_t i = 0;
        while(bits > BITSET_BITS_IN_BUCKET){
          storage[i++] ^= BITSET_FULL_BUCKET;
          bits -= BITSET_BITS_IN_BUCKET;
        }
        storage[i] ^= BITSET_FULL_BUCKET >> (BITSET_BITS_IN_BUCKET - bits);
      }
      
      void invert()
      {
        flip_all();
      }
      
      uint64_t capacity() const
      {
        return num_bits;
      }
      
      uint64_t size() const
      {
        return count();
      }
      
      uint64_t count() const
      {
        uint64_t accu = 0;
        for(uint64_t i = 0; i < num_buckets(); ++i)
          accu += NUM_ONES_INL(storage[i]);
        return accu;
      }
      
      //! count the items whose value is at least x
      uint64_t count_larger(const uint64_t x) const
      {
        if(x <= num_bits){
          const uint64_t first_bucket = BUCKET_OF(x);
          const uint64_t first_offset = x % BITSET_BITS_IN_BUCKET;
          uint64_t accu = NUM_ONES_INL(storage[first_bucket] >> first_offset);
          for(uint64_t i = first_bucket + 1; i < num_buckets(); ++i)
            accu += NUM_ONES_INL(storage[i]);
          return accu;
        } else return 0;
      }
      
      //! count the items whose value is at most x
      uint64_t count_smaller(const uint64_t x) const
      {
        if(x < num_bits){
          const uint64_t last_bucket = BUCKET_OF(x);
          const uint64_t bits_in_last_bucket = x % BITSET_BITS_IN_BUCKET;
          uint64_t accu = 0;
          for(uint64_t i = 0; i < last_bucket; ++i)
            accu += NUM_ONES_INL(storage[i]);
          accu += NUM_ONES_INL(storage[last_bucket] << (BITSET_BITS_IN_BUCKET - bits_in_last_bucket));
          return accu;
        } else return count();
      }

      bool empty() const
      {
        for(uint64_t i = 0; i < num_buckets(); ++i)
          if(storage[i] != 0) return false;
        return true;
      }
      
      uint64_t front() const
      {
        uint64_t result = 0;
        for(uint64_t i = 0; i < num_buckets(); ++i)
          if(storage[i] == 0) 
            result += BITSET_BITS_IN_BUCKET;
          else
            return result + NUM_TRAILING_ZEROSL(storage[i]);
        throw out_of_range("front() on empty bitset");
      }

      iterable_bitset& operator=(const iterable_bitset& bs)
      {
        if(bs.num_bits > num_bits){
          num_bits = bs.num_bits;
          free(storage);
          storage = (storage_type*)malloc(num_buckets() * BITSET_BYTES_IN_BUCKET);
        }
        for(uint64_t i = 0; i < num_buckets(); ++i)
          storage[i] = bs.storage[i];
        return *this;
      }
      
      bool operator==(const iterable_bitset& bs)
      {
        const uint64_t min_buckets = std::min(num_buckets(), bs.num_buckets());
        for(uint64_t i = 0; i < min_buckets; ++i)
          if(storage[i] != bs.storage[i]) return false;
        for(uint64_t i = min_buckets + 1; i < num_buckets(); ++i)
          if(storage[i] != 0) return false;
        for(uint64_t i = min_buckets + 1; i < bs.num_buckets(); ++i)
          if(bs.storage[i] != 0) return false;
        return true;
      }
      
      iterable_bitset& operator|=(const iterable_bitset& bs)
      {
        const uint64_t min_buckets = std::min(num_buckets(), bs.num_buckets());
        for(uint64_t i = 0; i < min_buckets; ++i)
          storage[i] |= bs.storage[i];
        return *this;
      }
      
      iterable_bitset& operator&=(const iterable_bitset& bs)
      {
        const uint64_t min_buckets = std::min(num_buckets(), bs.num_buckets());
        for(uint64_t i = 0; i < min_buckets; ++i)
          storage[i] &= bs.storage[i];
        for(uint64_t i = min_buckets + 1; i < num_buckets(); ++i)
          storage[i] = 0;
        return *this;
      }
      
      iterable_bitset& operator^=(const iterable_bitset& bs)
      {
        const uint64_t min_buckets = std::min(num_buckets(), bs.num_buckets());
        for(uint64_t i = 0; i < min_buckets; ++i)
          storage[i] ^= bs.storage[i];
        for(uint64_t i = min_buckets + 1; i < num_buckets(); ++i)
          storage[i] = ~storage[i];
        return *this;
      }



      friend class bitset_iterator;
      friend std::ostream& operator<<(std::ostream& os, const iterable_bitset& bs);

      bitset_iterator begin() const;
      bitset_iterator end() const;
      bitset_iterator find(const uint64_t x) const;
  };

  std::ostream& operator<<(std::ostream& os, const iterable_bitset& bs)
  {
    for(uint64_t i = bs.capacity(); i != 0;) std::cout << (bs.test(--i) ? '1' : '0');
    return os << " ("<<bs.num_buckets()<<" buckets, "<<bs.capacity()<<" bits)";
  }



  // NOTE: we do not correspond to the official standard since we do not abide by the following condition:
  // "if a and b compare equal then either they are both non-dereferenceable or *a and *b are references bound to the same object"
  // since our *-operation does not return a reference, but an integer
  // however, iteration a la "for(auto i: my_set)" works very well...
  class bitset_iterator: public std::iterator<std::forward_iterator_tag, uint64_t>
  {
    const iterable_bitset& target;
    uint64_t index;
    iterable_bitset::storage_type buffer;

  public:
    bitset_iterator(const iterable_bitset& _target, const uint64_t _index = 0):
      target(_target), index(_index), buffer(_target.storage[_index])
    {
    }
    // create an iterator at a specific bit inside target[index]
    bitset_iterator(const iterable_bitset& _target, const uint64_t _index, const char sub_index):
      target(_target), index(_index), buffer(_target.storage[_index])
    {
      buffer &= (BITSET_FULL_BUCKET << sub_index);
    }

    bool is_valid() const
    {
      return index < target.num_buckets();
    }

    operator bool() const
    {
      return is_valid();
    }

    uint64_t operator*() const
    {
      return index * BITSET_BITS_IN_BUCKET + NUM_TRAILING_ZEROSL(buffer);
    }

    bitset_iterator& operator++()
    {
      buffer ^= (1ul << NUM_TRAILING_ZEROSL(buffer));
      while((buffer == 0) && is_valid())
        buffer = target.storage[++index];
      return *this;
    }

    bitset_iterator operator++(int)
    {
      bitset_iterator result(*this);
      ++(*this);
      return result;
    }

    bool operator==(const bitset_iterator& it) const
    {
      const bool we_at_end = !is_valid();
      const bool they_at_end = !it.is_valid();
      if(we_at_end != they_at_end) return false;
      if(we_at_end && they_at_end) return true;
      return (index == it.index) && (buffer == it.buffer);
    }

    bool operator!=(const bitset_iterator& it) const
    {
      return !this->operator==(it);
    }
  };

  bitset_iterator iterable_bitset::begin() const
  {
    assert(!empty());
    return bitset_iterator(*this);
  }

  bitset_iterator iterable_bitset::end() const
  {
    return bitset_iterator(*this, num_buckets());
  }

  bitset_iterator iterable_bitset::find(const uint64_t x) const
  {
    return test(x) ? bitset_iterator(*this, x / BITSET_BITS_IN_BUCKET, x % BITSET_BITS_IN_BUCKET) : end();
  }

}
