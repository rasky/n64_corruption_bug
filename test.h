#ifndef TEST_H
#define TEST_H

#define TEST_FLAG_NONE          0
#define TEST_FLAG_COUNT_POWER2 (1 << 0)
#define TEST_FLAG_EDIT_POWER2  (1 << 1)

typedef struct {
    const char* label;
    const char* const* value_labels;
    uint32_t min;
    uint32_t max;
    uint32_t selected;
    uint32_t current;
    uint32_t last;
    uint32_t flags;
} test_param_t;

#define P_SIZE 0
#define P_ZEROS 1
#define P_DEVICE 2
#define P_DIR 3
#define P_TMODE 4
#define P_OFFSETS 5
#define P_REPEATS 6
#define P_COUNT 7

extern test_param_t params[P_COUNT];
extern bool test_running;

extern uint32_t next_param_value(int32_t p, bool sel, bool edit, bool reverse);
extern void test_main();

#endif
