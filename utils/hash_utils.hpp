

#pragma once


// hashing functions from https://github.com/skeeto/hash-prospector
uint32_t uint32_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    x ^= x >> 16;
    return x;
}
uint32_t uint32_unhash(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x43021123);
    x ^= x >> 15 ^ x >> 30;
    x *= UINT32_C(0x1d69e2a5);
    x ^= x >> 16;
    return x;
}

uint64_t uint64_hash(uint64_t x) {
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  x = x ^ (x >> 31);
  return x;
}

uint64_t uint64_unhash(uint64_t x) {
	x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
	x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
	x = x ^ (x >> 30) ^ (x >> 60);
	return x;
}



uint64_t hash_combine(size_t x, const size_t y)
{
  x^= y + 0x9e3779b9 + (x << 6) + (x >> 2);
  return x;
}



