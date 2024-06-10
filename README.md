## N64 Hardware Bug

This repository contains information and reproduction examples of what it
appears to be a Nintendo 64 hardware bug related to RDRAM and RSP DMA transfers.
This bug has probably gone unnoticed for many years because it requires very
specific conditions to reproduce, but it is not impossible for it to happen
during a normal working condition (eg: within a game).

## Short description of the bug

After a RSP DMA transferring a sequence of fixed bytes with value 0xFF,
*sometimes* one or more CPU i-cache or d-cache fetches appear to partially get
corrupted; specifically a portion of the cacheline seem to contain the wrong data
(data coming from a different RDRAM location). This of course causes a
corruption in what the CPU read from memory, normally leading to a software crash.

## Detailed description

The first ingredient to trigger the bug is running a RSP DMA. The direction
of the transfer does not matter, the bug can trigger either while transferring
RDRAM to IMEM/DMEM, or the other way around. The important part is that this
transfer must move bytes with some very specific value, and be of a non
trivial length.

The most common reproduction can be made with the value 0xFF, which appears the
one that triggers most often the bug. Reproductions happened also with different
values such as 0xEF, 0xAA, 0x0F. The value does have an impact on the likeliness of
triggering the bug, so it is hard to completely rule out specific values,
but those values are the ones that normally do trigger the bug in a short
amount of time.

The transfer must have a minimal length of between 1 KiB and 2 KiB. Again, since
different sizes seemingly only affect the likeliness of the bug to happen, it is
hard to rule out

The bug triggers *after* the transfer is finished. This is probably the most
surprising part, because it would seem more likely that the bug somehow occurred
while the transfer was in progress because of some RCP hardware bug, but instead
we easily reproduce it even just by doing nothing while the transfer is running
and then proceeding to running code.

If the bug triggers, one of the subsequent cache fetches performed by the VR4300
CPU, either i-cache (code) or d-cache (data) gets corrupted. The corruption
appears to be as follows: a certain amount of bytes at the beginning of the
cacheline are correctly fetched; the remaining bytes at the end of the cachelines
are corrupted and do not match the RDRAM contents. In the case of i-cache
fetches, it can be shown that the corrupted part of the cacheline contain
valid MIPS opcodes. It is not clear if the previous contents of the cacheline
are partially preserved, or different RDRAM locations are fetched.

It is important to notice that the likeliness of the bug seems to somehow be
related to the temperature of the console. When the console is cold, it is
normally very easy to reproduce. As the console gets progressively warmer,
the likeliness decreases.

## Hypothesis on the root cause

Currently, the root cause is unknown. The hypothesis is as follows:

* RSP DMA causes high-frequency bursts of data to/from RDRAM.
* The value 0xFF forces many bits to stay high on the bus for a long time.
* RDRAM current is somehow incorrectly calibrated, and the chips cannot cope
  with this transfer correctly.
* After that, chips are confused in some way, and a read burst from the CPU
  fails somehow

We believe the issue is related to RDRAM calibration because it is possible
to seemingly fix the bug by forcing a lower current target on the chips. By
patching the IPL3 to force a lower target than that the calibration has suggested,
the bug cannot be reproduced anymore. The target must be quite a bit lower: in
the hardware register scale (0x00 - 0x3F), you need to decrease the value by
something like 0xA to workaround the bug.

This might point to the fact that the calibration process is not correctly
implemented by IPL3; or maybe the bug is elsewhere but lowering the current
manages to hide it.

## FAQ

### How did you find out about this bug?

We were experimenting with some custom microcode in Tiny3D and
libdragon to clear the Z-buffer via the RSP. This requires running RSP DMA
transfers to RDRAM with the fixed value of 0xFFFC which is the reset value
for the Z-buffer. During initial development of the feature, we had a fixed
value of 0xFFFF instead and we started noticing some random crashes.

### Does this bug reproduce with official IPL3s from Nintendo?

Yes, we can reproduce the bug also with Nintendo IPL3, not only with
libdragon's open source IPL3. There are surely small differences in the way
RDRAM is calibrated and initialized between the two. If this is not a hardware
bug but rather a RDRAM configuration bug, it must exist in both IPL3s.

### Does this bug reproduce on all consoles?

It does reproduce on all consoles we tested, so it appears not to be specific
of regions, or different motherboards.
