#include <libdragon.h>
#include "tui.h"

#include "detect.h"
#include "controller.h"
#include "test.h"
#include "trigger.h"

static plot_info_t plot_presets[PLOT_PRESET_COUNT][PLOT_COUNT] = {
    {
        {YAXIS_TEST_FAILURES,  P_DEVICE,           32, 3},
        {YAXIS_TEST_FAILURES,  P_DIR,              32, 2},
        {YAXIS_TEST_FAILURES,  P_TMODE,            32, 2},
        {YAXIS_BIT_CLEAR_ADDR, P_TMODE,            32, 2},
        {YAXIS_BIT_CLEAR_DATA, P_TMODE,            32, 2},
        {YAXIS_BIT_SET_ADDR,   P_TMODE,            32, 2},
        {YAXIS_BIT_SET_DATA,   P_TMODE,            32, 2},
        {YAXIS_WORD_ZERO,      P_TMODE,            32, 2},
        {YAXIS_WORD_UNKNOWN,   P_TMODE,            32, 2},
        {YAXIS_BIT_CLEAR_ADDR, P_DIR,              32, 2},
        {YAXIS_BIT_CLEAR_DATA, P_DIR,              32, 2},
        {YAXIS_BIT_SET_ADDR,   P_DIR,              32, 2},
        {YAXIS_BIT_SET_DATA,   P_DIR,              32, 2},
        {YAXIS_WORD_ZERO,      P_DIR,              32, 2},
        {YAXIS_WORD_UNKNOWN,   P_DIR,              32, 2},
    }, {
        {YAXIS_BIT_CLEAR_ADDR, XAXIS_BIT_IDX,      6, 32},
        {YAXIS_BIT_CLEAR_DATA, XAXIS_BIT_IDX,      6, 32},
        {YAXIS_BIT_CLEAR_ADDR, XAXIS_WORD_IDX,     32, 4},
        {YAXIS_BIT_CLEAR_DATA, XAXIS_WORD_IDX,     32, 4},
        {YAXIS_BIT_CLEAR_ADDR, XAXIS_BUF_POS,      4, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_BIT_CLEAR_DATA, XAXIS_BUF_POS,      4, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_BIT_SET_ADDR,   XAXIS_BIT_IDX,      6, 32},
        {YAXIS_BIT_SET_DATA,   XAXIS_BIT_IDX,      6, 32},
        {YAXIS_BIT_SET_ADDR,   XAXIS_WORD_IDX,     32, 4},
        {YAXIS_BIT_SET_DATA,   XAXIS_WORD_IDX,     32, 4},
        {YAXIS_BIT_SET_ADDR,   XAXIS_BUF_POS,      4, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_BIT_SET_DATA,   XAXIS_BUF_POS,      4, 50, PLOT_FLAG_NOLABELS},
    }, {
        {YAXIS_TEST_FAILURES,  XAXIS_CC_0_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_0_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_0_TRIGGER, 3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_0_TRIGGER, 3, 50},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_1_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_1_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_1_TRIGGER, 3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_1_TRIGGER, 3, 50},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_2_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_2_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_2_TRIGGER, 3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_2_TRIGGER, 3, 50},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_3_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_3_PRIME,   3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_FAILURES,  XAXIS_CC_3_TRIGGER, 3, 50, PLOT_FLAG_NOLABELS},
        {YAXIS_TEST_PASSES,    XAXIS_CC_3_TRIGGER, 3, 50},
    }, {
        {YAXIS_TEST_PASSES,    P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_TEST_FAILURES,  P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_BIT_CLEAR_ADDR, P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_BIT_CLEAR_DATA, P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_BIT_SET_ADDR,   P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_BIT_SET_DATA,   P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_WORD_ZERO,      P_RCPCC,            7, RCPCC_COUNT},
        {YAXIS_WORD_UNKNOWN,   P_RCPCC,            7, RCPCC_COUNT},
    }, 
};
static const char* preset_descriptions[PLOT_PRESET_COUNT] = {
    "Failure types",
    "Failure locations",
    "Current control",
    "RCP current control",
};

static uint8_t sel_preset = 0;

static void apply_preset() {
    for(int32_t p=0; p<PLOT_COUNT; ++p){
        plots[p].info = plot_presets[sel_preset][p];
    }
}

static uint32_t max_reduce(uint32_t* data, uint32_t size){
    uint32_t ret = 0;
    for(uint32_t i=0; i<size; ++i){
        if(data[i] > ret) ret = data[i];
    }
    return ret;
}

#define PLOTS_AREA_W 108
#define PLOTS_AREA_MID ((PLOTS_AREA_W >> 1) - 1)
#define PLOTS_AREA_H 46
static char plots_area[PLOTS_AREA_H*PLOTS_AREA_W];

static void tui_plots_clear(){
    char* p = plots_area;
    for(int32_t y=0; y<PLOTS_AREA_H; ++y){
        int32_t x;
        for(x=0; x<PLOTS_AREA_MID; ++x){
            *p++ = ' ';
        }
        *p++ = '|';
        for(++x; x<PLOTS_AREA_W-1; ++x){
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

static const char* get_xaxis_label(uint8_t xaxis){
    return xaxis < XAXIS_MIN ? param_info[xaxis].label : xaxis_labels[xaxis - XAXIS_MIN];
}

static void tui_render_plots(){
    tui_plots_clear();
    int32_t x = 0, y = 0;
    for(int32_t p=0; p<PLOT_COUNT; ++p){
        uint8_t yaxis = plots[p].info.yaxis, xaxis = plots[p].info.xaxis;
        uint32_t xcount = plots[p].info.xcount, ycount = plots[p].info.ytiles;
        uint8_t flags = plots[p].info.flags;
        if(yaxis == YAXIS_OFF) continue;
        
        bool horiz = xcount <= 8;
        int32_t r = horiz ? xcount + 1 : (flags & PLOT_FLAG_NOLABELS) ? ycount + 1 : ycount + 3;
        if(y + r > PLOTS_AREA_H){
            y = 0;
            if(x == 0){
                x = PLOTS_AREA_MID + 3;
            }else{
                break;
            }
        }
        
        int32_t tmpx = x, tmpy = y;
        uint32_t mx = max_reduce(plots[p].data, xcount);
        PLOTPRINT(tmpx, tmpy, "%s over %s", yaxis_labels[yaxis], get_xaxis_label(xaxis));
        ++tmpy;
        
        if(!horiz){
            tmpx += (50 - xcount) >> 1;
        }
        for(int32_t datax = 0; datax < xcount; ++datax){
            char label_str[16];
            bool extend_first = false, extend_last = false;
            if(xaxis < XAXIS_MIN){
                if(param_info[xaxis].value_labels != NULL){
                    snprintf(label_str, 16, "%s", param_info[xaxis].value_labels[datax]);
                }else{
                    snprintf(label_str, 16, "%2ld", param_info[xaxis].conversion(datax));
                }
            }else{
                int32_t label = datax;
                if(xaxis == XAXIS_BIT_IDX){
                    label = 31 - datax;
                }else if(xaxis == XAXIS_BUF_POS){
                    extend_last = true;
                }else if(xaxis >= XAXIS_CC_0_PRIME && xaxis <= XAXIS_CC_3_TRIGGER){
                    label = datax + (32 - (PLOT_MAX_X >> 1));
                    extend_first = extend_last = true;
                }
                snprintf(label_str, 16, "%2ld", label);
            }
            if(horiz){
                PLOTPRINT(tmpx, tmpy + datax, "%s", label_str);
                PLOTPRINT(tmpx + 44, tmpy + datax, "%6lu", plots[p].data[datax]);
            }else if(!(flags & PLOT_FLAG_NOLABELS)){
                if(extend_first && datax == 0){
                    label_str[0] = ' '; label_str[1] = '<';
                }else if(extend_last && datax == xcount - 1){
                    label_str[0] = ' '; label_str[1] = '>';
                }
                plots_area[(tmpy+ycount  )*PLOTS_AREA_W + (tmpx+datax)] = label_str[0];
                plots_area[(tmpy+ycount+1)*PLOTS_AREA_W + (tmpx+datax)] = label_str[1];
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

static void tui_render_top() {
    debugf(
        "\033[H" // Move cursor to upper left
        "N64 Corruption Bug TUI by Sauraen; based on previous work by korgeaux, Rasky, HailToDodongo\n\n");
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
    if((buttons_press & BTN_DDOWN) && cursor_p < (P_COUNT + 1 + PLOT_COUNT) - 1){
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
    {
        bool sel_p = cursor_p == P_COUNT;
        bool can_left = sel_p && sel_preset > 0;
        bool can_right = sel_p && sel_preset < PLOT_PRESET_COUNT - 1;
        debugf("\n%c Plot presets: %c %s %c  %s                      \n",
            sel_p ? '>' : ' ',
            can_left ? '<' : ' ',
            preset_descriptions[sel_preset],
            can_right ? '>' : ' ',
            sel_p ? "(Press A to apply)" : "");
        if(can_right && press_right) ++sel_preset;
        if(can_left && press_left) --sel_preset;
        if(sel_p && (buttons_press & BTN_A)) apply_preset();
    }
    if(cursor_p >= P_COUNT + 1){
        debugf("(Edit with C^ / Cv)\n");
    }else{
        debugf("                   \n");
    }
    for(int32_t p=0; p<PLOT_COUNT; ++p){
        bool sel_p = (cursor_p - (P_COUNT + 1)) == p;
        debugf("%c %c%23s%c",
            sel_p ? '>' : ' ',
            sel_p && cursor_v == 0 ? '>' : ' ',
            yaxis_labels[plots[p].info.yaxis],
            sel_p && cursor_v == 0 ? '<' : ' ');
        if(plots[p].info.yaxis == YAXIS_OFF){
            debugf("                                                              \n");
        }else{
            if(sel_p && press_right && cursor_v < 3) ++cursor_v;
            if(sel_p && press_left && cursor_v > 0) --cursor_v;
            debugf(" over %c%-31s%c, size %c%2u%c, %c%s%c        \n",
                sel_p && cursor_v == 1 ? '>' : ' ',
                get_xaxis_label(plots[p].info.xaxis),
                sel_p && cursor_v == 1 ? '<' : ' ',
                sel_p && cursor_v == 2 ? '>' : ' ',
                plots[p].info.ytiles,
                sel_p && cursor_v == 2 ? '<' : ' ',
                sel_p && cursor_v == 3 ? '>' : ' ',
                (plots[p].info.flags & PLOT_FLAG_NOLABELS) ? "no labels" : "--",
                sel_p && cursor_v == 3 ? '<' : ' ');
        }
    }
}

void tui_init() {
    apply_preset();
}

void tui_render() {
    static uint16_t last_buttons = 0;
    uint16_t buttons = poll_controller();
    uint16_t buttons_press = (buttons ^ last_buttons) & buttons;
    
    if((buttons_press & BTN_START)){
        test_running = !test_running;
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
        tui_render_top();
        if(test_all_disabled){
            debugf("There are no tests to run, press START to fix your test parameters\n");
        }else{
            tui_render_plots();
        }
    }else{
        tui_render_top();
        tui_render_setup(buttons, buttons_press);
    }
    
    last_buttons = buttons;
}
