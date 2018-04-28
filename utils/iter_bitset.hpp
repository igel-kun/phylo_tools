
#pragma once

#include <string.h>


#define NUM_LEADING_ZEROS(x) __builtin_clz(x)
#define NUM_LEADING_ZEROSL(x) __builtin_clzl(x)
#define NUM_TRAILING_ZEROS(x) __builtin_ctz(x)
#define NUM_TRAILING_ZEROSL(x) __builtin_ctzl(x)
#define NUM_ONES_IN(x) __builtin_popcount(x)
#define NUM_ONES_INL(x) __builtin_popcountl(x)
#define BITSET_FULL_BUCKET ULONG_MAX
#define BITSET_BUCKET_TYPE unsigned long
#define BITSET_BUCKET_SIZE (sizeof(BITSET_BUCKET_TYPE))
#define BITSET_BUCKET_SIZE_BIT (CHAR_BIT * BITSET_BUCKET_SIZE)

namespace std {
      
  class bitset_iterator;

  // ATTENTION: THIS DOES NO ERROR CHECKING!!! 
  class iterable_bitset 
  {
    typedef BITSET_BUCKET_TYPE storage_type;
    
    uint64_t num_bits;
    storage_type* storage;

    inline uint64_t num_buckets() const
    {
      return 1ul + num_bits / BITSET_BUCKET_SIZE_BIT;
    }

    public:
      iterable_bitset(const iterable_bitset& bs):
        num_bits(bs.num_bits),
        storage(num_bits ? (storage_type*)calloc(num_buckets(), BITSET_BUCKET_SIZE) : nullptr)
      {
        if(storage) memcpy(storage, bs.storage, num_buckets() * BITSET_BUCKET_SIZE);
      }

      iterable_bitset(const uint64_t _num_bits = 0):
        num_bits(_num_bits),
        storage(num_bits ? (storage_type*)calloc(num_buckets(), BITSET_BUCKET_SIZE) : nullptr)
      {
      }

      ~iterable_bitset()
      {
        if(storage) free(storage);
      }

      bool test(const uint64_t x) const
      {
        return (storage[x / BITSET_BUCKET_SIZE_BIT] >> (x % BITSET_BUCKET_SIZE_BIT)) & 1;
      }
      void set(const uint64_t x, const bool value)
      {
        if(value) set(x); else clear(x);
      }
      void set(const uint64_t x)
      {
        storage[x / BITSET_BUCKET_SIZE_BIT] |= (1ul << (x % BITSET_BUCKET_SIZE_BIT));
      }
      void clear(const uint64_t x)
      {
        storage[x / BITSET_BUCKET_SIZE_BIT] &= ~(1ul << (x % BITSET_BUCKET_SIZE_BIT));
      }
      void flip(const uint64_t x)
      {
        storage[x / BITSET_BUCKET_SIZE_BIT] ^= (1ul << (x % BITSET_BUCKET_SIZE_BIT));
      }
      void set_all()
      {
        uint64_t bits = num_bits;
        uint64_t i = 0;
        while(bits > BITSET_BUCKET_SIZE_BIT){
          storage[i++] = BITSET_FULL_BUCKET;
          bits -= BITSET_BUCKET_SIZE_BIT;
        }
        storage[i] = BITSET_FULL_BUCKET >> (BITSET_BUCKET_SIZE_BIT - bits);
      }
      void clear_all()
      {
        for(uint64_t i = 0; i < num_buckets(); ++i)
          storage[i] = 0;
      }
      void flip_all()
      {
        uint64_t bits = num_bits;
        uint64_t i = 0;
        while(bits > BITSET_BUCKET_SIZE_BIT){
          storage[i++] ^= BITSET_FULL_BUCKET;
          bits -= BITSET_BUCKET_SIZE_BIT;
        }
        storage[i] ^= (1 << bits) - 1;
      }
      uint64_t size() const
      {
        return num_bits;
      }
      uint64_t count() const
      {
        uint64_t accu = 0;
        for(uint64_t i = 0; i < num_buckets(); ++i)
          accu += NUM_ONES_INL(storage[i]);
        return accu;
      }
      bool is_empty() const
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
            result += BITSET_BUCKET_SIZE_BIT;
          else
            return result + NUM_TRAILING_ZEROSL(storage[i]);
        return BITSET_FULL_BUCKET;
      }

      iterable_bitset& operator=(const iterable_bitset& bs)
      {
        if(bs.num_bits > num_bits){
          num_bits = bs.num_bits;
          free(storage);
          storage = (storage_type*)malloc(num_buckets() * BITSET_BUCKET_SIZE);
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
  };

  std::ostream& operator<<(std::ostream& os, const iterable_bitset& bs)
  {
    for(uint64_t i = bs.size(); i != 0;) std::cout << (bs.test(--i) ? '1' : '0');
    return os << " ("<<bs.num_buckets()<<" buckets, "<<bs.size()<<" bits)";
  }



  class bitset_iterator
  {
    const iterable_bitset& target;
    uint64_t index;
    iterable_bitset::storage_type buffer;

  public:
    bitset_iterator(const iterable_bitset& _target, const uint64_t _index = 0):
      target(_target), index(_index), buffer(_target.storage[_index])
    {}

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
      return index * BITSET_BUCKET_SIZE_BIT + NUM_TRAILING_ZEROSL(buffer);
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
  };

  bitset_iterator iterable_bitset::begin() const
  {
    return bitset_iterator(*this);
  }

  bitset_iterator iterable_bitset::end() const
  {
    return bitset_iterator(*this, num_buckets());
  }

}
