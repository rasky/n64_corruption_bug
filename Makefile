BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: n64_corruption_bug.z64

hdr = \
  hashes.h \
  controller.h \
  tui.h \
  test.h \
  prime.h \
  trigger.h \
  detect.h \
  xact_critical_section.h
# Order matters; from test.c through xact_critical_section.S will be in icache
src = \
  main.c \
  hashes.c \
  controller.c \
  tui.c \
  test.c \
  prime.c \
  trigger.c \
  detect.c
asm = xact_critical_section.S

$(src): $(hdr)

$(BUILD_DIR)/n64_corruption_bug.elf: $(src:%.c=$(BUILD_DIR)/%.o) $(asm:%.S=$(BUILD_DIR)/%.o)

n64_corruption_bug.z64: N64_ROM_TITLE="N64 Corruption Bug"

clean:
	rm -rf $(BUILD_DIR) filesystem n64_corruption_bug.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
