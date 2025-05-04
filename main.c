#include <libdragon.h>

#include "test.h"
#include "xact_critical_section.h"

int main() {
    // Move the stack to 0x80040000 (grows downward). Currently __bss_end is
    // 0x80023b00, so there should be plenty of room for some more code and
    // the stack. This sp manipulation can't be done in a more complex function
    // where the compiler may expect the stack to remain valid from entry to
    // exit.
    asm("move $sp, %0" : : "r" (0x8003FFE0));
    
    disable_interrupts();
    debug_init_usblog();
    debugf("\033[2J\033[H\n");
    test_main();
    dummy_function_end();
}
