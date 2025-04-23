#include <libdragon.h>
#include "test.h"

#include "detect.h"
#include "hashes.h"
#include "prime.h"
#include "trigger.h"
#include "tui.h"
#include "xact_critical_section.h"

uint32_t cc_after_prime;
uint32_t cc_after_trigger;
uint8_t test_device;
uint8_t test_dir;
uint8_t test_tmode;

void prefill_icache() {
    // Check the memory map to ensure that these are really the beginning and
    // end of the code which will be executed during the test.
    uint32_t code_start_addr = (uint32_t)(void*)&prefill_icache;
    uint32_t code_end_addr = (uint32_t)(void*)&dummy_function_end;
    code_start_addr &= ~15;
    code_end_addr = (code_end_addr + 15) & ~15;
    assert(code_end_addr - code_start_addr < 16 * 1024);
    for(uint32_t a = code_start_addr; a < code_end_addr; a += 32){
        // Fill icache
        asm("\tcache %0,(%1)\n"::"i"(0x14), "r"(a));
    }
}

void test_main() {
    const uint32_t size = 4096;
    uint8_t* prime_ram = malloc_uncached(size);
    const uint32_t pattern = 0xFFFFFFFF;
    
    trigger_init();
    
    uint32_t test_count = 0;
    while(true){
        for(uint8_t device = PRIME_DEVICE_DMEM; device <= PRIME_DEVICE_PI; ++device){
            for(uint8_t dir = PRIME_DIR_RDRAM2RCP; dir <= PRIME_DIR_RCP2RDRAM; ++dir){
                prime_init(device, dir, size, prime_ram, pattern);
                for(uint8_t tmode = TRIGGER_DCACHE_READ; tmode <= TRIGGER_DCACHE_WRITE; ++tmode){
                    for(uint32_t offset_i = 0; offset_i < 128; ++offset_i){
                        if((test_count & 15) == 0){
                            tui_render();
                        }
                        uint32_t offset = integer_hash(test_count) & 0xFFF0;
                        uint32_t* taddr = trigger_get_addr(offset);
                        test_device = device;
                        test_dir = dir;
                        test_tmode = tmode;
                        prefill_icache();
                        for (uint32_t i=0; i<8; ++i) {
                            xact_critical_section(device, dir, size, prime_ram, tmode, taddr);
                            trigger_after(tmode, taddr);
                            detect_per_test(tmode, taddr);
                        }
                        ++test_count;
                    }
                    if(tmode == TRIGGER_DCACHE_WRITE){
                        // When done with write mode, check for bad writes which
                        // went elsewhere in RDRAM.
                        detect_full_scan();
                    }
                }
            }
        }
    }
}
