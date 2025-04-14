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
    uint32_t dwords[4];
    uint32_t read_area[RES_AREA_SIZE];
    uint32_t write_area[RES_AREA_SIZE];
} res_info_t;

typedef struct {
    res_info_t info;
    uint32_t bits[32];
} res_clear_t;

static uint32_t res_tests;
static uint32_t res_failed;
static uint32_t res_aset_total;
static uint32_t res_dset_total;
static uint32_t res_unknown;
static res_info_t res_zeros;
static res_clear_t res_aclear;
static res_clear_t res_dclear;
static uint32_t res_device_dir[6];
static uint32_t res_mode[2];

void record_error_info(res_info_t* ptr, uint32_t errors,
    uint32_t* start, uint32_t* end, uint32_t* a, uint32_t dword,
    uint8_t mode
){
    ptr->total += errors;
    ptr->dwords[dword & 3] += errors;
    uint32_t* area = (mode == TRIGGER_DCACHE_READ)
        ? ptr->read_area : ptr->write_area;
    uint32_t tui_offset = ((a - start) * RES_AREA_SIZE) / (end - start);
    if(tui_offset >= RES_AREA_SIZE) tui_offset = RES_AREA_SIZE - 1;
    area[tui_offset] += errors;
}

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
        for(int32_t i=0; i<32; ++i){
            if(corruption & 1) clear_ptr->bits[i]++;
            corruption >>= 1;
        }
        record_error_info(&clear_ptr->info, pop, start, end, a, dword, mode);
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
    
    bool errored = false;
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
                errored = true;
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
                    record_error_info(&res_zeros, 1, start, end, a, dword, mode);
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
                ++res_mode[mode & 1];
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
#ifndef REPORT_EACH
    ++res_tests;
    if(errored) ++res_failed;
#endif
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
    buf[32] = '\0';
}

void tui_vert_hm(char* buf, uint32_t* data, uint32_t mx,
    int32_t row, uint32_t height, uint32_t width
){
    for(int32_t i=0; i<width; ++i){
        int32_t thresh = -1;
        if(mx > 0 && data[i] > 0){
            thresh = data[i] * height / mx;
        }
        buf[i] = ((int32_t)(height - 1 - row) <= thresh) ? '|' : ' ';
    }
    buf[width] = '\0';
}

void tui_dual_heatmap(
    uint32_t* adata, uint32_t* ddata,
    uint32_t height, uint32_t width,
    const char* description, bool horizontal
) {
    char abuf[51];
    char dbuf[51];
    uint32_t ndata = horizontal ? height : width;
    uint32_t amax = max_reduce(adata, ndata);
    uint32_t dmax = max_reduce(ddata, ndata);
    debugf(
        "                                                     |\n"
        "%-52s | %-52s\n", description, description);
    for(int32_t row=0; row<height; ++row){
        if(horizontal){
            tui_horiz_hm(abuf, adata[row], amax);
            tui_horiz_hm(dbuf, ddata[row], dmax);
            debugf("      %ld %s %8ld    |       %ld  %s %8ld\n",
                row, abuf, adata[row],
                row, dbuf, ddata[row]);
        }else{
            tui_vert_hm(abuf, adata, amax, row, height, width);
            tui_vert_hm(dbuf, ddata, dmax, row, height, width);
            if(row == 0 && width == 32){
                debugf("  %8ld%s           |   %8ld%s\n", amax, abuf, dmax, dbuf);
            }else{
                int padcols = (52 - width) >> 1;
                debugf("%*s%s%*s | %*s%s\n",
                    padcols, "", abuf, padcols, "", padcols, "", dbuf);
            }
        }
    }
}

void trigger_tui_render() {
    debugf(
        //"\033[2J\033[H"
        "\033[H"
        "N64 Corruption Bug TUI by Sauraen; based on previous work by korgeaux, Rasky, HailToDodongo\n\n"
        "%8ld tests  %8ld corrupted, %8ld not  %8ld unknown corruptions\n\n"
        "     ADDRESS BIT CLEARS   (%4ld address bit sets)    |"
        "        DATA BIT CLEARS   (%4ld data bit sets)\n",
        res_tests, res_failed, res_tests - res_failed, res_unknown,
        res_aset_total, res_dset_total);
    
    tui_dual_heatmap(res_aclear.bits, res_dclear.bits, 8, 32,
        "Heatmap over bits:", false);
    debugf(
        "          33222222222211111111110000000000           |"
        "           33222222222211111111110000000000\n"
        "          10987654321098765432109876543210           |"
        "           10987654321098765432109876543210\n");
    tui_dual_heatmap(res_aclear.info.dwords, res_dclear.info.dwords, 4, 32,
        "Heatmap over dwords of cacheline:", true);
    tui_dual_heatmap(res_aclear.info.read_area, res_dclear.info.read_area, 4, 50,
        "Heatmap over buffer for reads:", false);
    /*
    tui_dual_heatmap(res_aclear.info.write_area, res_dclear.info.write_area, 4, 50,
        "Heatmap over most of RDRAM for writes:", false);
    */
    debugf("\n\n"
        "            ZERO WORDS (%8ld total)              |"
        "                  BY PRIME METHOD\n"
        "                                                     |\n"
        "Heatmap over dwords of cacheline:                    |"
        "                  +------------+------------+\n",
        res_zeros.total);
    {
        char buf[51];
        uint32_t mx = max_reduce(res_zeros.dwords, 4);
        tui_horiz_hm(buf, res_zeros.dwords[0], mx);
        debugf("      0 %s %8ld    |                  | RDRAM->RCP | RCP->RDRAM |\n",
            buf, res_zeros.dwords[0]);
        tui_horiz_hm(buf, res_zeros.dwords[1], mx);
        debugf("      1 %s %8ld    |     +------------+------------+------------+\n",
            buf, res_zeros.dwords[1]);
        tui_horiz_hm(buf, res_zeros.dwords[2], mx);
        debugf("      2 %s %8ld    |     | RSP DMEM   | %10ld | %10ld |\n",
            buf, res_zeros.dwords[2], res_device_dir[0], res_device_dir[1]);
        tui_horiz_hm(buf, res_zeros.dwords[3], mx);
        debugf("      3 %s %8ld    |     | RSP IMEM   | %10ld | %10ld |\n"
            "                                                     |"
            "     | Cart \"ROM\" | %10ld | %10ld |\n"
            "Heatmap over buffer for reads:                       |"
            "     +------------+------------+------------+\n",
            buf, res_zeros.dwords[3], res_device_dir[2], res_device_dir[3],
            res_device_dir[4], res_device_dir[5]);
        mx = max_reduce(res_zeros.read_area, 4);
        tui_vert_hm(buf, res_zeros.read_area, mx, 0, 4, 50);
        debugf(" %s  |\n", buf);
        tui_vert_hm(buf, res_zeros.read_area, mx, 1, 4, 50);
        debugf(" %s  |                  BY TRIGGER MODE\n", buf);
        tui_vert_hm(buf, res_zeros.read_area, mx, 2, 4, 50);
        debugf(" %s  |               DCACHE read  %8ld\n", buf, res_mode[0]);
        tui_vert_hm(buf, res_zeros.read_area, mx, 3, 4, 50);
        debugf(" %s  |               DCACHE write %8ld\n", buf, res_mode[1]);
    }
}
