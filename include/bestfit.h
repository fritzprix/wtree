/*
 * bestfit.h
 *
 *  Created on: Jun 30, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_BESTFIT_H_
#define INCLUDE_BESTFIT_H_

#include "wtree.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	wtreeRoot_t     bfit_cache;
	wt_map_func_t   mapper;
	wt_unmap_func_t unmapper;
	size_t          total_sz;
	size_t          free_sz;
	void*           ext_ctx;
} bfitRoot_t;

extern void bfit_root_init(bfitRoot_t* root, void* ext_ctx, wt_map_func_t mapper, wt_unmap_func_t unmapper);
extern void* bfit_reclaim_chunk(bfitRoot_t* root, size_t sz);
extern void* bfit_grows_chunk(bfitRoot_t* root, void* chunk, size_t nsz);
extern void bfit_free_chunk(bfitRoot_t* root, void* chunk);
extern void bfit_purge_cache(bfitRoot_t* root);
extern void bfit_clean_up(bfitRoot_t* root);


#ifdef __cplusplus
}
#endif



#endif /* INCLUDE_BESTFIT_H_ */
