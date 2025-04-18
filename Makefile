BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: n64_corruption_bug.z64

src = main.c prime.c trigger.c
asm = xact_critical_section.S

main.c prime.c: prime.h

main.c trigger.c: trigger.h

main.c: xact_critical_section.h

$(BUILD_DIR)/n64_corruption_bug.elf: $(src:%.c=$(BUILD_DIR)/%.o) $(asm:%.S=$(BUILD_DIR)/%.o)

n64_corruption_bug.z64: N64_ROM_TITLE="N64 Corruption Bug"

clean:
	rm -rf $(BUILD_DIR) filesystem n64_corruption_bug.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
