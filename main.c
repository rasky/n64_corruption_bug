#include <libdragon.h>
#include "prime.h"
#include "trigger.h"
#include "xact_critical_section.h"

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

void run_test(uint32_t iters,
        uint8_t device, uint8_t dir, uint32_t size, void* prime_ram,
        uint8_t tmode, uint32_t offset){
    //debugf("\nRunning test %ld times\nPrime: dev %d dir %d size %ld\nTrigger: mode %d offset %08lX\n",
    //    iters, device, dir, size, tmode, offset);
    uint32_t* taddr = trigger_get_addr(offset);
    test_device = device;
    test_dir = dir;
    test_tmode = tmode;
    prefill_icache();
    for (uint32_t i=0; i<iters; ++i) {
        xact_critical_section(device, dir, size, prime_ram, tmode, taddr);
        trigger_detect(tmode, taddr);
    }
}

int main() {
    disable_interrupts();
    debug_init_usblog();
    debugf("\033[2J\033[HInitializing...\n");

    const uint32_t size = 4096;
    uint8_t* prime_ram = malloc_uncached(size);
    const uint32_t pattern = 0xFFFFFFFF;
    
    trigger_init();
    
    uint32_t test_count = 0;
    while(test_count < 100000){
        for(uint8_t device = PRIME_DEVICE_DMEM; device <= PRIME_DEVICE_PI; ++device){
            for(uint8_t dir = PRIME_DIR_RDRAM2RCP; dir <= PRIME_DIR_RCP2RDRAM; ++dir){
                prime_init(device, dir, size, prime_ram, pattern);
                for(uint8_t tmode = TRIGGER_DCACHE_READ; tmode <= TRIGGER_DCACHE_WRITE; ++tmode){
                    for(uint32_t offset_count = 0; offset_count < 128; ++offset_count){
                        if((test_count & 15) == 0){
                            trigger_tui_render();
                        }
                        uint32_t offset = integer_hash(test_count) & 0xFFF0;
                        run_test(8, device, dir, size, prime_ram, tmode, offset);
                        ++test_count;
                    }
                }
            }
        }
    }
    
    debugf("Gave up\n");
    dummy_function_end();
}
