/*
 * nwtbfit.h
 *
 *  Created on: May 31, 2016
 *      Author: innocentevil
 */



#ifndef INCLUDE_BESTFIT_H_
#define INCLUDE_BESTFIT_H_


#include "nwtree.h"
#include "cdsl_slist.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void bfCache_t;


extern bfCache_t* bfit_bootstrapCache(void* initseg_addr, size_t sz);
extern void bfit_addSegment(bfCache_t* cache, void* addr, size_t sz);
extern void* bfit_reclaimChunk(bfCache_t* cache, size_t sz);
extern void* bfit_growChunk(bfCache_t* cache, void* chunk, size_t sz);
extern int bfit_freeChunk(bfCache_t* cache, void* chunk);
extern void bfit_purgeCache(bfCache_t* cache);



#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_BESTFIT_H_ */
