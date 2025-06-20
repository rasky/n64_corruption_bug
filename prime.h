#ifndef PRIME_H
#define PRIME_H
/** Prime the corruption, i.e. do a large DMA of lots of 1s, to put the hardware
into the bad state where future transactions may be corrupted. */

#define PRIME_DEVICE_DMEM 0
#define PRIME_DEVICE_IMEM 1
#define PRIME_DEVICE_PI 2
#define PRIME_DEVICE_COUNT 3

#define PRIME_DIR_RDRAM2RCP 0
#define PRIME_DIR_RCP2RDRAM 1
#define PRIME_DIR_COUNT 2

#define MAX_PRIME_SIZE 4096

/** Get the source address, i.e. the address of the memory prime will copy from.
If you want the destination address, pass !dir. */
extern volatile uint32_t* prime_get_src_addr(uint8_t device, uint8_t dir);

/** Initialize the source for the future DMA to 32-bit pattern. If dir is
PRIME_DIR_RDRAM2RCP, this means filling ram_addr with pattern; if dir is
PRIME_DIR_RCP2RDRAM, this means using slow, one-word-at-a-time writes to
DMEM/IMEM/PI. You only have to run this function when you change the arguments
being passed to prime_go, you don't have to run it before every prime_go.
For PI, uses the end of a 64 MiB flashcart, which assumes that ROM space is
writable. Also assumes that size_bytes is a multiple of 8, ram_addr is aligned
to 16, etc. */
extern void prime_init(uint8_t device, uint8_t dir, uint16_t size_bytes, uint32_t pattern);

/** Initialized the destination for prime to address hashes. Only needs to be
called if check_prime is true. */
extern void prime_check_init(uint8_t device, uint8_t dir, uint16_t size_bytes);

/** Prime the corruption by executing a DMA. Must have been previously
initialized by prime_init. */
extern void prime_go(uint8_t device, uint8_t dir, uint32_t size_bytes, uint32_t count);

#endif
