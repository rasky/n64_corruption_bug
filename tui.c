#include <libdragon.h>
#include "tui.h"

#include "detect.h"
#include "controller.h"
#include "test.h"
#include "trigger.h"

static uint32_t max_reduce(uint32_t* data, uint32_t size){
    uint32_t ret = 0;
    for(uint32_t i=0; i<size; ++i){
        if(data[i] > ret) ret = data[i];
    }
    return ret;
}

#define PLOTS_AREA_W 106
#define PLOTS_AREA_H 46
static char plots_area[PLOTS_AREA_H*PLOTS_AREA_W];

static void tui_plots_clear(){
    char* p = plots_area;
    for(int32_t y=0; y<PLOTS_AREA_H; ++y){
        for(int32_t x=0; x<PLOTS_AREA_W-1; ++x){
            *p++ = ' ';
        }
        *p++ = '\n';
    }
}

#define PLOTPRINT(_x, _y, _format, ...) { \
    int32_t _chars = snprintf(&plots_area[(_y)*PLOTS_AREA_W+(_x)], PLOTS_AREA_W-(_x), \
        _format, __VA_ARGS__); \
    assert(_chars > 0); \
    plots_area[(_y)*PLOTS_AREA_W+(_x)+_chars] = ((_x) + _chars) == PLOTS_AREA_W ? '\n' : ' '; \
}

static void tui_render_plots(){
    tui_plots_clear();
    int32_t x = 0, y = 0;
    for(int32_t p=0; p<PLOT_COUNT; ++p){
        uint8_t yaxis = plots[p].yaxis, xaxis = plots[p].xaxis;
        uint32_t xcount = plots[p].xcount, ycount = plots[p].ytiles;
        if(yaxis == YAXIS_OFF) continue;
        
        bool horiz = xcount <= 8;
        int32_t r = horiz ? xcount + 1 : ycount + 3;
        if(y + r > PLOTS_AREA_H){
            y = 0;
            if(x == 0){
                x = 52;
            }else{
                break;
            }
        }
        
        int32_t tmpx = x, tmpy = y;
        uint32_t mx = max_reduce(plots[p].data, xcount);
        PLOTPRINT(tmpx, tmpy, "%s over %s", yaxis_labels[yaxis],
            xaxis < XAXIS_MIN ? param_info[xaxis].label : xaxis_labels[xaxis - XAXIS_MIN]);
        ++tmpy;
        
        if(!horiz){
            tmpx += (50 - xcount) >> 1;
        }
        for(int32_t datax = 0; datax < xcount; ++datax){
            if(horiz){
                if(xaxis < XAXIS_MIN){
                    if(param_info[xaxis].value_labels != NULL){
                        PLOTPRINT(tmpx, tmpy + datax, "%s", param_info[xaxis].value_labels[datax]);
                    }else{
                        PLOTPRINT(tmpx, tmpy + datax, "%lu", param_info[xaxis].conversion(datax));
                    }
                }else{
                    PLOTPRINT(tmpx, tmpy + datax, "%ld", datax);
                }
                PLOTPRINT(tmpx + 44, tmpy + datax, "%6lu", plots[p].data[datax]);
            }else{
                int32_t label = datax;
                char special_char = '\0';
                if(xaxis == XAXIS_BIT_IDX){
                    label = 31 - datax;
                }else if(xaxis == XAXIS_BUF_POS){
                    if(datax == xcount - 1) special_char = '>';
                }else if(xaxis >= XAXIS_CC_0_PRIME && xaxis <= XAXIS_CC_3_TRIGGER){
                    label = datax + (32 - (PLOT_MAX_X >> 1));
                    if(datax == 0) special_char = '<';
                    if(datax == xcount - 1) special_char = '>';
                }
                assert(label >= 0 && label <= 99);
                int32_t tens = label / 10;
                int32_t ones = label - tens * 10;
                if(special_char == '\0' && tens != 0){
                    plots_area[(tmpy+ycount  )*PLOTS_AREA_W + (tmpx+datax)] = tens + '0';
                }
                plots_area[(tmpy+ycount+1)*PLOTS_AREA_W + (tmpx+datax)] = 
                    special_char != '\0' ? special_char : (ones + '0');
            }
        }
        if(horiz){
            tmpx += 10;
        }
        
        int32_t subchar = horiz ? 1 : 3;
        for(int32_t datax = 0; datax < xcount; ++datax){
            uint32_t value = plots[p].data[datax];
            int32_t thresh = -1;
            if(mx > 0 && value > 0){
                thresh = (value * (subchar * ycount)) / mx;
            }
            for(int32_t datay = 0; datay < ycount; ++datay){
                if(horiz){
                    plots_area[(tmpy+datax)*PLOTS_AREA_W + (tmpx+datay)] =
                        (datay <= thresh) ? '=' : ' ';
                }else{
                    int32_t z = thresh - (int32_t)(ycount - 1 - datay) * subchar + 1;
                    if(z > subchar) z = subchar;
                    if(z < 0) z = 0;
                    plots_area[(tmpy+datay)*PLOTS_AREA_W + (tmpx+datax)] = " ,;|"[z];
                }
            }
        }
        if(!horiz){
            PLOTPRINT(x, tmpy, "%lu", mx);
        }
        
        y += r + 1;
    }
    plots_area[PLOTS_AREA_H*PLOTS_AREA_W - 1] = '\0'; // Replace last newline with null term
    debugf("%s", plots_area);
}

/*
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
*/

static void tui_render_top() {
    debugf(
        "\033[H" // Move cursor to upper left
        "N64 Corruption Bug TUI by Sauraen; based on previous work by korgeaux, Rasky, HailToDodongo\n\n"
        /*
        "%8ld tests  %8ld corrupted, %8ld not  %8ld unknown corruptions\n",
        res.tests, res.failed, res.tests - res.failed, res.unknown*/);
}

static void tui_render_setup(uint16_t buttons, uint16_t buttons_press) {
    debugf("\nPress START to start/stop test\n"
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
        debugf("%c %s: ", sel_p ? '>' : ' ', param_info[p].label);
        if(param_info[p].value_labels != NULL){
            if(sel_p && press_right && cursor_v < param_info[p].max - 1) ++cursor_v;
            if(sel_p && press_left && cursor_v > 0) --cursor_v;
            for(int32_t v=0; v<param_info[p].max; ++v){
                bool sel_v = cursor_v == v;
                if(sel_p && sel_v && (buttons_press & BTN_A)){
                    param_state[p].selected ^= 1 << v;
                }
                bool on = (param_state[p].selected & (1 << v));
                debugf("%c%c%s%c%c  ",
                    sel_p && sel_v ? '>' : ' ',
                    on ? '[' : ' ',
                    param_info[p].value_labels[v],
                    on ? ']' : ' ',
                    sel_p && sel_v ? '<' : ' ');
            }
            debugf("\n");
        }else{
            bool can_edit_left = param_state[p].selected > 1;
            bool can_edit_right = param_state[p].selected < param_info[p].max;
            debugf("%lu to %lu (stop at %c %lu %c)           \n",
                param_info[p].conversion(0),
                param_info[p].conversion(param_state[p].selected - 1),
                sel_p && can_edit_left ? '<' : ' ',
                param_info[p].conversion(param_state[p].selected),
                sel_p && can_edit_right ? '>' : ' ');
            if(sel_p && press_left && can_edit_left){
                param_state[p].selected = (param_info[p].flags & TEST_FLAG_EDIT_POWER2) ?
                    param_state[p].selected >> 1 : param_state[p].selected - 1;
            }
            if(sel_p && press_right && can_edit_right){
                param_state[p].selected = (param_info[p].flags & TEST_FLAG_EDIT_POWER2) ?
                    param_state[p].selected << 1 : param_state[p].selected + 1;
            }
        }
    }
}

/*
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
    //tui_dual_heatmap(res.aclear.info.write_area, res.dclear.info.write_area, 4, 50,
    //    "Heatmap over most of RDRAM for writes:", NULL, false);
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

#define TUI_SCREEN_HEATMAPS 0
#define TUI_SCREEN_CC 1
static uint8_t tui_screen = TUI_SCREEN_HEATMAPS;
*/

void tui_render() {
    static uint16_t last_buttons = 0;
    uint16_t buttons = poll_controller();
    uint16_t buttons_press = (buttons ^ last_buttons) & buttons;
    //uint8_t last_screen = tui_screen;
    
    if((buttons_press & BTN_START)){
        test_running = !test_running;
        //last_screen = -1;
        if(test_running){
            debugf("\nFilling RDRAM...\n");
            for(int32_t p=0; p<PLOT_COUNT; ++p){
                memset(plots[p].data, 0, PLOT_MAX_X * sizeof(uint32_t));
            }
            trigger_init();
            test_reset();
        }else{
            debugf("\033[2J"); // Clear screen
        }
    }
    
    if(test_running){
        /*
        if((buttons_press & BTN_R) && tui_screen < TUI_SCREEN_CC){
            ++tui_screen;
        }else if((buttons_press & BTN_L) && tui_screen > TUI_SCREEN_HEATMAPS){
            --tui_screen;
        }
        if(last_screen != tui_screen){
            debugf("\033[2J"); // Clear screen
        }
        */
        tui_render_top();
        if(test_all_disabled){
            debugf("There are no tests to run, press START to fix your test parameters\n");
        }else/* if(tui_screen == TUI_SCREEN_HEATMAPS){
            tui_render_heatmaps();
        }else if(tui_screen == TUI_SCREEN_CC){
            tui_render_cc();
        }*/{
            tui_render_plots();
        }
    }else{
        tui_render_top();
        tui_render_setup(buttons, buttons_press);
    }
    
    last_buttons = buttons;
}
