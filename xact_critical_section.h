#include <libdragon.h>

extern void xact_critical_section(
    uint8_t device, uint8_t dir, uint32_t size,
    uint8_t tmode, uint32_t* taddr, uint8_t rcpcc);
extern void dummy_function_end();
