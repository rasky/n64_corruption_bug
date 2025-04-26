#include <libdragon.h>
#include "tui.h"

#include "detect.h"
#include "controller.h"
#include "test.h"

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
            thresh = data[i] * (3 * height) / mx;
        }
        thresh -= (int32_t)(height - 1 - row) * 3;
        thresh += 1; // Before this was [-1 or less, 0, 1, 2 or more]
        if(thresh > 3) thresh = 3;
        if(thresh < 0) thresh = 0;
        buf[i] = " ,;|"[thresh];
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

static void tui_render_top() {
    debugf(
        "\033[H" // Move cursor to upper left
        "N64 Corruption Bug TUI by Sauraen; based on previous work by korgeaux, Rasky, HailToDodongo\n\n"
        "%8ld tests  %8ld corrupted, %8ld not  %8ld unknown corruptions\n",
        res.tests, res.failed, res.tests - res.failed, res.unknown);
}

static void tui_render_setup(uint16_t buttons, uint16_t buttons_press) {
    debugf("\nPress START to start/stop test, <C / C> to change pages\n"
        "Note that controller may be intermittently unresponsive during test\n\n");
    
    static uint8_t cursor_p = 0;
    static uint8_t cursor_v = 0;
    if((buttons_press & BTN_DUP) && cursor_p > 0){
        --cursor_p;
        cursor_v = 0;
    }
    if((buttons_press & BTN_DDOWN) && cursor_p < P_COUNT-1){
        ++cursor_p;
        cursor_v = 0;
    }
    bool press_right = buttons_press & BTN_DRIGHT;
    bool press_left = buttons_press & BTN_DLEFT;
    
    for(int32_t p=0; p<P_COUNT; ++p){
        bool sel_p = cursor_p == p;
        debugf("%c %s: ", sel_p ? '>' : ' ', params[p].label);
        if(params[p].value_labels != NULL){
            if(sel_p && press_right && cursor_v < params[p].max - 1) ++cursor_v;
            if(sel_p && press_left && cursor_v > 0) --cursor_v;
            for(int32_t v=0; v<params[p].max; ++v){
                bool sel_v = cursor_v == v;
                if(sel_p && sel_v && (buttons_press & BTN_A)){
                    params[p].selected ^= 1 << v;
                }
                bool on = (params[p].selected & (1 << v));
                debugf("%c%c%s%c%c  ",
                    sel_p && sel_v ? '>' : ' ',
                    on ? '[' : ' ',
                    params[p].value_labels[v],
                    on ? ']' : ' ',
                    sel_p && sel_v ? '<' : ' ');
            }
            debugf("\n");
        }else{
            uint32_t final_value = next_param_value(p, true, false, true);
            uint32_t edit_left =   next_param_value(p, true, true, true);
            uint32_t edit_right =  next_param_value(p, true, true, false);
            bool can_edit_left = edit_left != params[p].min;
            bool can_edit_right = edit_right != params[p].max;
            debugf("%lu to %lu (stop at %c %lu %c)           \n",
                params[p].min,
                final_value,
                sel_p && can_edit_left ? '<' : ' ',
                params[p].selected,
                sel_p && can_edit_right ? '>' : ' ');
            if(sel_p && press_left && can_edit_left) params[p].selected = edit_left;
            if(sel_p && press_right && can_edit_right) params[p].selected = edit_right;
        }
    }
}

static void tui_render_heatmaps() {
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

static void tui_render_cc() {
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
}

#define TUI_SCREEN_SETUP 0
#define TUI_SCREEN_HEATMAPS 1
#define TUI_SCREEN_CC 2

static uint16_t last_buttons = 0;
static uint8_t tui_screen = TUI_SCREEN_SETUP;

void tui_render() {
    uint16_t buttons = poll_controller();
    uint16_t buttons_press = (buttons ^ last_buttons) & buttons;
    
    uint8_t last_screen = tui_screen;
    if((buttons_press & BTN_CRIGHT) && tui_screen < TUI_SCREEN_CC){
        ++tui_screen;
    }else if((buttons_press & BTN_CLEFT) && tui_screen > TUI_SCREEN_SETUP){
        --tui_screen;
    }
    if((buttons_press & BTN_START)){
        test_running = !test_running;
        if(test_running){
            memset(&res, 0, sizeof(res));
            if(tui_screen == TUI_SCREEN_SETUP) tui_screen = TUI_SCREEN_HEATMAPS;
        }
    }
    if(last_screen != tui_screen){
        debugf("\033[2J"); // Clear screen
    }
    
    tui_render_top();
    if(tui_screen == TUI_SCREEN_SETUP){
        tui_render_setup(buttons, buttons_press);
    }else if(tui_screen == TUI_SCREEN_HEATMAPS){
        tui_render_heatmaps();
    }else if(tui_screen == TUI_SCREEN_CC){
        tui_render_cc();
    }
    
    last_buttons = buttons;
}
