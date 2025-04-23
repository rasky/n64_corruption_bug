#include <libdragon.h>
#include "tui.h"

#include "detect.h"

static uint32_t max_reduce(uint32_t* data, uint32_t size){
    uint32_t ret = 0;
    for(uint32_t i=0; i<size; ++i){
        if(data[i] > ret) ret = data[i];
    }
    return ret;
}

static void tui_horiz_hm(char* buf, uint32_t value, uint32_t mx){
    int32_t thresh = -1;
    if(mx > 0){
        thresh = value * 32 / mx;
    }
    for(int32_t i=0; i<32; ++i) buf[i] = (i <= thresh) ? '=' : ' ';
    buf[32] = '\0';
}

static void tui_vert_hm(char* buf, uint32_t* data, uint32_t mx,
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

static void tui_dual_heatmap(
    uint32_t* adata, uint32_t* ddata,
    uint32_t height, uint32_t width,
    const char* description, const char* desc2, bool horizontal
) {
    char abuf[51];
    char dbuf[51];
    uint32_t ndata = horizontal ? height : width;
    uint32_t amax = max_reduce(adata, ndata);
    uint32_t dmax = max_reduce(ddata, ndata);
    debugf(
        "                                                     |\n"
        "%-52s | %-52s\n", description, desc2 ? desc2 : description);
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

static uint32_t tui_counter = 0;
static uint32_t tui_state = 0;

void tui_render() {
    ++tui_counter;
    if(tui_counter >= 40){
        tui_counter = 0;
        ++tui_state;
        if(tui_state >= 2){
            tui_state = 0;
        }
        debugf("\033[2J"); // Clear screen
    }
    debugf(
        "\033[H" // Move cursor to upper left
        "N64 Corruption Bug TUI by Sauraen; based on previous work by korgeaux, Rasky, HailToDodongo\n\n"
        "%8ld tests  %8ld corrupted, %8ld not  %8ld unknown corruptions\n",
        res.tests, res.failed, res.tests - res.failed, res.unknown);
    
    if(tui_state == 1){
        //debugf("Last prime %08lX trigger %08lX\n", cc_after_prime, cc_after_trigger);
        debugf(
            "RDRAM auto current control value after PRIME:\n"
            "                When memory corrupted                |"
            "             When memory NOT corrupted\n");
        tui_dual_heatmap(res.cc_fail_prime.modules[0].hm, res.cc_pass_prime.modules[0].hm,
            3, RES_CC_HM_SIZE, "Module 0 (0-2 MiB):", NULL, false);
        tui_dual_heatmap(res.cc_fail_prime.modules[1].hm, res.cc_pass_prime.modules[1].hm,
            3, RES_CC_HM_SIZE, "Module 1 (2-4 MiB):", NULL, false);
        tui_dual_heatmap(res.cc_fail_prime.modules[2].hm, res.cc_pass_prime.modules[2].hm,
            3, RES_CC_HM_SIZE, "Module 2 (4-6 MiB):", NULL, false);
        tui_dual_heatmap(res.cc_fail_prime.modules[3].hm, res.cc_pass_prime.modules[3].hm,
            3, RES_CC_HM_SIZE, "Module 3 (6-8 MiB):", NULL, false);
        debugf(
            "RDRAM auto current control value after TRIGGER:\n"
            "                When memory corrupted                |"
            "             When memory NOT corrupted\n");
        tui_dual_heatmap(res.cc_fail_trigger.modules[0].hm, res.cc_pass_trigger.modules[0].hm,
            3, RES_CC_HM_SIZE, "Module 0 (0-2 MiB):", NULL, false);
        tui_dual_heatmap(res.cc_fail_trigger.modules[1].hm, res.cc_pass_trigger.modules[1].hm,
            3, RES_CC_HM_SIZE, "Module 1 (2-4 MiB):", NULL, false);
        tui_dual_heatmap(res.cc_fail_trigger.modules[2].hm, res.cc_pass_trigger.modules[2].hm,
            3, RES_CC_HM_SIZE, "Module 2 (4-6 MiB):", NULL, false);
        tui_dual_heatmap(res.cc_fail_trigger.modules[3].hm, res.cc_pass_trigger.modules[3].hm,
            3, RES_CC_HM_SIZE, "Module 3 (6-8 MiB):", NULL, false);
    }else if(tui_state == 0){
        debugf(
            "     ADDRESS BIT CLEARS   (%4ld address bit sets)    |"
            "        DATA BIT CLEARS   (%4ld data bit sets)\n",
            res.aset_total, res.dset_total);
        tui_dual_heatmap(res.aclear.bits, res.dclear.bits, 8, 32,
            "Heatmap over bits:", NULL, false);
        debugf(
            "          33222222222211111111110000000000           |"
            "           33222222222211111111110000000000\n"
            "          10987654321098765432109876543210           |"
            "           10987654321098765432109876543210\n");
        tui_dual_heatmap(res.aclear.info.dwords, res.dclear.info.dwords, 4, 32,
            "Heatmap over dwords of cacheline:", NULL, true);
        tui_dual_heatmap(res.aclear.info.read_area, res.dclear.info.read_area, 4, 50,
            "Heatmap over beginning of buffer for reads:",
            "Heatmap over whole buffer for reads:", false);
        /*
        tui_dual_heatmap(res.aclear.info.write_area, res.dclear.info.write_area, 4, 50,
            "Heatmap over most of RDRAM for writes:", NULL, false);
        */
        debugf("\n\n"
            "            ZERO WORDS (%8ld total)              |"
            "                  BY PRIME METHOD\n"
            "                                                     |\n"
            "Heatmap over dwords of cacheline:                    |"
            "                  +------------+------------+\n",
            res.zeros.total);
        {
            char buf[51];
            uint32_t mx = max_reduce(res.zeros.dwords, 4);
            tui_horiz_hm(buf, res.zeros.dwords[0], mx);
            debugf("      0 %s %8ld    |                  | RDRAM->RCP | RCP->RDRAM |\n",
                buf, res.zeros.dwords[0]);
            tui_horiz_hm(buf, res.zeros.dwords[1], mx);
            debugf("      1 %s %8ld    |     +------------+------------+------------+\n",
                buf, res.zeros.dwords[1]);
            tui_horiz_hm(buf, res.zeros.dwords[2], mx);
            debugf("      2 %s %8ld    |     | RSP DMEM   | %10ld | %10ld |\n",
                buf, res.zeros.dwords[2], res.device_dir[0], res.device_dir[1]);
            tui_horiz_hm(buf, res.zeros.dwords[3], mx);
            debugf("      3 %s %8ld    |     | RSP IMEM   | %10ld | %10ld |\n"
                "                                                     |"
                "     | Cart \"ROM\" | %10ld | %10ld |\n"
                "Heatmap over beginning of buffer for reads:          |"
                "     +------------+------------+------------+\n",
                buf, res.zeros.dwords[3], res.device_dir[2], res.device_dir[3],
                res.device_dir[4], res.device_dir[5]);
            mx = max_reduce(res.zeros.read_area, 50);
            tui_vert_hm(buf, res.zeros.read_area, mx, 0, 4, 50);
            debugf(" %s  |\n", buf);
            tui_vert_hm(buf, res.zeros.read_area, mx, 1, 4, 50);
            debugf(" %s  |                  BY TRIGGER MODE\n", buf);
            tui_vert_hm(buf, res.zeros.read_area, mx, 2, 4, 50);
            debugf(" %s  |               DCACHE read  %8ld\n", buf, res.mode[0]);
            tui_vert_hm(buf, res.zeros.read_area, mx, 3, 4, 50);
            debugf(" %s  |               DCACHE write %8ld\n", buf, res.mode[1]);
        }
    }
}
