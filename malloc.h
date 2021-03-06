/*
 * malloc.h
 *
 *  Created on: Aug 12, 2016
 *      Author: innocentevil
 */

#ifndef MALLOC_H_
#define MALLOC_H_

#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __YAM_MALLOC

#define malloc(size)         yam_malloc(size)
#define realloc(ptr,size)    yam_realloc(ptr,size)
#define free(ptr)            yam_free(ptr)
#define calloc(size,cnt)     yam_calloc(size,cnt)
#define memalign(align,sz)   yam_memalign(align,sz)
#define malloc_size(ptr)     yam_malloc_size(ptr)

#endif


extern __attribute__((constructor)) void yam_init();
extern __attribute__((destructor)) void yam_exit();
extern void yam_free(void* chunk);
extern void* yam_malloc(size_t sz);
extern void* yam_realloc(void* chunk, size_t sz);
extern void* yam_calloc(size_t sz, size_t cnt);
extern void* yam_memalign(size_t alignment, size_t sz);
extern size_t yam_malloc_size(void* ptr);

#define YAMALLOC_VERSION_MAJOR       0
#define YAMALLOC_VERSION_MINOR       1


#ifdef __cplusplus
}
#endif

#endif /* MALLOC_H_ */
