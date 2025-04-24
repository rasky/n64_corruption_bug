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
    // Currently __bss_end is  0x8001e0c0
    most_of_dram     = (uint32_t*)0x80080000;
    most_of_dram_end = (uint32_t*)(is_memory_expanded() ? 0x80780000 : 0x80380000);
    // libdragon allocates some stuff at the very end of RAM
    for(uint32_t* a = most_of_dram; a < most_of_dram_end; ++a){
        *a = __integer_hash((uint32_t)a);
    }
}

uint32_t* trigger_get_addr(uint32_t offset){
    uint32_t center_addr = is_memory_expanded() ? 0x80680000 : 0x80280000;
    center_addr += offset & ~15;
    return (uint32_t*)center_addr;
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
