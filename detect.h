#ifndef DETECT_H
#define DETECT_H

#include "test.h"

#define YAXIS_OFF            0
#define YAXIS_TEST_FAILURES  1
#define YAXIS_TEST_PASSES    2
#define YAXIS_WORD_ANY       3
#define YAXIS_WORD_ZERO      4
#define YAXIS_WORD_UNKNOWN   5
#define YAXIS_PWORD_ANY      6
#define YAXIS_PWORD_UNWRIT   7
#define YAXIS_PWORD_UNKNOWN  8
#define YAXIS_BIT_CLEAR_ADDR 9 // Leave these four in the same order
#define YAXIS_BIT_CLEAR_DATA 10
#define YAXIS_BIT_SET_ADDR   11
#define YAXIS_BIT_SET_DATA   12
#define YAXIS_COUNT          13

// P_SIZE through P_TMODE are params
#define XAXIS_MIN          P_OFFSETS
#define XAXIS_BIT_IDX      (XAXIS_MIN +  0)
#define XAXIS_WORD_IDX     (XAXIS_MIN +  1)
#define XAXIS_BUF_POS      (XAXIS_MIN +  2)
#define XAXIS_PBUF_POS     (XAXIS_MIN +  3)
#define XAXIS_CC_0_PRIME   (XAXIS_MIN +  4)
#define XAXIS_CC_1_PRIME   (XAXIS_MIN +  5)
#define XAXIS_CC_2_PRIME   (XAXIS_MIN +  6)
#define XAXIS_CC_3_PRIME   (XAXIS_MIN +  7)
#define XAXIS_CC_0_TRIGGER (XAXIS_MIN +  8)
#define XAXIS_CC_1_TRIGGER (XAXIS_MIN +  9)
#define XAXIS_CC_2_TRIGGER (XAXIS_MIN + 10)
#define XAXIS_CC_3_TRIGGER (XAXIS_MIN + 11)
#define XAXIS_COUNT        (XAXIS_MIN + 12)

extern const char* yaxis_labels[YAXIS_COUNT];
extern const char* xaxis_labels[XAXIS_COUNT - XAXIS_MIN];

#define PLOT_COUNT 16
#define PLOT_PRESET_COUNT 5
#define PLOT_MAX_X 50

#define PLOT_FLAG_NOLABELS (1 << 0)

typedef struct {
    uint8_t yaxis;
    uint8_t xaxis;
    uint8_t ytiles;
    uint8_t xcount;
    uint8_t flags;
} plot_info_t;

typedef struct {
    plot_info_t info;
    uint32_t data[PLOT_MAX_X];
} plot_t;

extern plot_t plots[PLOT_COUNT];

typedef struct {
    int32_t cc_after_prime; // Must be first for asm
    int32_t cc_after_trigger; // Must be second for asm
    uint8_t bit;
    uint32_t* start;
    uint32_t* end;
    uint32_t* a;
    int32_t dword;
} detect_state_t;
extern detect_state_t dstate;

/*
#define RES_AREA_SIZE 50
typedef struct {
    uint32_t total;
    uint32_t dwords[4];
    uint32_t read_area[RES_AREA_SIZE];
    //uint32_t write_area[RES_AREA_SIZE];
} res_info_t;

typedef struct {
    res_info_t info;
    uint32_t bits[32];
} res_clear_t;

#define RES_CC_HM_SIZE 24
typedef struct {
    uint32_t hm[RES_CC_HM_SIZE];
} res_cc_hm_t;

typedef struct {
    res_cc_hm_t modules[4];
} res_cc_t;

typedef struct {
    uint32_t tests;
    uint32_t failed;
    uint32_t aset_total;
    uint32_t dset_total;
    uint32_t unknown;
    res_info_t zeros;
    res_clear_t aclear;
    res_clear_t dclear;
    uint32_t device_dir[6];
    uint32_t mode[2];
    res_cc_t cc_fail_prime;
    res_cc_t cc_pass_prime;
    res_cc_t cc_fail_trigger;
    res_cc_t cc_pass_trigger;
} res_t;

extern res_t res;
*/

/** Detect the corruption after each test. Depends on parts of dstate having
been set up already. */
extern void detect_per_test(uint32_t* addr);
/** After all TRIGGER_DCACHE_WRITE tests are done, detect and fix corruptions in
the whole memory area (almost whole RDRAM). */
extern void detect_full_scan();
/** If check_prime is true, call this after detect_per_test, to check whether
the memory supposed to be written by prime is correct. */
extern void prime_check(
    uint8_t device, uint8_t dir, uint32_t size_bytes, uint32_t pattern);

#endif
