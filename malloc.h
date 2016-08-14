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

#define malloc               yam_malloc
#define realloc              yam_realloc
#define free                 yam_free
#define calloc               yam_calloc
#define memalign             yam_memalign

extern __attribute__((constructor)) void yam_init();
extern __attribute__((destructor)) void yam_exit();
extern void yam_free(void* chunk);
extern void* yam_malloc(size_t sz);
extern void* yam_realloc(void* chunk, size_t sz);
extern void* yam_calloc(size_t sz, size_t cnt);
extern void* yam_memalign(size_t alignment, size_t sz);
extern size_t yam_malloc_size(void* ptr);


#ifdef __cplusplus
}
#endif

#endif /* MALLOC_H_ */
