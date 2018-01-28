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
 * Algorithm
 * ---------
 * This allocator is based on "The Art of Computer Programming" by Donald Knuth.
 * The algorithm itself is described in :
 *  - Volume 1 - Fundamental Algorithms
 *    - Chapter 2 – Information Structures
 *      - 2.5 Dynamic Storage
 *        - Algorithm A (First-fit method)
 *        - Algorithm C (Liberation with boundary tags)
 *
 * The point of a memory allocator is to manage memory in terms of allocation
 * and liberation requests. Allocation finds and reserves memory for a user,
 * whereas liberation makes that memory available again for future allocations.
 * Memory is not created, it must be available from the start, and the allocator
 * simply tracks its usage with metadata, allocated from memory itself.
 *
 * Data structures
 * ---------------
 * Here is a somewhat graphical representation of how memory is organized in
 * this implementation :
 *
 *   allocated block              free block
 * +------+-----------------+   +------+-----------------+
 * | size | allocation flag |   | size | allocation flag | <- header boundary
 * +------+-----------------+   +------+-----------------+    tag
 * |                        |   | free list node (prev   | <- payload or
 * .       payload          .   | and next pointers)     |    free list node
 * .                        .   +------------------------+
 * .                        .   .                        .
 * .                        .   .                        .
 * .                        .   .                        .
 * +------+-----------------+   +------+-----------------+
 * | size | allocation flag |   | size | allocation flag | <- footer boundary
 * +------+-----------------+   +------+-----------------+    tag
 *
 * Here is a view of multiple contiguous blocks :
 *
 * +------+-----------------+ <--+
 * | size | allocation flag |    |
 * +------+-----------------+    +- single block
 * |                        |    |
 * .       payload          .    |
 * .                        .    |
 * .                        .    |
 * +------+-----------------+    |
 * | size | allocation flag |    |
 * +------+-----------------+ <--+
 * +------+-----------------+
 * | size | allocation flag |
 * +------+-----------------+
 * |                        |
 * .       payload          .
 * .                        .
 * .                        .
 * +------+-----------------+
 * | size | allocation flag |
 * +------+-----------------+
 * +------+-----------------+
 * | size | allocation flag |
 * +------+-----------------+
 * |                        |
 * .       payload          .
 * .                        .
 * .                        .
 * +------+-----------------+
 * | size | allocation flag |
 * +------+-----------------+
 *
 * The reason for the footer boundary tag is merging on liberation. When
 * called, the mem_free() function is given a pointer to a payload. Since
 * the size of a boundary tag is fixed, the address of the whole block
 * can easily be computed. In order to reduce fragmentation, i.e. a state
 * where all free blocks are small and prevent allocating bigger blocks,
 * the allocator attempts to merge neighbor free blocks. Obtaining the
 * address of the next block is easily achieved by simply adding the size
 * of the current block, stored in the boundary tag, to the address of
 * the block. But without a footer boundary tag, finding the address of
 * the previous block is computationally expensive.
 *
 * Alignment
 * ---------
 * The word "aligned" and references to "alignment" in general can be
 * found throughout the documentation of this module. Alignment is a
 * property of a value, usually an address, to be a multiple of a size.
 * This value is said to be "size-byte aligned", or "aligned on a size
 * byte boundary". Common sizes include the processor word or cache line
 * sizes. For example, the x86 architecture is 32-bits, making the word
 * size 4 bytes. Addresses such as 0, 4, 8, 12, 512 and 516 are 4-byte
 * aligned, whereas 1, 2, 3, 511, 513, 514 and 515 aren't.
 *
 *
 * Pointers
 * --------
 * The code in this module makes extensive use of pointer arithmetic and
 * conversion between pointer types. It's important to keep in mind that,
 * in C, the void pointer is meant as a generic reference to objects of
 * any type. As a result, any pointer can be assigned to a void pointer
 * without explicit casting, and a void pointer may be assigned to any
 * pointer as well.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/list.h>
#include <lib/macros.h>

#include "mem.h"
#include "mutex.h"

/*
 * Total size of the backing storage heap.
 *
 * Note that the resulting kernel image file remains much smaller than this.
 * The reason is because the heap is defined as uninitialized data, which are
 * allocated out of the bss section by default. The bss section is filled
 * with zeroes when the kernel image is loaded by the boot loader, as
 * mandated by the ELF specification, which means there is no need to store
 * the heap data, or any other statically allocated uninitialized data, in
 * the kernel image file.
 */
#define MEM_HEAP_SIZE       (64 * 1024)

/*
 * Alignment required on addresses returned by mem_alloc().
 *
 * See the description of mem_alloc() in the public header.
 */
#define MEM_ALIGN           8

/*
 * Minimum size of a block.
 *
 * When free, the payload of a block is used as storage for the free list node.
 */
#define MEM_BLOCK_MIN_SIZE  P2ROUND(((sizeof(struct mem_btag) * 2) \
                                    + sizeof(struct mem_free_node)), MEM_ALIGN)

/*
 * The heap itself must be aligned, so that the first block is also aligned.
 * Assuming all blocks have an aligned size, the last block must also end on
 * an aligned address.
 *
 * This kind of check increases safety and robustness when changing
 * compile-time parameters such as the heap size declared above.
 *
 * Another relevant check would be to make sure the heap is large enough
 * to contain at least one block, but since the minimum block size is
 * defined using the C sizeof operator, unknown to the C preprocessor,
 * this check requires C11 static assertions.
 */
#if !P2ALIGNED(MEM_HEAP_SIZE, MEM_ALIGN)
#error "invalid heap size"
#endif

/*
 * Masks applied on boundary tags to extract the size and the allocation flag.
 */
#define MEM_BTAG_ALLOCATED_MASK ((size_t)0x1)
#define MEM_BTAG_SIZE_MASK      (~MEM_BTAG_ALLOCATED_MASK)

/*
 * Boundary tag.
 *
 * Since the alignment constraint specified for mem_alloc() applies to
 * payloads, not blocks, it's important that boundary tags also be aligned.
 * This is a check that would best be performed with C11 static assertions.
 *
 * In addition, the alignment constraint implies that the least significant
 * bit is always 0. Therefore, this bit is used to store the allocation flag.
 */
struct mem_btag {
    size_t value;
};

/*
 * Memory block.
 *
 * Note the use of a C99 flexible array member. This construction enables
 * the implementation to directly access the payload without pointer
 * arithmetic or casting, and is one of the preferred ways to deal with
 * headers.
 */
struct mem_block {
    struct mem_btag btag;
    char payload[] __aligned(MEM_ALIGN);
};

/*
 * Free list node.
 *
 * This structure is used as the payload of free blocks.
 */
struct mem_free_node {
    struct list node;
};

/*
 * List of free nodes.
 *
 * Here is an example of a TODO entry, a method used to store and retrieve
 * pending tasks using source code only :
 *
 * TODO Statistics counters.
 */
struct mem_free_list {
    struct list free_nodes;
};

/*
 * Memory heap.
 *
 * This allocator uses a single heap initialized with a single large free
 * block. The allocator could be extended to support multiple heaps, each
 * initialized with a single free block, forming an initial free list of
 * more than one block.
 *
 * The heap must be correctly aligned, so that the first block is
 * correctly aligned.
 */
static char mem_heap[MEM_HEAP_SIZE] __aligned(MEM_ALIGN);

/*
 * The unique free list.
 *
 * In order to improve allocation speed, multiple lists may be used, each
 * for a specific size range. Such lists are called segregated free lists
 * and enable more allocation policies, such as instant-fit.
 */
static struct mem_free_list mem_free_list;

/*
 * Global mutex used to serialize access to allocation data.
 */
static struct mutex mem_mutex;

static bool
mem_aligned(size_t value)
{
    return P2ALIGNED(value, MEM_ALIGN);
}

static void *
mem_heap_end(void)
{
    return &mem_heap[sizeof(mem_heap)];
}

static bool
mem_btag_allocated(const struct mem_btag *btag)
{
    return btag->value & MEM_BTAG_ALLOCATED_MASK;
}

static void
mem_btag_set_allocated(struct mem_btag *btag)
{
    btag->value |= MEM_BTAG_ALLOCATED_MASK;
}

static void
mem_btag_clear_allocated(struct mem_btag *btag)
{
    btag->value &= ~MEM_BTAG_ALLOCATED_MASK;
}

static size_t
mem_btag_size(const struct mem_btag *btag)
{
    return btag->value & MEM_BTAG_SIZE_MASK;
}

static void
mem_btag_init(struct mem_btag *btag, size_t size)
{
    btag->value = size;
    mem_btag_set_allocated(btag);
}

static size_t
mem_block_size(const struct mem_block *block)
{
    return mem_btag_size(&block->btag);
}

static struct mem_block *
mem_block_from_payload(void *payload)
{
    size_t offset;

    /*
     * Here, payload refers to the payload member in struct mem_block, not
     * the local variable.
     */
    offset = offsetof(struct mem_block, payload);

    /*
     * Always keep pointer arithmetic in mind !
     *
     * The rule is fairly simple : whenever arithmetic operators are used
     * on pointers, the operation is scaled on the type size, so that e.g.
     * adding 1 means pointing to the next element. A good way to remember
     * this is to remember that pointers can be used as arrays, so that
     * both these expressions are equivalent :
     *  - &ptr[1]
     *  - ptr + 1
     *
     * As a result, when counting in bytes and not in objects, it is
     * necessary to cast into a suitable pointer type. The char * type
     * is specifically meant for this as C99 mandates that sizeof(char) be 1
     * (6.5.3.4 The sizeof operator).
     *
     * As a side note, a GNU extension allows using pointer arithmetic on
     * void pointers, where the "size of void" is 1, turning pointer
     * arithmetic on void pointers into byte arithmetic, allowing this
     * expression to be written as :
     *
     * return payload - offset;
     *
     * See https://gcc.gnu.org/onlinedocs/gcc-6.4.0/gcc/Pointer-Arith.html
     */
    return (struct mem_block *)((char *)payload - offset);
}

static void *
mem_block_end(const struct mem_block *block)
{
    /* See mem_block_from_payload() */
    return (struct mem_block *)((char *)block + mem_block_size(block));
}

static struct mem_btag *
mem_block_header_btag(struct mem_block *block)
{
    return &block->btag;
}

static struct mem_btag *
mem_block_footer_btag(struct mem_block *block)
{
    struct mem_btag *btag;

    btag = mem_block_end(block);

    /*
     * See ISO/IEC 9899:1999, 6.5.2.1 "Array subscripting" :
     * "A postfix expression followed by an expression in square brackets []
     * is a subscripted designation of an element of an array object. The
     * definition of the subscript operator [] is that E1[E2] is identical to
     * (*((E1)+(E2)))".
     *
     * This, by the way, implies the following equivalent expressions :
     *  - &btag[-1]
     *  - &(-1)[btag]
     *  - &*(btag - 1)
     *  - btag - 1
     */
    return &btag[-1];
}

static struct mem_block *
mem_block_prev(struct mem_block *block)
{
    struct mem_btag *btag;

    if ((char *)block == mem_heap) {
        return NULL;
    }

    btag = mem_block_header_btag(block);
    return (struct mem_block *)((char *)block - mem_btag_size(&btag[-1]));
}

static struct mem_block *
mem_block_next(struct mem_block *block)
{
    block = mem_block_end(block);

    if ((void *)block == mem_heap_end()) {
        return NULL;
    }

    return block;
}

static bool
mem_block_allocated(struct mem_block *block)
{
    return mem_btag_allocated(mem_block_header_btag(block));
}

static void
mem_block_set_allocated(struct mem_block *block)
{
    mem_btag_set_allocated(mem_block_header_btag(block));
    mem_btag_set_allocated(mem_block_footer_btag(block));
}

static void
mem_block_clear_allocated(struct mem_block *block)
{
    mem_btag_clear_allocated(mem_block_header_btag(block));
    mem_btag_clear_allocated(mem_block_footer_btag(block));
}

static void
mem_block_init(struct mem_block *block, size_t size)
{
    mem_btag_init(mem_block_header_btag(block), size);
    mem_btag_init(mem_block_footer_btag(block), size);
}

static void *
mem_block_payload(struct mem_block *block)
{
    return block->payload;
}

static struct mem_free_node *
mem_block_get_free_node(struct mem_block *block)
{
    assert(!mem_block_allocated(block));
    return mem_block_payload(block);
}

static bool
mem_block_inside_heap(const struct mem_block *block)
{
    void *heap_end;

    heap_end = mem_heap_end();
    return (((char *)block >= mem_heap)
            && ((void *)block->payload < heap_end)
            && ((void *)mem_block_end(block) <= heap_end));
}

static void
mem_free_list_add(struct mem_free_list *list, struct mem_block *block)
{
    struct mem_free_node *free_node;

    assert(mem_block_allocated(block));

    mem_block_clear_allocated(block);
    free_node = mem_block_get_free_node(block);

    /*
     * Free blocks may be added at either the head or the tail of a list.
     * In this case, it's normally better to add at the head, because the
     * first-fit algorithm implementation starts from the beginning. This
     * means there is a good chance that a block recently freed may "soon"
     * be allocated again. Since it's likely that this block was accessed
     * before it was freed, there is a good chance that (part of) its memory
     * is still in the processor cache, potentially increasing the chances
     * of cache hits and saving a few expensive accesses from the processor
     * to memory. This is an example of inexpensive micro-optimization.
     */
    list_insert_head(&list->free_nodes, &free_node->node);
}

static void
mem_free_list_remove(struct mem_free_list *list, struct mem_block *block)
{
    struct mem_free_node *free_node;

    (void)list;

    assert(!mem_block_allocated(block));

    free_node = mem_block_get_free_node(block);
    list_remove(&free_node->node);
    mem_block_set_allocated(block);
}

static struct mem_block *
mem_free_list_find(struct mem_free_list *list, size_t size)
{
    struct mem_free_node *free_node;
    struct mem_block *block;

    /*
     * The algorithmic complexity of this operation is O(n) [1] which
     * basically means the maximum number of steps, and time, for the
     * operation to complete depend on the number of elements in the list.
     * This is one of the main reasons why memory allocation is generally
     * avoided in interrupt handlers and real-time applications. Special
     * allocators with guaranteed constant time or a fixed and known worst
     * case time, may be used in these cases.
     *
     * [1] https://en.wikipedia.org/wiki/Big_O_notation
     */
    list_for_each_entry(&list->free_nodes, free_node, node) {
        block = mem_block_from_payload(free_node);

        if (mem_block_size(block) >= size) {
            return block;
        }
    }

    return NULL;
}

static void
mem_free_list_init(struct mem_free_list *list)
{
    list_init(&list->free_nodes);
}

static bool
mem_block_inside(struct mem_block *block, void *addr)
{
    return (addr >= (void *)block) && (addr < mem_block_end(block));
}

static bool
mem_block_overlap(struct mem_block *block1, struct mem_block *block2)
{
    return mem_block_inside(block1, block2)
           || mem_block_inside(block2, block1);
}

static struct mem_block *
mem_block_split(struct mem_block *block, size_t size)
{
    struct mem_block *block2;
    size_t total_size;

    assert(mem_block_allocated(block));
    assert(mem_aligned(size));

    if (mem_block_size(block) < (size + MEM_BLOCK_MIN_SIZE)) {
        return NULL;
    }

    total_size = mem_block_size(block);
    mem_block_init(block, size);
    block2 = mem_block_end(block);
    mem_block_init(block2, total_size - size);

    return block2;
}

static struct mem_block *
mem_block_merge(struct mem_block *block1, struct mem_block *block2)
{
    size_t size;

    assert(!mem_block_overlap(block1, block2));

    if (mem_block_allocated(block1) || mem_block_allocated(block2)) {
        return NULL;
    }

    mem_free_list_remove(&mem_free_list, block1);
    mem_free_list_remove(&mem_free_list, block2);
    size = mem_block_size(block1) + mem_block_size(block2);

    if (block1 > block2) {
        block1 = block2;
    }

    mem_block_init(block1, size);
    mem_free_list_add(&mem_free_list, block1);
    return block1;
}

void
mem_setup(void)
{
    struct mem_block *block;

    block = (struct mem_block *)mem_heap;
    mem_block_init(block, sizeof(mem_heap));
    mem_free_list_init(&mem_free_list);
    mem_free_list_add(&mem_free_list, block);
    mutex_init(&mem_mutex);
}

static size_t
mem_convert_to_block_size(size_t size)
{
    /*
     * Make sure all blocks have a correctly aligned size. That, and the fact
     * that the heap address is also aligned, means all block addresses are
     * aligned.
     */
    size = P2ROUND(size, MEM_ALIGN);
    size += sizeof(struct mem_btag) * 2;

    if (size < MEM_BLOCK_MIN_SIZE) {
        size = MEM_BLOCK_MIN_SIZE;
    }

    return size;
}

void *
mem_alloc(size_t size)
{
    struct mem_block *block, *block2;
    void *ptr;

    if (size == 0) {
        return NULL;
    }

    size = mem_convert_to_block_size(size);

    mutex_lock(&mem_mutex);

    block = mem_free_list_find(&mem_free_list, size);

    if (block == NULL) {
        mutex_unlock(&mem_mutex);
        return NULL;
    }

    mem_free_list_remove(&mem_free_list, block);
    block2 = mem_block_split(block, size);

    if (block2 != NULL) {
        mem_free_list_add(&mem_free_list, block2);
    }

    mutex_unlock(&mem_mutex);

    ptr = mem_block_payload(block);
    assert(mem_aligned((uintptr_t)ptr));
    return ptr;
}

void
mem_free(void *ptr)
{
    struct mem_block *block, *tmp;

    if (!ptr) {
        return;
    }

    assert(mem_aligned((uintptr_t)ptr));

    block = mem_block_from_payload(ptr);
    assert(mem_block_inside_heap(block));

    mutex_lock(&mem_mutex);

    mem_free_list_add(&mem_free_list, block);

    tmp = mem_block_prev(block);

    if (tmp) {
        tmp = mem_block_merge(block, tmp);

        if (tmp) {
            block = tmp;
        }
    }

    tmp = mem_block_next(block);

    if (tmp) {
        mem_block_merge(block, tmp);
    }

    mutex_unlock(&mem_mutex);
}
