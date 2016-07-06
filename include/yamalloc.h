/*
 * yamalloc.h
 *
 *  Created on: Jul 5, 2016
 *      Author: innocentevil
 */

#ifndef YAMALLOC_H_
#define YAMALLOC_H_

#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif



extern __attribute__((malloc)) void* yam_malloc(size_t sz);
extern __attribute__((malloc)) void* yam_realloc(void* chunk, size_t sz);
extern __attribute__((malloc)) void* yam_calloc(size_t sz, size_t cnt);
extern void yam_free(void* chunk);

extern void yam_init();


#ifdef __cplusplus
}
#endif


#endif /* YAMALLOC_H_ */
