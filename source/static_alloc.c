#define SMSPLUS_STATIC_ALLOC_NO_MACROS
#include "static_alloc.h"

#if defined(SMSPLUS_STATIC_MEMORY)
#include <stdint.h>
#include <string.h>

#ifndef SMSPLUS_STATIC_MEMORY_BYTES
#define SMSPLUS_STATIC_MEMORY_BYTES (16u * 1024u * 1024u)
#endif

#define SMSPLUS_STATIC_ALIGN ((size_t)sizeof(uintptr_t) > 8u ? (size_t)sizeof(uintptr_t) : 8u)
#define SMSPLUS_STATIC_MAGIC 0x534D5350u /* SMSP */

typedef struct smsplus_static_block_s
{
    size_t size;
    struct smsplus_static_block_s *next;
    uint32_t magic;
    uint8_t free;
} smsplus_static_block_t;

typedef union
{
    uint64_t align_u64;
    void *align_ptr;
    double align_double;
    uint8_t bytes[SMSPLUS_STATIC_MEMORY_BYTES];
} smsplus_static_arena_t;

static smsplus_static_arena_t smsplus_static_arena;
static smsplus_static_block_t *smsplus_static_head;
static size_t smsplus_static_used_bytes;
static size_t smsplus_static_peak_bytes;

static size_t smsplus_static_align_up(size_t value)
{
    return (value + (SMSPLUS_STATIC_ALIGN - 1u)) & ~(SMSPLUS_STATIC_ALIGN - 1u);
}

static void smsplus_static_init(void)
{
    if (smsplus_static_head)
        return;

    smsplus_static_head = (smsplus_static_block_t *)smsplus_static_arena.bytes;
    smsplus_static_head->size = SMSPLUS_STATIC_MEMORY_BYTES - sizeof(smsplus_static_block_t);
    smsplus_static_head->next = NULL;
    smsplus_static_head->magic = SMSPLUS_STATIC_MAGIC;
    smsplus_static_head->free = 1;
    smsplus_static_used_bytes = 0;
    smsplus_static_peak_bytes = 0;
}

static void smsplus_static_note_alloc(size_t size)
{
    smsplus_static_used_bytes += size;
    if (smsplus_static_used_bytes > smsplus_static_peak_bytes)
        smsplus_static_peak_bytes = smsplus_static_used_bytes;
}

static void smsplus_static_split(smsplus_static_block_t *block, size_t size)
{
    uint8_t *base;
    smsplus_static_block_t *next;
    size_t remaining;

    if (block->size < size + sizeof(smsplus_static_block_t) + SMSPLUS_STATIC_ALIGN)
        return;

    remaining = block->size - size - sizeof(smsplus_static_block_t);
    base = (uint8_t *)(block + 1);
    next = (smsplus_static_block_t *)(base + size);
    next->size = remaining;
    next->next = block->next;
    next->magic = SMSPLUS_STATIC_MAGIC;
    next->free = 1;

    block->size = size;
    block->next = next;
}

static void smsplus_static_coalesce(void)
{
    smsplus_static_block_t *block = smsplus_static_head;

    while (block && block->next)
    {
        if (block->free && block->next->free)
        {
            smsplus_static_block_t *next = block->next;
            block->size += sizeof(smsplus_static_block_t) + next->size;
            block->next = next->next;
        }
        else
        {
            block = block->next;
        }
    }
}

static int smsplus_static_owns(const void *ptr)
{
    const uint8_t *p = (const uint8_t *)ptr;
    return p >= smsplus_static_arena.bytes && p < (smsplus_static_arena.bytes + SMSPLUS_STATIC_MEMORY_BYTES);
}

void *smsplus_static_malloc(size_t size)
{
    smsplus_static_block_t *block;

    if (!size)
        return NULL;

    smsplus_static_init();
    size = smsplus_static_align_up(size);

    for (block = smsplus_static_head; block; block = block->next)
    {
        if (block->free && block->size >= size)
        {
            smsplus_static_split(block, size);
            block->free = 0;
            smsplus_static_note_alloc(block->size);
            return (void *)(block + 1);
        }
    }

    return NULL;
}

void *smsplus_static_calloc(size_t count, size_t size)
{
    size_t total;
    void *ptr;

    if (count && size > ((size_t)-1) / count)
        return NULL;

    total = count * size;
    ptr = smsplus_static_malloc(total);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void smsplus_static_free(void *ptr)
{
    smsplus_static_block_t *block;

    if (!ptr)
        return;

    smsplus_static_init();
    if (!smsplus_static_owns(ptr))
        return;

    block = ((smsplus_static_block_t *)ptr) - 1;
    if (block->magic != SMSPLUS_STATIC_MAGIC || block->free)
        return;

    block->free = 1;
    if (smsplus_static_used_bytes >= block->size)
        smsplus_static_used_bytes -= block->size;
    else
        smsplus_static_used_bytes = 0;
    smsplus_static_coalesce();
}

void *smsplus_static_realloc(void *ptr, size_t size)
{
    smsplus_static_block_t *block;
    void *new_ptr;
    size_t copy_size;

    if (!ptr)
        return smsplus_static_malloc(size);
    if (!size)
    {
        smsplus_static_free(ptr);
        return NULL;
    }

    smsplus_static_init();
    if (!smsplus_static_owns(ptr))
        return NULL;

    block = ((smsplus_static_block_t *)ptr) - 1;
    if (block->magic != SMSPLUS_STATIC_MAGIC || block->free)
        return NULL;

    size = smsplus_static_align_up(size);
    if (block->size >= size)
    {
        size_t old_size = block->size;
        smsplus_static_split(block, size);
        if (old_size > block->size && smsplus_static_used_bytes >= old_size - block->size)
            smsplus_static_used_bytes -= old_size - block->size;
        return ptr;
    }

    if (block->next && block->next->free &&
        block->size + sizeof(smsplus_static_block_t) + block->next->size >= size)
    {
        size_t old_size = block->size;
        smsplus_static_block_t *next = block->next;
        block->size += sizeof(smsplus_static_block_t) + next->size;
        block->next = next->next;
        smsplus_static_split(block, size);
        smsplus_static_used_bytes += block->size - old_size;
        if (smsplus_static_used_bytes > smsplus_static_peak_bytes)
            smsplus_static_peak_bytes = smsplus_static_used_bytes;
        return ptr;
    }

    new_ptr = smsplus_static_malloc(size);
    if (!new_ptr)
        return NULL;

    copy_size = block->size < size ? block->size : size;
    memcpy(new_ptr, ptr, copy_size);
    smsplus_static_free(ptr);
    return new_ptr;
}

void smsplus_static_reset(void)
{
    smsplus_static_head = NULL;
    smsplus_static_init();
}

size_t smsplus_static_memory_used(void)
{
    return smsplus_static_used_bytes;
}

size_t smsplus_static_memory_peak(void)
{
    return smsplus_static_peak_bytes;
}

size_t smsplus_static_memory_capacity(void)
{
    return SMSPLUS_STATIC_MEMORY_BYTES;
}

#endif /* SMSPLUS_STATIC_MEMORY */
