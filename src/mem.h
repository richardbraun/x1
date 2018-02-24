/*
 * Copyright (c) 2017 Richard Braun.
 * Copyright (c) 2017 Jerko Lenstra.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *
 * Kernel dynamic memory allocator.
 *
 * Here, the word "dynamic" is used in opposition to "static", which denotes
 * memory allocated at compile time by the linker.
 */

#ifndef MEM_H
#define MEM_H

#include <stddef.h>

/*
 * Initialize the mem module.
 */
void mem_setup(void);

/*
 * Allocate memory.
 *
 * This function conforms to the specification of the standard malloc()
 * function, i.e. :
 *  - The size argument is the allocation request size, in bytes.
 *  - An allocation size of 0 is permitted.
 *  - The content of the allocated block is uninitialized.
 *  - The returned value is the address of the allocated block of memory.
 *  - The address of the allocated block is aligned to the maximum built-in
 *    type size. Since this code targets the 32-bits i386 architecture, the
 *    largest built-in type is unsigned int, resulting in addresses aligned
 *    to 4 bytes boundaries. Here, "built-in" means natively supported by
 *    the processor. The document that defines the size of built-in types
 *    is the ABI (Application Binary Interface) specification, in this case
 *    System V Intel386 ABI [1] (see the GCC -mabi option for x86). The ABI
 *    normally uses one of the most common data models [2] for C types, in
 *    this case ILP32 (for int/long/pointers 32-bits).
 *
 * This last detail is important because C specifies the alignment of both
 * built-in and aggregate types. In particular, the alignment of structure
 * members must match the alignment of their respective types
 * (ISO/IEC 9899:1999, 6.7.2.1 "Structure and union specifiers", 12 "Each
 * non-bit-field member of a structure or union object is aligned in an
 * implementation-defined manner appropriate to its type". A compiler may
 * safely assume that structure member accesses are correctly aligned and
 * generate instructions assuming this alignment.
 *
 * On x86, this doesn't matter too much, because unaligned accesses have
 * always been supported, although they are less performant, since the
 * processor potentially has more work to do. For example, if an unaligned
 * variable crosses a cache line boundary, the processor may have to load
 * two cache lines instead of one.
 *
 * On other architectures, unaligned accesses may simply not be supported,
 * and generate exceptions.
 *
 * [1] http://www.sco.com/developers/devspecs/abi386-4.pdf
 * [2] http://www.unix.org/version2/whatsnew/lp64_wp.html
 */
void * mem_alloc(size_t size);

/*
 * Free memory.
 *
 * This function conforms to the specification of the standard free()
 * function, i.e. :
 *  - It may safely be called with a NULL argument.
 *  - Otherwise, it may only be passed memory addresses returned by mem_alloc().
 */
void mem_free(void *ptr);

#endif /* MEM_H */
