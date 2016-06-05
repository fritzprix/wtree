/*
 * wtmalloc.h
 *
 *  Created on: Jun 2, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_WTMALLOC_H_
#define INCLUDE_WTMALLOC_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif



extern void wt_init();
extern void wt_exit();
extern void* wt_malloc(size_t sz);
extern void wt_stats();
extern void* wt_realloc(void* chnk, size_t sz);
extern void* wt_calloc(size_t sz);
extern void wt_print();
extern uint32_t wt_level();
extern void wt_free(void* chnk);
extern void wt_purgeCache();



#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_WTMALLOC_H_ */
