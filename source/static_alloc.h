#ifndef SMSPLUS_STATIC_ALLOC_H_
#define SMSPLUS_STATIC_ALLOC_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SMSPLUS_STATIC_MEMORY)
void *smsplus_static_malloc(size_t size);
void *smsplus_static_calloc(size_t count, size_t size);
void *smsplus_static_realloc(void *ptr, size_t size);
void smsplus_static_free(void *ptr);
void smsplus_static_reset(void);
size_t smsplus_static_memory_used(void);
size_t smsplus_static_memory_peak(void);
size_t smsplus_static_memory_capacity(void);
#endif

#ifdef __cplusplus
}
#endif

#if defined(SMSPLUS_STATIC_MEMORY) && !defined(SMSPLUS_STATIC_ALLOC_NO_MACROS) && !defined(__cplusplus)
#define malloc(size) smsplus_static_malloc(size)
#define calloc(count, size) smsplus_static_calloc((count), (size))
#define realloc(ptr, size) smsplus_static_realloc((ptr), (size))
#define free(ptr) smsplus_static_free(ptr)
#endif

#endif /* SMSPLUS_STATIC_ALLOC_H_ */
