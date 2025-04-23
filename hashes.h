#ifndef HASHES_H
#define HASHES_H

/** https://github.com/skeeto/hash-prospector */
__attribute__ ((always_inline))
inline uint32_t __integer_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

/** Inverse of above hash function, used to convert corrupted data to the
address that data belonged to. */
__attribute__ ((always_inline))
inline uint32_t __integer_hash_inverse(uint32_t x) {
    x ^= x >> 16;
    x *= 0x43021123;
    x ^= x >> 15 ^ x >> 30;
    x *= 0x1d69e2a5;
    x ^= x >> 16;
    return x;
}

/** Non-inline versions */
extern uint32_t integer_hash(uint32_t x);
extern uint32_t integer_hash_inverse(uint32_t x);

#endif
