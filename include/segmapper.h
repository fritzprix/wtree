/*
 * segmapper.h
 *
 *  Created on: Jun 23, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_SEGMAPPER_H_
#define INCLUDE_SEGMAPPER_H_

#include "wtree.h"
#include "cdsl_nrbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	nrbtreeNode_t     rbnode;
	nrbtreeRoot_t     addr_rbroot;
	wtreeRoot_t       seg_cache;
	pthread_mutex_t   cache_lock;
	size_t            total_sz;
	size_t            free_sz;
} segmentCache_t;

typedef struct {
	nrbtreeRoot_t     cache_root;
	wt_map_func_t     mapper;         // underlying mapper
	wt_unmap_func_t   unmapper;       // underlying unmapper
} segmentMapper_t;

extern void segmapper_init(segmentMapper_t* mapper);
extern void segmapper_add_cache(segmentMapper_t* mapper, trkey_t cache_id, segmentCache_t* segment_cache);
extern BOOL segmapper_is_from_cache(segmentMapper_t* mapper, trkey_t cache_id, void* addr);
extern void segmapper_map(trkey_t cache_id, size_t sz);
extern void segmapper_unmap(trkey_t cache_id, void* addr, size_t sz);
extern void segmapper_purge_target_cache(segmentMapper_t* mapper, trkey_t cache_id);
extern void segmapper_purge_caches(segmentMapper_t* mapper);
extern void segmapper_cleanup(segmentMapper_t* mapper);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_SEGMAPPER_H_ */
