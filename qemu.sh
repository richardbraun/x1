#!/bin/sh

# Start the QEMU emulator with options doing the following :
#  - GDB remote access on the local TCP port 1234
#  - 64MB of physical memory (RAM)
#  - No video device (automatically sets the first COM port as the console)
#  - Boot from the generated cdrom image.
#
# In order to dump all exceptions and interrupts to a log file, you may add
# the following options :
#       -d int \
#       -D int_debug.log \
#
# Note that these debugging options do not work when KVM is enabled.
qemu-system-i386 \
        -gdb tcp::1234 \
        -m 64 \
        -nographic \
        -kernel x1
