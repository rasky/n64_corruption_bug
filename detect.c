#include <libdragon.h>
#include "detect.h"

#include "hashes.h"
#include "test.h"
#include "trigger.h"

res_t res;

uint32_t cc_after_prime;
uint32_t cc_after_trigger;

static uint32_t popcnt(uint32_t x){
    uint32_t ret = 0;
    for(int32_t i=0; i<32; ++i){
        if(x & 1) ++ret;
        x >>= 1;
    }
    return ret;
}

static void record_error_info(res_info_t* ptr,
    uint32_t errors, uint32_t dword, int32_t area_offset
){
    ptr->total += errors;
    ptr->dwords[dword & 3] += errors;
    if(area_offset >= 0){
        ptr->read_area[area_offset] += errors;
    }
}

static bool check_record_error(
    uint32_t golden, uint32_t actual,
    uint32_t* set_total_ptr, res_clear_t* clear_ptr,
    uint32_t dword, int32_t area_offset, bool swap
){
    if(swap){
        // For address errors when writing, the inverse hash of actual is the
        // address it intended to write to, and golden == a is the incorrect
        // address which was written to. So swap them, which effectively swaps
        // whether bits are set or cleared.
        uint32_t temp = golden; golden = actual; actual = temp;
    }
    uint32_t corruption = golden ^ actual;
    uint32_t pop = popcnt(corruption);
    if(pop > 6) return false; // Not this kind of corruption
    uint32_t set = corruption & actual;
    uint32_t clear = corruption & golden;
    if(set == 0){
        // Bits cleared, usual case
        for(int32_t i=0; i<32; ++i){
            // TUI renders in the order 31-0
            if(corruption & 0x80000000) clear_ptr->bits[i]++;
            corruption <<= 1;
        }
        record_error_info(&clear_ptr->info, pop, dword, area_offset);
    }else if(clear == 0){
        // Bits set
        *set_total_ptr += pop;
    }else{
        // Some combination
        res.unknown += pop;
    }
    return true;
}

static void record_cc_info(res_cc_t* res_cc, uint32_t cc){
    for(int32_t m=0; m<4; ++m){
        int32_t x = (cc >> ((3 - m) << 3)) & 0xFF;
        x -= (32 - (RES_CC_HM_SIZE >> 1)); // 32 is the midpoint
        if(x < 0) x = 0;
        if(x >= RES_CC_HM_SIZE) x = RES_CC_HM_SIZE - 1;
        ++res_cc->modules[m].hm[x];
    }
}

static bool check_area(uint32_t* start, uint32_t* end, uint8_t device, uint8_t dir, uint8_t mode){
    uint32_t device_dir = (device << 1) | dir;
    if(device_dir >= 6) device_dir = 5;
    
    bool errored = false;
    for(uint32_t* a = start; a < end; ){
        for(int32_t dword=0; dword<4; ++dword){ // Iterate over cachelines
            // Identify the type of corruption
            uint32_t golden = __integer_hash((uint32_t)a);
            uint32_t actual = *a;
            if(golden != actual){
                errored = true;
                int32_t za_area_offset = -1, d_area_offset = -1;
                if(mode == TRIGGER_MODE_DCACHE_READ){
                    za_area_offset = (a - start) >> 2; // Number of cachelines in
                    d_area_offset = ((int32_t)(a - start) * RES_AREA_SIZE) / (int32_t)(end - start);
                    if(za_area_offset >= RES_AREA_SIZE) za_area_offset = RES_AREA_SIZE - 1;
                    if( d_area_offset >= RES_AREA_SIZE)  d_area_offset = RES_AREA_SIZE - 1;
                }
                if(actual == 0){
                    record_error_info(&res.zeros, 1, dword, za_area_offset);
                }else if(check_record_error(golden, actual,
                    &res.dset_total, &res.dclear, dword, d_area_offset, false)){
                    // Recorded in check_record_error function
                }else if(check_record_error((uint32_t)a, integer_hash_inverse(actual),
                    &res.aset_total, &res.aclear, dword, za_area_offset,
                    mode == TRIGGER_MODE_DCACHE_WRITE)){
                    // Recorded in check_record_error function
                }else{
                    ++res.unknown;
                }
                ++res.device_dir[device_dir];
                ++res.mode[mode & 1];
                *a = golden; // fix for next iter
            }
            ++a;
        }
    }
    return errored;
}

void detect_per_test(uint8_t device, uint8_t dir, uint8_t mode, uint32_t* addr){
    // After each test, only check the test area.
    uint32_t* start = addr;
    uint32_t* end = addr + (DCACHE_SIZE_BYTES >> 2);
    bool errored = check_area(start, end, device, dir, mode);
    ++res.tests;
    if(errored) ++res.failed;
    record_cc_info(errored ? &res.cc_fail_prime : &res.cc_pass_prime, cc_after_prime);
    record_cc_info(errored ? &res.cc_fail_trigger : &res.cc_pass_trigger, cc_after_trigger);
}

void detect_full_scan(uint8_t device, uint8_t dir) {
    // Bad writes could go anywhere, so check and fix the entire memory area.
    // This only applies to TRIGGER_DCACHE_WRITE.
    check_area(most_of_dram, most_of_dram_end, device, dir, TRIGGER_MODE_DCACHE_WRITE);
}
