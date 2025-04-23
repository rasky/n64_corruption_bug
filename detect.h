#ifndef DETECT_H
#define DETECT_H

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
extern void detect_per_test(uint8_t mode, uint32_t* addr);
/** After all TRIGGER_DCACHE_WRITE tests are done, detect and fix corruptions in
the whole memory area (almost whole RDRAM). */
extern void detect_full_scan();

#endif
