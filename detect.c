#include <libdragon.h>
#include "detect.h"

#include "hashes.h"
#include "test.h"
#include "trigger.h"

const char* yaxis_labels[YAXIS_COUNT] = {
    "Off",
    "Test failures",
    "Test passes",
    "Any corrupted words",
    "Zero words",
    "Unknown corrupted words",
    "Address bit clears",
    "Data bit clears",
    "Address bit sets",
    "Data bit sets",
};
const char* xaxis_labels[XAXIS_COUNT - XAXIS_MIN] = {
    "Bit in word",
    "Word in cacheline",
    "Cacheline in buffer",
    "RDRAM module 0 CC after prime",
    "RDRAM module 1 CC after prime",
    "RDRAM module 2 CC after prime",
    "RDRAM module 3 CC after prime",
    "RDRAM module 0 CC after trigger",
    "RDRAM module 1 CC after trigger",
    "RDRAM module 2 CC after trigger",
    "RDRAM module 3 CC after trigger",
};

plot_t plots[PLOT_COUNT] = {
    {YAXIS_TEST_FAILURES,  P_DEVICE, 32, 2, {}},
    {YAXIS_TEST_FAILURES,  P_DIR, 32, 2, {}},
    {YAXIS_TEST_FAILURES,  P_TMODE, 32, 2, {}},
    {YAXIS_BIT_SET_ADDR,   P_TMODE, 32, 2, {}},
    {YAXIS_BIT_SET_DATA,   P_TMODE, 32, 2, {}},
    {YAXIS_BIT_CLEAR_ADDR, P_TMODE, 32, 2, {}},
    {YAXIS_BIT_CLEAR_DATA, P_TMODE, 32, 2, {}},
    {YAXIS_BIT_CLEAR_ADDR, XAXIS_WORD_IDX, 32, 4, {}},
    {YAXIS_BIT_CLEAR_DATA, XAXIS_WORD_IDX, 32, 4, {}},
    {YAXIS_BIT_CLEAR_ADDR, XAXIS_BIT_IDX, 6, 32, {}},
    {YAXIS_BIT_CLEAR_DATA, XAXIS_BIT_IDX, 6, 32, {}},
    {YAXIS_BIT_CLEAR_ADDR, XAXIS_BUF_POS, 4, 50, {}},
    {YAXIS_BIT_CLEAR_DATA, XAXIS_BUF_POS, 4, 50, {}},
    {YAXIS_OFF, P_SIZE, 8, 1, {}},
    {YAXIS_OFF, P_SIZE, 8, 1, {}},
};

detect_state_t dstate;

static uint32_t popcnt(uint32_t x){
    uint32_t ret = 0;
    for(int32_t i=0; i<32; ++i){
        if(x & 1) ++ret;
        x >>= 1;
    }
    return ret;
}

static void record_event(uint8_t yaxis) {
    for(int32_t p=0; p<PLOT_COUNT; ++p){
        if(plots[p].yaxis != yaxis) continue;
        uint8_t xaxis = plots[p].xaxis;
        int32_t x = -1;
        if(xaxis < XAXIS_MIN){
            x = param_state[xaxis].current;
        }else if(xaxis == XAXIS_BIT_IDX){
            x = 31 - dstate.bit;
        }else if(xaxis == XAXIS_WORD_IDX){
            x = dstate.dword;
        }else if(xaxis == XAXIS_BUF_POS){
            /*
            int32_t za_area_offset = -1, d_area_offset = -1;
            if(mode == TRIGGER_MODE_DCACHE_READ){
                za_area_offset = (a - start) >> 2; // Number of cachelines in
                d_area_offset = ((int32_t)(a - start) * RES_AREA_SIZE) / (int32_t)(end - start);
                if(za_area_offset >= RES_AREA_SIZE) za_area_offset = RES_AREA_SIZE - 1;
                if( d_area_offset >= RES_AREA_SIZE)  d_area_offset = RES_AREA_SIZE - 1;
            }
            */
            // Ignore detections in full scan for now
            if(dstate.end - dstate.start > (DCACHE_SIZE_BYTES >> 2)) continue;
            x = (dstate.a - dstate.start) >> 2;
            if(x >= PLOT_MAX_X) x = PLOT_MAX_X - 1;
        }else if(xaxis >= XAXIS_CC_0_PRIME && xaxis <= XAXIS_CC_3_TRIGGER){
            x = (xaxis >= XAXIS_CC_0_TRIGGER) ? dstate.cc_after_trigger : dstate.cc_after_prime;
            x = (x >> ((3 - ((xaxis - XAXIS_CC_0_TRIGGER) & 3)) << 3)) & 0xFF;
            x -= (32 - (PLOT_MAX_X >> 1)); // 32 is the midpoint
            if(x < 0) x = 0;
            if(x >= PLOT_MAX_X) x = PLOT_MAX_X - 1;
        }
        if(x >= PLOT_MAX_X || x < 0){
            debugf("Invalid x %lu, yaxis %u plot %ld\n", x, yaxis, p);
            assert(false);
        }
        ++plots[p].data[x];
    }
}

static bool check_record_bit_error(
    uint32_t golden, uint32_t actual, bool is_data
){
    if(!is_data && param_state[P_TMODE].real == TRIGGER_MODE_DCACHE_WRITE){
        // For address errors when writing, the inverse hash of actual is the
        // address it intended to write to, and golden == a is the incorrect
        // address which was written to. So swap them, which effectively swaps
        // whether bits are set or cleared.
        uint32_t temp = golden; golden = actual; actual = temp;
    }
    uint32_t corruption = golden ^ actual;
    uint32_t pop = popcnt(corruption);
    if(pop > 6) return false; // Not this kind of corruption
    for(dstate.bit = 0; dstate.bit < 32; ++dstate.bit){
        if((corruption & 1)){
            record_event(((actual & 1) ? YAXIS_BIT_SET_ADDR : YAXIS_BIT_CLEAR_ADDR)
                + (int32_t)is_data);
        }
        corruption >>= 1;
        actual >>= 1;
    }
    return true;
}

static bool check_area(uint32_t* start, uint32_t* end){
    dstate.start = start;
    dstate.end = end;
    
    bool errored = false;
    for(uint32_t* a = start; a < end; ){
        for(int32_t dword=0; dword<4; ++dword){ // Iterate over cachelines
            // Identify the type of corruption
            uint32_t golden = __integer_hash((uint32_t)a);
            uint32_t actual = *a;
            if(golden != actual){
                errored = true;
                dstate.a = a;
                dstate.dword = dword;
                record_event(YAXIS_WORD_ANY);
                if(actual == 0){
                    record_event(YAXIS_WORD_ZERO);
                }else if(check_record_bit_error(golden, actual, true)){
                    // Recorded in check_record_bit_error function
                }else if(check_record_bit_error((uint32_t)a, integer_hash_inverse(actual), false)){
                    // Recorded in check_record_bit_error function
                }else{
                    record_event(YAXIS_WORD_UNKNOWN);
                }
                *a = golden; // fix for next iter
            }
            ++a;
        }
    }
    return errored;
}

void detect_per_test(uint32_t* addr){
    // After each test, only check the test area.
    bool errored = check_area(addr, addr + (DCACHE_SIZE_BYTES >> 2));
    record_event(errored ? YAXIS_TEST_FAILURES : YAXIS_TEST_PASSES);
}

void detect_full_scan() {
    // Bad writes could go anywhere, so check and fix the entire memory area.
    // This only applies to TRIGGER_DCACHE_WRITE.
    bool errored = check_area(most_of_dram, most_of_dram_end);
    if(errored) record_event(YAXIS_TEST_FAILURES);
}
