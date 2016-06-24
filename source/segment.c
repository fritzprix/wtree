/*
 * segmapper.c
 *
 *  Created on: Jun 23, 2016
 *      Author: innocentevil
 */

#include "segment.h"


#define MIN_SEGMENT_SIZE		((size_t) 1 << 24)


typedef struct {
	nrbtreeNode_t  addr_node;
	wtreeNode_t    cache_node;
} segment_t;


static DECLARE_TRAVERSE_CALLBACK(segment_for_each_contains);


void segment_root_init(segmentRoot_t* root,wt_map_func_t mapper, wt_unmap_func_t unmapper) {
	if(!mapper)   return;
	cdsl_nrbtreeRootInit(&root->cache_root);
	root->mapper = mapper;
	root->unmapper = unmapper;
}

void segment_add_cache(segmentRoot_t* root, trkey_t cache_id, segmentCache_t* segment_cache) {
	if(!root || !segment_cache)  return;

	cdsl_nrbtreeRootInit(&segment_cache->addr_rbroot);
	cdsl_nrbtreeNodeInit(&segment_cache->rbnode, cache_id);
	segment_cache->free_sz = segment_cache->total_sz = 0;
	wtree_rootInit(&segment_cache->seg_cache, root->mapper, root->unmapper, NULL, NULL);
}

BOOL segment_is_from_cache(segmentRoot_t* root, trkey_t cache_id, void* addr) {
	if(!root || !addr) return FALSE;

	segmentCache_t* cache = (segmentCache_t*) cdsl_nrbtreeLookup(&root->cache_root,cache_id);

	if(!cache) return FALSE;

	cache = container_of(cache, segmentCache_t, rbnode);
	BOOL found = FALSE;
	cdsl_nrbtreeTraverseTarget(&cache->addr_rbroot, NULL, (trkey_t) addr, &found);
	return found;
}


void segment_map(segmentRoot_t* root, trkey_t cache_id, size_t sz) {

}

void segment_unmap(segmentRoot_t* root, trkey_t cache_id, void* addr, size_t sz) {

}

void segment_purge_target_cache(segmentRoot_t* root, trkey_t cache_id) {

}

void segment_purge_caches(segmentRoot_t* root) {

}

void segmapper_cleanup(segmentRoot_t* root) {

}



static DECLARE_TRAVERSE_CALLBACK(segment_for_each_contains) {

}
