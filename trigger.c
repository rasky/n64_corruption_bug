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

void trigger_init(){
    // Currently __bss_end is  0x8001e0c0
    uint32_t* mem_area     = (uint32_t*)0x80080000;
    uint32_t* mem_area_end = (uint32_t*)(is_memory_expanded() ? 0x80780000 : 0x80380000);
    // libdragon allocates some stuff at the very end of RAM
    for(uint32_t* a = mem_area; a < mem_area_end; ++a){
        *a = __integer_hash((uint32_t)a);
    }
}

uint32_t* trigger_get_addr(uint32_t offset){
    uint32_t center_addr = is_memory_expanded() ? 0x80680000 : 0x80280000;
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

uint8_t test_device;
uint8_t test_dir;
uint8_t test_tmode;

#define RES_AREA_SIZE 50
typedef struct {
    uint32_t total;
    uint32_t bits[32];
    uint32_t dwords[4];
    uint32_t read_area[RES_AREA_SIZE];
    uint32_t write_area[RES_AREA_SIZE];
} res_clear_t;

static uint32_t res_tests;
static uint32_t res_zeros;
static uint32_t res_aset_total;
static uint32_t res_dset_total;
static uint32_t res_unknown;
static res_clear_t res_aclear;
static res_clear_t res_dclear;
static uint32_t res_device_dir[6];

bool check_record_error(
    uint32_t golden, uint32_t actual,
    uint32_t* set_total_ptr, res_clear_t* clear_ptr,
    uint32_t* start, uint32_t* end, uint32_t* a, uint32_t dword,
    uint8_t mode
){
    uint32_t corruption = golden ^ actual;
    uint32_t pop = __popcnt(corruption);
    if(pop > 6) return false; // Not this kind of corruption
    uint32_t set = corruption & actual;
    uint32_t clear = corruption & golden;
    if(set == 0){
        // Bits cleared, usual case
        clear_ptr->total += pop;
        for(int32_t i=0; i<32; ++i){
            if(corruption & 1) clear_ptr->bits[i]++;
            corruption >>= 1;
        }
        clear_ptr->dwords[dword & 3] += pop;
        uint32_t* area = (mode == TRIGGER_DCACHE_READ)
            ? clear_ptr->read_area : clear_ptr->write_area;
        uint32_t tui_offset = ((a - start) * RES_AREA_SIZE) / (end - start);
        if(tui_offset >= RES_AREA_SIZE) tui_offset = RES_AREA_SIZE - 1;
        area[tui_offset] += pop;
    }else if(clear == 0){
        // Bits set
        *set_total_ptr += pop;
    }else{
        // Some combination
        res_unknown += pop;
    }
    return true;
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
    
#ifndef REPORT_EACH
    ++res_tests;
#endif
    for(uint32_t* a = start; a < end; ){
#ifdef REPORT_EACH
        int32_t first_wrong = -1;
        int32_t last_wrong = -1;
        uint32_t symptoms = 0;
        uint32_t corrupt_mask = 0;
        uint32_t corrupt_bit_set = 0;
        uint32_t corrupt_bit_clear = 0;
#endif
        for(int32_t dword=0; dword<4; ++dword){ // Iterate over cachelines
            // Identify the type of corruption
            uint32_t golden = __integer_hash((uint32_t)a);
            uint32_t actual = *a;
            if(golden != actual){
#ifdef REPORT_EACH
                if(first_wrong < 0){
                    first_wrong = dword;
                }
                last_wrong = dword;
                if(actual == 0){
                    symptoms |= SYMPTOM_ZERO;
                }else{
                    uint32_t corruption = golden ^ actual;
                    if(__popcnt(corruption) <= 6){
                        symptoms |= SYMPTOM_DATA;
                        corrupt_mask |= corruption;
                        corrupt_bit_set |= corruption & actual;
                        corrupt_bit_clear |= corruption & golden;
                    }else{
                        uint32_t actual_inverse = integer_hash_inverse(actual);
                        corruption = (uint32_t)a ^ actual_inverse;
                        if(__popcnt(corruption) <= 6){
                            symptoms |= SYMPTOM_ADDR;
                            corrupt_mask |= corruption;
                            corrupt_bit_set |= corruption & actual_inverse;
                            corrupt_bit_clear |= corruption & (uint32_t)a;
                        }else{
                            symptoms |= SYMPTOM_OTHER;
                            debugf("%08lX: exp %08lX got %08lX inv %08lX\n",
                                (uint32_t)a, golden, actual, actual_inverse);
                        }
                    }
                }
#else
                if(actual == 0){
                    ++res_zeros;
                }else if(check_record_error(golden, actual,
                    &res_dset_total, &res_dclear,
                    start, end, a, dword, mode)){
                    // Recorded in check_record_error function
                }else if(check_record_error((uint32_t)a, integer_hash_inverse(actual),
                    &res_aset_total, &res_aclear,
                    start, end, a, dword, mode)){
                    // Recorded in check_record_error function
                }else{
                    ++res_unknown;
                }
                uint32_t device_dir = (test_device << 1) | test_dir;
                if(device_dir >= 6) device_dir = 5;
                ++res_device_dir[device_dir];
#endif
                *a = golden; // fix for next iter
            }
            ++a;
        }
#ifdef REPORT_EACH
        // Report corruption
        if(symptoms != 0){
            debugf("%08lX (%08lX in) dw %ld-%ld ",
                (uint32_t)a, (uint32_t)a - (uint32_t)addr, first_wrong, last_wrong);
            if(symptoms == SYMPTOM_ZERO){
                debugf("all zeros\n");
            }else if(symptoms == SYMPTOM_ADDR || symptoms == SYMPTOM_DATA){
                debugf("%s ", symptoms == SYMPTOM_ADDR ? "ADDR" : "DATA");
                if(corrupt_bit_set == 0){
                    debugf("cleared %08lX\n", corrupt_bit_clear);
                }else if(corrupt_bit_clear == 0){
                    debugf("set %08lX\n", corrupt_bit_set);
                }else{
                    debugf("mask %08lX set %08lX cleared %08lX\n",
                        corrupt_mask, corrupt_bit_set, corrupt_bit_clear);
                }
            }else{
                debugf("multiple types or unidentifiable corruption\n");
            }
        }
#endif
    }
}

uint32_t max_reduce(uint32_t* data, uint32_t size){
    uint32_t ret = 0;
    for(uint32_t i=0; i<size; ++i){
        if(data[i] > ret) ret = data[i];
    }
    return ret;
}

void tui_horiz_hm(char* buf, uint32_t value, uint32_t mx){
    int32_t thresh = -1;
    if(mx > 0){
        thresh = value * 32 / mx;
    }
    for(int32_t i=0; i<32; ++i) buf[i] = (i <= thresh) ? '=' : ' ';
}

#define TUI_VERT_HM_HEIGHT 8

void tui_vert_hm(char* buf, uint32_t* data, uint32_t mx, int32_t pixel){
    for(int32_t i=0; i<32; ++i){
        int32_t thresh = -1;
        if(mx > 0 && data[i] > 0){
            thresh = data[i] * TUI_VERT_HM_HEIGHT / mx;
        }
        buf[i] = ((TUI_VERT_HM_HEIGHT - 1 - pixel) <= thresh) ? '|' : ' ';
    }
}

void trigger_tui_render() {
    debugf(
        //"\033[2J\033[H"
        "\033[H"
        "N64 Corruption Bug TUI by Sauraen\n"
        "Based on previous work by korgeaux, Rasky, HailToDodongo\n\n"
        "%8ld tests | %8ld zero dwords | %8ld unknown corruptions\n\n"
        "     ADDRESS BIT CLEARS   (%4ld address bit sets)   |"
        "        DATA BIT CLEARS   (%4ld data bit sets)\n"
        "                                                    |\n",
        res_tests, res_zeros, res_unknown,
        res_aset_total, res_dset_total);
    char abuf[33];
    char dbuf[33];
    abuf[32] = '\0';
    dbuf[32] = '\0';
    uint32_t amax, dmax;
    
    amax = max_reduce(res_aclear.bits, 32);
    dmax = max_reduce(res_dclear.bits, 32);
    debugf(
        "Heatmap over bits:                                  |"
        " Heatmap over bits:\n");
    for(int32_t pixel=0; pixel<TUI_VERT_HM_HEIGHT; ++pixel){
        tui_vert_hm(abuf, res_aclear.bits, amax, pixel);
        tui_vert_hm(dbuf, res_dclear.bits, dmax, pixel);
        if(pixel == 0){
            debugf("%8ld%s            | %8ld%s\n", amax, abuf, dmax, dbuf);
        }else{
            debugf("        %s            |         %s\n", abuf, dbuf);
        }
    }
    debugf(
        "        33222222222211111111110000000000            |"
        "         33222222222211111111110000000000\n"
        "        10987654321098765432109876543210            |"
        "         10987654321098765432109876543210\n"
        "                                                    |\n"
        "Heatmap over dwords of cacheline:                   |"
        " Heatmap over dwords of cacheline:\n");
    amax = max_reduce(res_aclear.dwords, 4);
    dmax = max_reduce(res_dclear.dwords, 4);
    for(int32_t dword=0; dword<4; ++dword){
        tui_horiz_hm(abuf, res_aclear.dwords[dword], amax);
        tui_horiz_hm(dbuf, res_dclear.dwords[dword], dmax);
        debugf("      %ld %s %8ld   |       %ld  %s %8ld\n",
            dword, abuf, res_aclear.dwords[dword],
            dword, dbuf, res_dclear.dwords[dword]);
    }
}
