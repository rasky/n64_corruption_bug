#include <libdragon.h>
#include "trigger.h"

static uint32_t* mem_area;
static uint32_t* mem_area_end;

#define DCACHE_LINE_SIZE 16
#define DCACHE_LINE_COUNT 512
#define DCACHE_SIZE_BYTES (DCACHE_LINE_SIZE*DCACHE_LINE_COUNT)

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

uint32_t integer_hash(uint32_t x){
    return __integer_hash(x);
}

/** Inverse of above hash function, used to convert corrupted data to the
address that data belonged to. */
uint32_t integer_hash_inverse(uint32_t x) {
    x ^= x >> 16;
    x *= 0x43021123;
    x ^= x >> 15 ^ x >> 30;
    x *= 0x1d69e2a5;
    x ^= x >> 16;
    return x;
}

uint32_t __popcnt(uint32_t x){
    uint32_t ret = 0;
    for(int32_t i=0; i<32; ++i){
        if(x & 1) ++ret;
        x >>= 1;
    }
    return ret;
}

void trigger_init_bank(uint8_t bank){
    if(bank == 0){
        // Currently __bss_end is  0x8001e0c0
        mem_area     = ((uint32_t*)0x80080000);
        mem_area_end = ((uint32_t*)0x80100000);
    }else if(bank == 1){
        mem_area     = ((uint32_t*)0x80100000);
        mem_area_end = ((uint32_t*)0x80200000);
    }else if(bank == 2){
        mem_area     = ((uint32_t*)0x80200000);
        mem_area_end = ((uint32_t*)0x80300000);
    }else if(bank == 3){
        mem_area     = ((uint32_t*)0x80300000);
        mem_area_end = ((uint32_t*)0x80380000);
        // libdragon allocates some stuff at the very end of RAM
    }else{
        assert(0);
    }
    for(uint32_t* a = mem_area; a < mem_area_end; ++a){
        *a = __integer_hash((uint32_t)a);
    }
}

uint32_t* trigger_get_addr(uint32_t offset){
    uint32_t center_addr = (uint32_t)mem_area 
        + (((uint32_t)mem_area_end - (uint32_t)mem_area) >> 1);
    center_addr += offset & ~15;
    return (uint32_t*)center_addr;
}

void trigger_setup(uint8_t mode, uint32_t* addr){
    if(mode == TRIGGER_DCACHE_READ){
        // Don't care about the writing, just need all invalid after.
        whole_dcache_writeifdirty_inval(addr);
    }else{
        // Don't care about the reading, just need all dirty and correct data.
        whole_dcache_read_makedirty(addr);
    }
}

void trigger_go(uint8_t mode, uint32_t* addr){
    if(mode == TRIGGER_DCACHE_READ){
        // Read and then dirty each cacheline without changing its data.
        whole_dcache_read_makedirty(addr);
    }else{
        // Write each cacheline; previously made dirty. Also need it to be
        // invalidated after so the detect code reads from RDRAM.
        whole_dcache_writeifdirty_inval(addr);
    }
}

void trigger_detect(uint8_t mode, uint32_t* addr){
    if(mode == TRIGGER_DCACHE_READ){
        // Writeback so we can see the cache state; previously made dirty.
        whole_dcache_writeifdirty_inval(addr);
    }
    MEMORY_BARRIER();
    uint32_t* start;
    uint32_t* end;
    if(mode == TRIGGER_DCACHE_READ){
        // The reads might be from different places, but the writes should be
        // back to the correct area, so only check that area to speed up.
        start = addr - 4;
        end = addr + (DCACHE_SIZE_BYTES >> 2) + 4;
    }else{
        // The writes could theoretically go anywhere, but we'll check the
        // whole area.
        start = mem_area;
        end = mem_area_end;
    }
    
    bool errored = false;
    for(uint32_t* a = start; a < end; ++a){
        uint32_t golden = __integer_hash((uint32_t)a);
        uint32_t actual = *a;
        if(golden != actual){
            uint32_t data_corruption = golden ^ actual;
            if(__popcnt(data_corruption) <= 3){
                debugf("%08lX: exp %08lX got %08lX, mask %08lX\n",
                    (uint32_t)a, golden, actual, data_corruption);
            }else{
                uint32_t actual_inverse = integer_hash_inverse(actual);
                uint32_t addr_corruption = (uint32_t)a ^ actual_inverse;
                if(__popcnt(addr_corruption) <= 3){
                    debugf("%08lX had data for %08lX, mask %08lX\n",
                        (uint32_t)a, actual_inverse, addr_corruption);
                }else{
                    debugf("%08lX: exp %08lX got %08lX\n",
                        (uint32_t)a, golden, actual);
                }
            }
            *a = golden; // fix for next iter
            errored = true;
        }
    }
    assert(!errored);
}
