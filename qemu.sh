#!/bin/sh

# Start the QEMU emulator with options doing the following :
#  - GDB remote access on the local TCP port 1234
#
# In order to dump all exceptions and interrupts to a log file, you may add
# the following options :
#       -d int \
#       -D int_debug.log \
#
# Note that these debugging options do not work when KVM is enabled.

qemu-system-arm \
        -M netduino2 \
        -cpu cortex-m3 \
        -gdb tcp::1234 \
        -monitor stdio \
        -d guest_errors -kernel x1
