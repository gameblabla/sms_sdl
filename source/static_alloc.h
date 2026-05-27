/*
 * MultiRexZ80
 *
 * Multi-system Z80 emulator based on SMS Plus GX by Eke-Eke, itself based on
 * SMS Plus by Charles MacDonald.
 *
 * Default project license: GPL-2.0-or-later.  File-specific notices below
 * are retained and take precedence for imported or derived components,
 * including MAME-derived code and other third-party modules.
 */

#ifndef MULTIREXZ80_STATIC_ALLOC_H_
#define MULTIREXZ80_STATIC_ALLOC_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MULTIREXZ80_STATIC_MEMORY)
void *multirexz80_static_malloc(size_t size);
void *multirexz80_static_calloc(size_t count, size_t size);
void *multirexz80_static_realloc(void *ptr, size_t size);
void multirexz80_static_free(void *ptr);
void multirexz80_static_reset(void);
size_t multirexz80_static_memory_used(void);
size_t multirexz80_static_memory_peak(void);
size_t multirexz80_static_memory_capacity(void);
#endif

#ifdef __cplusplus
}
#endif

#if defined(MULTIREXZ80_STATIC_MEMORY) && !defined(MULTIREXZ80_STATIC_ALLOC_NO_MACROS) && !defined(__cplusplus)
#define malloc(size) multirexz80_static_malloc(size)
#define calloc(count, size) multirexz80_static_calloc((count), (size))
#define realloc(ptr, size) multirexz80_static_realloc((ptr), (size))
#define free(ptr) multirexz80_static_free(ptr)
#endif

#endif /* MULTIREXZ80_STATIC_ALLOC_H_ */
