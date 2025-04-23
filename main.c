#include <libdragon.h>

#include "test.h"
#include "xact_critical_section.h"

int main() {
    disable_interrupts();
    debug_init_usblog();
    debugf("\033[2J\033[HInitializing...\n");
    test_main();
    dummy_function_end();
}
