MAKEFLAGS += --no-builtin-rules
MAKEFLAGS += --no-builtin-variables

# The project version.
VERSION = 1.0

# The selected C compiler.
#
# Here is an example of overriding the compiler :
# $ make CC=clang
CC = arm-none-eabi-gcc

# C preprocessor flags.
#
# Use man gcc for more details. On Debian, the GNU FDL license is considered
# non free, and as a result, the gcc-doc package is part of the non-free
# components.

# Do not search the standard system directories for header files.
# The kernel is a free standing environment, where no host library can
# work, at least not without (usually heavy) integration work.
X1_CPPFLAGS += -nostdinc

# The -I option is used to add directories to the search list.
X1_CPPFLAGS += -Iinclude -I.

# The -print-file-name=include option prints the directory where the compiler
# headers are located. This directory is normally part of the standard system
# directories for header files, excluded by the -nostdinc option. But the
# C free standing environment still assumes the availability of some headers
# which can be found there.
X1_CPPFLAGS += -isystem $(shell $(CC) -print-file-name=include)

# Pass the project version as a macro.
X1_CPPFLAGS += -DVERSION="$(VERSION)"

# Append user-provided preprocessor flags, if any.
#
# Here is an example that turns off assertions :
# $ make CPPFLAGS=-DNDEBUG
X1_CPPFLAGS += $(CPPFLAGS)

# C flags.
#
# Use man gcc for more details. On Debian, the GNU FDL license is considered
# non free, and as a result, the gcc-doc package is part of the non-free
# components.

# These are common warning options.
X1_CFLAGS = -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes

# Enable warnings about shadow declarations. A shadow declaration occurs
# when a name, e.g. a variable name, "hides" another declaration with the
# same name, but from an outer scope. Here is an example :
#
# int
# main(void)
# {
#     int a;
#
#     {
#         int a;
#     }
# }
#
# Local variables may shadow global variables, which is why global
# names are usually prefixed with a namespace name. Another common
# case is variables declared inside function-like macros, where a
# popular solution is to prefix the names with a number of underscores.
X1_CFLAGS += -Wshadow

# This is currently a Clang-specific warning about declarations that are
# unused, a problem that quickly occurs when using conditional compilation
# like assertions.
X1_CFLAGS += -Wno-unneeded-internal-declaration

# Set the language as C99 with GNU extensions.
X1_CFLAGS += -std=gnu99

# Build with optimizations as specified by the -Os option.
X1_CFLAGS += -Os

# Include debugging symbols, giving inspection tools a lot more debugging
# data to work with, e.g. allowing them to translate between addresses and
# source locations.
X1_CFLAGS += -g

X1_CFLAGS += -mcpu=cortex-m4
X1_CFLAGS += -mthumb

X1_CFLAGS += -fno-PIE
X1_CFLAGS += -fsigned-char

# Target a free standing environment as defined by C99.
#
# This option tells the compiler that it may not assume a hosted environment,
# e.g. that it cannot assume the availability of the C library.
#
# See ISO/IEC 9899:1999 5.1.2.1 "Freestanding environment" for all the details.
X1_CFLAGS += -ffreestanding

# Keep using the frame pointer.
#
# When a function is called, the stack pointer (esp) points to the "end" of
# the stack frame, and the frame or base pointer (ebp) points to the
# "beginning". When optimizations are enabled, the compiler only uses the
# stack pointer as a base pointer to refer to temporary data saved on the
# stack frame, and therefore omits the base pointer to gain a register.
# But this prevents debuggers from easily unwinding the stack since the
# base pointer allowed conveniently retrieving the return address. This
# option tells the compiler to keep the base pointer so that GDB can
# produce valid backtraces.
X1_CFLAGS += -fno-omit-frame-pointer

# Disable the generation of extra code for stack protection, as it requires
# additional support in the kernel which is beyond its scope, and may be
# enabled by default on some distributions.
X1_CFLAGS += -fno-stack-protector

# Disable strict aliasing, a C99 rule that states that pointers of different
# types may never refer to the same memory. Strict aliasing may provide
# performance improvements for some rare cases but may also cause weird bugs
# when casting pointers.
X1_CFLAGS += -fno-strict-aliasing

# Force all uninitialized global variables into a data section instead of
# generating them as "common blocks". If multiple definitions of the same
# global variable are made, this option will make the link fail.
X1_CFLAGS += -fno-common

# Append user-provided compiler flags, if any.
#
# Here are some examples :
# $ make CFLAGS="-O0 -g3"
#   Disable optimizations and include extra debugging information
# $ make CPPFLAGS=-DNDEBUG CFLAGS="-Os -flto -g0"
#   Disable assertions, optimize for size, enable LTO (link-time optimizations),
#   and don't produce debugging information
#
# Because these flags are added last, they can override any flag set in this
# Makefile.
X1_CFLAGS += $(CFLAGS)

# Linker flags.
#
# These are also GCC options, so use man gcc for more details. On Debian,
# the GNU FDL license is considered non free, and as a result, the gcc-doc
# package is part of the non-free components.

# Build a static executable, with no shared library.
X1_LDFLAGS += -static

# Do not use the standard system startup files or libraries when linking.
# The kernel is a free standing environment, where no host library can
# work, at least not without (usually heavy) integration work.
X1_LDFLAGS += -nostdlib

# Disable the generation of a build ID, a feature usually enabled by default
# on many distributions. A build ID is linked in its own special section, so
# disabling it makes the linker script simpler.
X1_LDFLAGS += -Xlinker --build-id=none

# Append user-provided linker flags, if any.
X1_LDFLAGS += $(LDFLAGS)

# Link against libgcc. This library is a companion to the compiler and
# adds support for operations required by C99 but that the hardware
# doesn't provide. An example is 64-bits integer additions on a 32-bits
# processor or a 64-bits processor running in 32-bits protected mode.
LIBS = -lgcc

BINARY = x1

LDS = src/kernel.lds

SOURCES = \
	src/boot.c \
	src/boot_asm.S \
	src/condvar.c \
	src/cpu.c \
	src/cpu_asm.S \
	src/flash.c \
	src/gpio.c \
	src/led.c \
	src/main.c \
	src/mem.c \
	src/mutex.c \
	src/nvic.c \
	src/panic.c \
	src/rcc.c \
	src/stdio.c \
	src/string.c \
	src/sw.c \
	src/systick.c \
	src/thread.c \
	src/timer.c \
	src/uart.c

SOURCES += \
	lib/cbuf.c \
	lib/fmt.c \
	lib/shell.c

OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SOURCES)))

$(BINARY): $(LDS) $(OBJECTS)
	$(CC) $(X1_CPPFLAGS) $(X1_CFLAGS) -o $@ $(X1_LDFLAGS) -Xlinker -T $(LDS) $(OBJECTS) $(LIBS)

%.o: %.c
	$(CC) $(X1_CPPFLAGS) $(X1_CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(X1_CPPFLAGS) $(X1_CFLAGS) -c -o $@ $<

%.lds: %.lds.S
	$(CC) -E -P $(X1_CPPFLAGS) -o $@ $<

clean:
	rm -f $(BINARY) $(OBJECTS)

# Making all sources phony means that make will always consider them and
# the targets using them as dependencies as obsolete. This basically forces
# make to rebuild all files, all the time. It's obviously not the best
# way to compile, but it's simple and reliable.
#
# The modern approach is to make compilation incremental, by generating
# dependency Makefiles with the compiler, which are then included in the
# main Makefile so that a target may only be rebuilt if any of its
# dependencies is newer. Check the X15 project [1] for an example of this
# technique.
#
# [1] https://git.sceen.net/rbraun/x15.git/
.PHONY: clean $(SOURCES) $(LDS).S
