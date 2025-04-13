#ifndef TRIGGER_H
#define TRIGGER_H
/** Trigger the corruption, i.e. do a memory operation which gets corrupted,
after the system has been primed. This also includes detection. */

#define TRIGGER_DCACHE_READ 0
#define TRIGGER_DCACHE_WRITE 1

extern uint32_t integer_hash(uint32_t x);
/** Initialize the trigger system, filling most of RAM with hash values. */
extern void trigger_init();
/** Returns a pointer to an address which needs to be passed into the other
functions. offset is a number of bytes, must be a multiple of 16, to offset the
operations by. This is needed because the corruption appears to involve clearing
address bits, so if the transactions always start at an address with a lot of
0s, the corruption may not appear. */
extern uint32_t* trigger_get_addr(uint32_t offset);
/** Set up the initial state for triggering, before prime. */
extern void trigger_setup(uint8_t mode, uint32_t* addr);
/** Trigger the corruption. */
extern void trigger_go(uint8_t mode, uint32_t* addr);
/** Detect the corruption. */
extern void trigger_detect(uint8_t mode, uint32_t* addr);
/** Render results in the terminal. */
extern void trigger_tui_render();

#define SYMPTOM_ZERO 1
#define SYMPTOM_ADDR 2
#define SYMPTOM_DATA 4
#define SYMPTOM_OTHER 8

extern uint8_t test_device;
extern uint8_t test_dir;
extern uint8_t test_tmode;

#endif
