/*
 * nwtmalloc.h
 *
 *  Created on: May 8, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_NWTMALLOC_H_
#define INCLUDE_NWTMALLOC_H_

#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void nwt_init();
extern void nwt_exit();
extern void* nwt_malloc(size_t sz);
extern void nwt_stats();
extern void* nwt_realloc(void* chnk, size_t sz);
extern void* nwt_calloc(size_t sz);
extern void nwt_free(void* chnk);

extern void nwt_purgeCache();
extern void nwt_purgeCacheForce();


#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_NWTMALLOC_H_ */
