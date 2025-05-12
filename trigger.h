#ifndef TRIGGER_H
#define TRIGGER_H
/** Trigger the corruption, i.e. do a memory operation which gets corrupted,
after the system has been primed. This also includes detection. */

#define TRIGGER_MODE_DCACHE_READ 0
#define TRIGGER_MODE_DCACHE_WRITE 1
#define TRIGGER_MODE_COUNT 2

#define DCACHE_LINE_SIZE 16
#define DCACHE_LINE_COUNT 512
#define DCACHE_SIZE_BYTES (DCACHE_LINE_SIZE*DCACHE_LINE_COUNT)

extern uint32_t* most_of_dram;
extern uint32_t* most_of_dram_end;

/** Initialize the trigger system, filling most of RAM with hash values. */
extern void trigger_init();
/** Returns a pointer to an address which needs to be passed into the other
functions. hash is a pseudorandom number to generate an address offset. */
extern uint32_t* trigger_get_addr(uint32_t hash);
/** Set up the initial state for triggering, before prime. */
extern void trigger_setup(uint8_t tmode, uint32_t* addr);
/** Trigger the corruption. */
extern void trigger_go(uint8_t tmode, uint32_t* addr);
/** Things that need to happen after the trigger (e.g. dcache writeback). */
extern void trigger_after(uint8_t tmode, uint32_t* addr);

#endif
