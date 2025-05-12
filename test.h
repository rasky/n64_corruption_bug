#ifndef TEST_H
#define TEST_H

#define TEST_FLAG_NONE          0
#define TEST_FLAG_EDIT_POWER2  (1 << 0)

typedef uint32_t (*ParamConversionFunc)(uint32_t in);

typedef struct {
    const char* label;
    const char* const* value_labels;
    ParamConversionFunc conversion;
    uint32_t max;
    uint32_t flags;
} test_param_info_t;

typedef struct {
    uint32_t selected;
    uint32_t current;
    uint32_t real;
} test_param_state_t;

#define P_RCPCC   0
#define P_SIZE    1
#define P_ZEROS   2
#define P_DEVICE  3
#define P_DIR     4
#define P_TMODE   5
#define P_OFFSETS 6
#define P_REPEATS 7
#define P_COUNT   8

extern const test_param_info_t param_info[P_COUNT];
extern test_param_state_t param_state[P_COUNT];
extern bool test_running;
extern bool test_all_disabled;

#define RCPCC_COUNT 12

extern uint32_t next_param_value(int32_t p, bool sel, bool edit, bool reverse);
extern void test_reset();
extern void test_main();

#endif
