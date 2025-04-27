#ifndef DETECT_H
#define DETECT_H

/*
#include "test.h"

#define YAXIS_TEST_FAILURES  0
#define YAXIS_TEST_PASSES    1
#define YAXIS_WORD_ANY       2
#define YAXIS_WORD_UNKNOWN   3
#define YAXIS_BIT_CLEAR_ADDR 4
#define YAXIS_BIT_CLEAR_DATA 5
#define YAXIS_BIT_SET_ADDR   6
#define YAXIS_BIT_SET_DATA   7
#define YAXIS_COUNT          8

// P_SIZE through P_TMODE are params
#define XAXIS_BIT_IDX      (P_OFFSETS +  0)
#define XAXIS_WORD_IDX     (P_OFFSETS +  1)
#define XAXIS_BUF_POS      (P_OFFSETS +  2)
#define XAXIS_CC_0_PRIME   (P_OFFSETS +  3)
#define XAXIS_CC_1_PRIME   (P_OFFSETS +  4)
#define XAXIS_CC_2_PRIME   (P_OFFSETS +  5)
#define XAXIS_CC_3_PRIME   (P_OFFSETS +  6)
#define XAXIS_CC_0_TRIGGER (P_OFFSETS +  7)
#define XAXIS_CC_1_TRIGGER (P_OFFSETS +  8)
#define XAXIS_CC_2_TRIGGER (P_OFFSETS +  9)
#define XAXIS_CC_3_TRIGGER (P_OFFSETS + 10)
#define XAXIS_COUNT        (P_OFFSETS + 11)

extern const char* yaxis_labels[YAXIS_COUNT];
extern const char* xaxis_labels[XAXIS_COUNT - P_COUNT];

typedef struct {
    uint32_t xmin;
    uint32_t xmax;
    uint8_t xcount;
} plot_xinfo_t;
extern const plot_xinfo_t xinfo[XAXIS_COUNT];

#define PLOT_MAX_X 50
typedef struct {
    uint8_t yaxis;
    uint8_t xaxis;
    uint8_t ytiles;
    uint32_t data[PLOT_MAX_X];
} plot_t;
#define PLOT_COUNT 8
extern plot_t plots[PLOT_COUNT];
*/


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


/** Detect the corruption after each test. */
extern void detect_per_test(uint8_t device, uint8_t dir, uint8_t mode, uint32_t* addr);
/** After all TRIGGER_DCACHE_WRITE tests are done, detect and fix corruptions in
the whole memory area (almost whole RDRAM). */
extern void detect_full_scan(uint8_t device, uint8_t dir);

#endif
