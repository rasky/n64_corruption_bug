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

uint32_t NullConversion(uint32_t in){
    return in;
}

uint32_t PrimeSizeConversion(uint32_t in){
    return 4096 >> in;
}

const test_param_info_t param_info[P_COUNT] = {
    {"Prime size", NULL, PrimeSizeConversion, 9, TEST_FLAG_NONE},
    {"Zeros in 32b prime pattern", NULL, NullConversion, 33, TEST_FLAG_NONE},
    {"Prime device", device_labels, NullConversion, PRIME_DEVICE_COUNT, TEST_FLAG_NONE},
    {"Prime direction", dir_labels, NullConversion, PRIME_DIR_COUNT, TEST_FLAG_NONE},
    {"Trigger mode", tmode_labels, NullConversion, TRIGGER_MODE_COUNT, TEST_FLAG_NONE},
    {"Offsets", NULL, NullConversion, 1024, TEST_FLAG_EDIT_POWER2},
    {"Repeats", NULL, NullConversion, 1024, TEST_FLAG_EDIT_POWER2},
};
test_param_state_t param_state[P_COUNT] = {
    {1, 0, -1},
    {1, 0, -1},
    {
        (1 << PRIME_DEVICE_DMEM) | (1 << PRIME_DEVICE_IMEM) | (1 << PRIME_DEVICE_PI),
        PRIME_DEVICE_DMEM, -1
    },
    {
        (1 << PRIME_DIR_RDRAM2RCP) | (1 << PRIME_DIR_RCP2RDRAM),
        PRIME_DIR_RDRAM2RCP, -1
    },
    {
        (1 << TRIGGER_MODE_DCACHE_READ) | (1 << TRIGGER_MODE_DCACHE_WRITE),
        TRIGGER_MODE_DCACHE_READ, -1
    },
    {128, 0, -1},
    {8, 0, -1}
};

bool test_running = false;
bool test_all_disabled = false;
uint32_t cc_after_prime;
uint32_t cc_after_trigger;

inline uint32_t has_hit_real_max(int32_t p, uint32_t current){
    return current >= (param_info[p].value_labels != NULL ? 
        param_info[p].max : param_state[p].selected);
}

void test_reset() {
    for(int32_t p=P_COUNT-1; p>=0; --p){
        param_state[p].current = -2;
        param_state[p].real = -2;
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
    test_reset();
    
    uint32_t test_count = 0;
    uint32_t last_render_ticks = TICKS_READ();
    
    while(true){
        uint32_t ticks = TICKS_READ();
        if(TICKS_DISTANCE(last_render_ticks, ticks) > TICKS_PER_SECOND / 10){
            tui_render();
            last_render_ticks = ticks;
        }
        if(!test_running) continue;
        
        // Increment and validate parameters. All parameters are always
        // validated (moved to the next valid value if they are currently
        // invalid), but the increments go from the highest-numbered parameter
        // like a carry.
        bool dir_or_above_changed = false;
        bool incrementNextParam = true;
        test_all_disabled = false;
        for(int32_t p=P_COUNT-1; p>=0; --p){
            bool changed = false;
            uint32_t seenZeroCount = 0;
            while(true) {
                if(incrementNextParam){
                    ++param_state[p].current;
                    changed = true;
                    incrementNextParam = false;
                }
                if(has_hit_real_max(p, param_state[p].current)){
                    param_state[p].current = 0;
                    changed = true;
                    incrementNextParam = true;
                    ++seenZeroCount;
                }
                if(param_info[p].value_labels == NULL) break;
                // For enums, find the next one which is selected
                if(param_state[p].selected & (1 << param_state[p].current)) break;
                // Not found a selected one yet. If we've looped twice, give up.
                if(seenZeroCount >= 2){
                    test_all_disabled = true;
                    break;
                }
                incrementNextParam = true; // not next one, this one
            }
            if(changed && p <= P_DIR) dir_or_above_changed = true;
        }
        for(int32_t p=P_COUNT-1; p>=0; --p){
            param_state[p].real = param_info[p].conversion(param_state[p].current);
        }
        if(test_all_disabled) continue;
        
        if(dir_or_above_changed){
            uint32_t pattern = 0xFFFFFFFF << param_state[P_ZEROS].real;
            prime_init(param_state[P_DEVICE].real, param_state[P_DIR].real,
                param_state[P_SIZE].real, pattern);
            dir_or_above_changed = false;
        }
        
        uint32_t offset = integer_hash(param_state[P_OFFSETS].real) & 0xFFF0;
        uint32_t* taddr = trigger_get_addr(offset);
        prefill_icache();
        xact_critical_section(param_state[P_DEVICE].real, param_state[P_DIR].real,
            param_state[P_SIZE].real, param_state[P_TMODE].real, taddr);
        trigger_after(param_state[P_TMODE].real, taddr);
        detect_per_test(param_state[P_DEVICE].real, param_state[P_DIR].real,
            param_state[P_TMODE].real, taddr);
        
        // If this is the end of the set of write mode tests (even if the next
        // set of tests is also write mode because read mode is disabled), check
        // for bad writes which went elsewhere in RDRAM. Determining whether to
        // run this scan is VERY annoying: the only way to tell if we're on the
        // last iteration of write mode is to simulate what the increments will
        // be without committing them. I've tried every possible way of sticking
        // this into the actual increment system above, but there's just too
        // many cases where we're not actually running, or it's the first
        // iteration and there isn't a previous device/dir to attribute detected
        // errors to, or we've aborted the incrementing of tmode because none
        // are selected but this happened to leave the tmode counter on write
        // mode.
        incrementNextParam = true;
        for(int32_t p=P_COUNT-1; p>P_TMODE; --p){
            if(!has_hit_real_max(p, param_state[p].current + 1)){
                incrementNextParam = false;
                break;
            }
        }
        // Now the value of incrementNextParam going into P_TMODE
        if(incrementNextParam && param_state[P_TMODE].real == TRIGGER_MODE_DCACHE_WRITE){
            detect_full_scan(param_state[P_DEVICE].real, param_state[P_DIR].real);
        }
        
        ++test_count;
    }
}
