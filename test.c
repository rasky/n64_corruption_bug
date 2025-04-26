#include <libdragon.h>
#include "test.h"

#include "detect.h"
#include "hashes.h"
#include "prime.h"
#include "trigger.h"
#include "tui.h"
#include "xact_critical_section.h"

static const char* const device_labels[PRIME_DEVICE_COUNT] = {"DMEM", "IMEM", "PI"};
static const char* const dir_labels[PRIME_DIR_COUNT] = {"RDRAM to RCP", "RCP to RDRAM"};
static const char* const tmode_labels[TRIGGER_MODE_COUNT] = {"DCACHE read", "DCACHE write"};

test_param_t params[P_COUNT] = {
    {"Prime size", NULL, 4096, 16, 2048,
        4096, 0, TEST_FLAG_COUNT_POWER2 | TEST_FLAG_EDIT_POWER2},
    {"Zeros in 32b prime pattern", NULL, 0, 32, 1,
        0, 1, TEST_FLAG_NONE},
    {"Prime device", device_labels, 0, PRIME_DEVICE_COUNT,
        (1 << PRIME_DEVICE_DMEM) | (1 << PRIME_DEVICE_IMEM) | (1 << PRIME_DEVICE_PI),
        PRIME_DEVICE_DMEM, 0xFF, TEST_FLAG_NONE},
    {"Prime direction", dir_labels, 0, PRIME_DIR_COUNT, 
        (1 << PRIME_DIR_RDRAM2RCP) | (1 << PRIME_DIR_RCP2RDRAM),
        PRIME_DIR_RDRAM2RCP, 0xFF, TEST_FLAG_NONE},
    {"Trigger mode", tmode_labels, 0, TRIGGER_MODE_COUNT,
        (1 << TRIGGER_MODE_DCACHE_READ) | (1 << TRIGGER_MODE_DCACHE_WRITE),
        TRIGGER_MODE_DCACHE_READ, 0xFF, TEST_FLAG_NONE},
    {"Offsets", NULL, 0, 1024, 128,
        0, 1, TEST_FLAG_EDIT_POWER2},
    {"Repeats", NULL, 0, 1024, 8,
        0, 1, TEST_FLAG_EDIT_POWER2},
};
bool test_running = false;

static bool params_changed(uint32_t mask){
    for(int32_t i=0; i<P_COUNT; ++i){
        if((mask & 1) && (params[i].current != params[i].last)){
            return true;
        }
        mask >>= 1;
    }
    return false;
}

uint32_t next_param_value(int32_t p, bool sel, bool edit, bool reverse){
    uint32_t from = sel ? params[p].selected : params[p].current;
    uint32_t flag = edit ? TEST_FLAG_EDIT_POWER2 : TEST_FLAG_COUNT_POWER2;
    if((params[p].flags & flag)){
        if((params[p].min < params[p].max) != reverse){
            return from << 1;
        }else{
            return from >> 1;
        }
    }else{
        if((params[p].min < params[p].max) != reverse){
            return from + 1;
        }else{
            return from - 1;
        }
    }
}

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
    trigger_init();
    
    uint32_t test_count = 0;
    uint32_t last_render_ticks = TICKS_READ();
    
    while(true){
        uint32_t ticks = TICKS_READ();
        if(TICKS_DISTANCE(last_render_ticks, ticks) > TICKS_PER_SECOND / 10){
            tui_render();
            last_render_ticks = ticks;
        }
        
        if(!test_running) continue;
        
        if(params_changed((1 << P_SIZE) | (1 << P_ZEROS) | (1 << P_DEVICE) | (1 << P_DIR))){
            uint32_t pattern = 0xFFFFFFFF << params[P_ZEROS].current;
            prime_init(params[P_DEVICE].current, params[P_DIR].current,
                params[P_SIZE].current, pattern);
        }
        
        uint32_t offset = integer_hash(params[P_OFFSETS].current) & 0xFFF0;
        uint32_t* taddr = trigger_get_addr(offset);
        prefill_icache();
        xact_critical_section(params[P_DEVICE].current, params[P_DIR].current,
            params[P_SIZE].current, params[P_TMODE].current, taddr);
        trigger_after(params[P_TMODE].current, taddr);
        detect_per_test(params[P_DEVICE].current, params[P_DIR].current,
            params[P_TMODE].current, taddr);
            
        for(int32_t p=P_COUNT-1; p>=0; --p){
            params[p].last = params[p].current;
        }
        for(int32_t p=P_COUNT-1; p>=0; --p){
            bool incrementNextParam = false;
            do {
                params[p].current = next_param_value(p, false, false, false);
                if(params[p].current == 
                        (params[p].value_labels != NULL ? params[p].max : params[p].selected)){
                    params[p].current = params[p].min;
                    incrementNextParam = true;
                }
            } while( // For enums, find the next one which is selected
                !incrementNextParam
                && params[p].value_labels != NULL
                && !(params[p].selected & (1 << params[p].current)));
            if(!incrementNextParam) break;
        }
        
        if(params[P_TMODE].current != params[P_TMODE].last &&
            params[P_TMODE].last == TRIGGER_MODE_DCACHE_WRITE){
            // When done with write mode, check for bad writes which
            // went elsewhere in RDRAM.
            detect_full_scan(params[P_DEVICE].current, params[P_DIR].current);
        }
        ++test_count;
    }
}
