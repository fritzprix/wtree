/*
 * segmapper.h
 *
 *  Created on: Jun 23, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_SEGMENT_H_
#define INCLUDE_SEGMENT_H_

#include <pthread.h>

#include "wtree.h"
#include "cdsl_nrbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	nrbtreeNode_t     rbnode;
	nrbtreeRoot_t     addr_rbroot;
	wtreeRoot_t       seg_cache;
	size_t            total_sz;
	size_t            free_sz;
} segmentCache_t;

typedef struct {
	nrbtreeRoot_t     cache_root;
	wt_map_func_t     mapper;         // underlying mapper
	wt_unmap_func_t   unmapper;       // underlying unmapper
} segmentRoot_t;

extern void segment_root_init(segmentRoot_t* root,wt_map_func_t mapper, wt_unmap_func_t unmapper);
extern void segment_add_cache(segmentRoot_t* root, trkey_t cache_id, segmentCache_t* segment_cache);
extern BOOL segment_is_from_cache(segmentRoot_t* root, trkey_t cache_id, void* addr);
extern void segment_map(segmentRoot_t* root, trkey_t cache_id, size_t sz);
extern void segment_unmap(segmentRoot_t* root, trkey_t cache_id, void* addr, size_t sz);
extern void segment_purge_target_cache(segmentRoot_t* root, trkey_t cache_id);
extern void segment_purge_caches(segmentRoot_t* root);
extern void segmapper_cleanup(segmentRoot_t* root);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_SEGMENT_H_ */
