/*
 * segmapper.c
 *
 *  Created on: Jun 23, 2016
 *      Author: innocentevil
 */


#include <stdio.h>
#include <stdlib.h>

#include "segment.h"

#ifndef SEGMENT_MIN_SIZE
#define SEGMENT_MIN_SIZE                ((size_t) 1 << 24)
#endif


typedef struct {
	wtreeNode_t    cache_node;
	nrbtreeNode_t  addr_node;
} segment_t;

typedef struct {
	BOOL          found;
	void*         addr;
} contain_arg_t;


static DECLARE_TRAVERSE_CALLBACK(segment_for_each_contains);
static DECLARE_WTREE_ONMERGE(on_segment_merged);


void segment_root_init(segmentRoot_t* root,wt_map_func_t mapper, wt_unmap_func_t unmapper) {
	if(!mapper)   return;
	cdsl_nrbtreeRootInit(&root->cache_root);
	root->mapper = mapper;
	root->unmapper = unmapper;
}

void segment_create_cache(segmentRoot_t* root, trkey_t cache_id) {
	if(!root)  return;

	size_t rsz;
	void* chunk = root->mapper(SEGMENT_MIN_SIZE, &rsz);

	wtreeRoot_t dummy_root;
	wtree_rootInit(&dummy_root, root->mapper, root->unmapper, NULL, NULL, sizeof(segment_t));
	wtreeNode_t* node = wtree_nodeInit(&dummy_root, chunk, rsz);
	segmentCache_t* segment_cache = wtree_reclaim_chunk_from_node(node, sizeof(segmentCache_t));


	cdsl_nrbtreeRootInit(&segment_cache->addr_rbroot);
	cdsl_nrbtreeNodeInit(&segment_cache->rbnode, cache_id);

	segment_cache->free_sz = segment_cache->total_sz = 0;
	wtree_rootInit(&segment_cache->seg_cache, root->mapper, root->unmapper, on_segment_merged,segment_cache, sizeof(segment_t));
	wtree_addNode(&segment_cache->seg_cache, node, TRUE);
	cdsl_nrbtreeInsert(&root->cache_root, &segment_cache->rbnode);
}

BOOL segment_is_from_cache(segmentRoot_t* root, trkey_t cache_id, void* addr) {
	if(!root || !addr) return FALSE;

	segmentCache_t* cache = (segmentCache_t*) cdsl_nrbtreeLookup(&root->cache_root,cache_id);

	if(!cache) return FALSE;

	cache = container_of(cache, segmentCache_t, rbnode);
	contain_arg_t arg;
	arg.found = FALSE;
	arg.addr = addr;
	cdsl_nrbtreeTraverseTarget(&cache->addr_rbroot, segment_for_each_contains, (trkey_t) addr, &arg);
	return arg.found;
}


void* segment_map(segmentRoot_t* root, trkey_t cache_id, size_t sz) {

	if(!root || !sz)  return NULL;

	segmentCache_t* cache = (segmentCache_t*) cdsl_nrbtreeLookup(&root->cache_root, cache_id);
	if(!cache) return NULL;

	cache = container_of(cache, segmentCache_t, rbnode);
	return wtree_reclaim_chunk(&cache->seg_cache, sz, TRUE);
}

void segment_unmap(segmentRoot_t* root, trkey_t cache_id, void* addr, size_t sz) {

	if(!root || !sz) return;

	segmentCache_t* cache = (segmentCache_t*) cdsl_nrbtreeLookup(&root->cache_root, cache_id);
	if(!cache) return;

	cache = container_of(cache, segmentCache_t, rbnode);
	segment_t* segment = (segment_t*) wtree_nodeInit(&cache->seg_cache, addr, sz);
	if(!segment) {
		fprintf(stderr, "unexpected null segment");
		exit(-1);
	}
	segment = container_of(segment, segment_t, cache_node);
	cdsl_nrbtreeNodeInit(&segment->addr_node, (trkey_t) addr);
	cdsl_nrbtreeInsert(&cache->addr_rbroot, &segment->addr_node);
	wtree_addNode(&cache->seg_cache, &segment->cache_node, TRUE);
}

void segment_print_cache(segmentRoot_t* root, trkey_t cache_id) {
	if(!root) return;

	segmentCache_t* cache = (segmentCache_t*) cdsl_nrbtreeLookup(&root->cache_root, cache_id);
	if(!cache) return;

	cache = container_of(cache, segmentCache_t, rbnode);
	wtree_print(&cache->seg_cache);
}


void segment_purge_target_cache(segmentRoot_t* root, trkey_t cache_id) {
	if(!root)  return;

	segmentCache_t* seg_cache = (segmentCache_t*) cdsl_nrbtreeLookup(&root->cache_root, cache_id);
	if(!seg_cache) return;

	seg_cache = container_of(seg_cache, segmentCache_t, rbnode);

}

void segment_purge_caches(segmentRoot_t* root) {

}

void segment_cleanup(segmentRoot_t* root) {

}



static DECLARE_TRAVERSE_CALLBACK(segment_for_each_contains) {
	segment_t* seg_node = container_of(node, segment_t, addr_node);
	contain_arg_t* targ = (contain_arg_t*) arg;
	if((size_t) targ->addr > (size_t) seg_node->cache_node.top)
		return TRAVERSE_OK;
	if((size_t) targ->addr < (size_t) (seg_node->cache_node.top - seg_node->cache_node.base_size))
		return TRAVERSE_OK;
	targ->found = TRUE;
	return TRAVERSE_OK;
}

static DECLARE_WTREE_ONMERGE(on_segment_merged) {
	printf("merged!!!\n");
	segmentCache_t* cache = (segmentCache_t*) arg;
	segment_t* segment = container_of(mergee, segment_t, cache_node);
	cdsl_nrbtreeDelete(&cache->addr_rbroot, segment->addr_node.key);
}

