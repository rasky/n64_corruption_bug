#include <libdragon.h>
#include "hashes.h"

uint32_t integer_hash(uint32_t x){
    return __integer_hash(x);
}

uint32_t integer_hash_inverse(uint32_t x){
    return __integer_hash_inverse(x);
}
