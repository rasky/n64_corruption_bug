#include <libdragon.h>
#include "trigger.h"

#include "hashes.h"

uint32_t* most_of_dram;
uint32_t* most_of_dram_end;

__attribute__ ((always_inline))
inline void whole_dcache_read_makedirty(uint32_t* addr){
    // Read and then rewrite one value every 16 bytes starting at addr, causing
    // each dcache line starting there to get read and marked dirty.
    for(uint32_t line = 0; line < DCACHE_LINE_COUNT; ++line){
        uint32_t val = *addr;
        asm volatile("\tsw %0, (0)(%1)\n"::"r"(val), "r"(addr) : "memory");
        addr += DCACHE_LINE_SIZE / sizeof(uint32_t);
    }
}

__attribute__ ((always_inline))
inline void whole_dcache_writeifdirty_inval(uint32_t* addr){
    // Index writeback invalidate the whole dcache starting at addr.
    for(uint32_t line = 0; line < DCACHE_LINE_COUNT; ++line){
        asm("\tcache %0,(%1)\n"::"i"(0x01), "r"((uint32_t)addr));
        addr += DCACHE_LINE_SIZE / sizeof(uint32_t);
    }
}

void trigger_init(){
    // Currently __bss_end is  0x80023b00, and we have placed the stack at
    // 0x80040000 (grows downward).
    most_of_dram     = (uint32_t*)0x80040000;
    most_of_dram_end = (uint32_t*)(0x80000000 + get_memory_size());
    for(uint32_t* a = most_of_dram; a < most_of_dram_end; ++a){
        *a = __integer_hash((uint32_t)a);
    }
}

uint32_t* trigger_get_addr(uint32_t hash){
    // The max RDRAM address is 0x807FFFFF so we don't care about the upper 9
    // bits; the lower 4 bits have to be always 0 for DMAs and cache coherency;
    // and we will assume the next 12 bits from the bottom are random based on
    // the hash. So we only care about bits 16-22 inclusive here, the "7F" in
    // the example above.
    // Code, bss, stack, etc. are up to 0x80040000 -> 0x04 in our system. So if
    // bits 18-22 all end up cleared, that is ~0x7C, code gets overwritten and
    // the test ROM crashes. So we want to keep at least one of these bits set,
    // even in the presence of one or more address bit clears. But we also don't
    // want to just always set all of them, cause then we will never detect the
    // rarer but possible bit sets in those places.
    // So we will clear up to 2 of these bits, leaving the other 3+ set.
    // Left shift this pattern, which has a 0 every 8 bits, by 0-15 places.
    // This gives a 1/8 chance of clearing each bit and a 3/8 chance of none.
    // Do that twice and this will clear up to 2 bits.
    //                      ---------*****rrrrrrrrrrrrrr0000 (r = random)
    const uint32_t mask = 0b00000000011111110111111101111111;
    uint32_t addr = ((uint32_t)most_of_dram_end - 1) & 0x007C0000;
    addr &= mask << (hash & 15); // lowest 4 bits don't go in the address
    addr &= mask << ((hash >> 18) & 15); // addr uses random bits 4-17 inclusive
    addr |= hash & 0x0003FFF0;
    // DCACHE dump has to fit here, size 0x2000.
    uint32_t end = (uint32_t)most_of_dram_end - 0x2000;
    if(addr > end) addr = end - 0x10; // more addr bits set in overflow case
    return (uint32_t*)(0x80000000 + addr);
}

void trigger_setup(uint8_t mode, uint32_t* addr){
    if(mode == TRIGGER_MODE_DCACHE_READ){
        // Don't care about the writing, just need all invalid after.
        whole_dcache_writeifdirty_inval(addr);
    }else{
        // Don't care about the reading, just need all dirty and correct data.
        whole_dcache_read_makedirty(addr);
    }
}

void trigger_go(uint8_t mode, uint32_t* addr){
    if(mode == TRIGGER_MODE_DCACHE_READ){
        // Read and then dirty each cacheline without changing its data.
        whole_dcache_read_makedirty(addr);
    }else{
        // Write each cacheline; previously made dirty. Also need it to be
        // invalidated after so the detect code reads from RDRAM.
        whole_dcache_writeifdirty_inval(addr);
    }
}

void trigger_after(uint8_t mode, uint32_t* addr){
    if(mode == TRIGGER_MODE_DCACHE_READ){
        // Writeback so we can see the cache state; previously made dirty.
        whole_dcache_writeifdirty_inval(addr);
    }
}
