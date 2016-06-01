/*
 * nwtbfit.c
 *
 *  Created on: May 31, 2016
 *      Author: innocentevil
 */


#include "bestfit.h"
#include "cdsl_slist.h"
/*
 * typedef struct {
	nwtreeRoot_t root;
	uint32_t     base_sz; // size of base segment in which cache header itself is contained
	uint32_t     free_cnt;
	size_t total_sz;       // size of total cache
	size_t free_sz;        // size of free cache
	slistEntry_t cleanup_list;
	pthread_t pid;
	int purge_hit_cnt;
} nwt_cache_t;
 */

struct alloc_header {
	uint32_t prev_sz;
	uint32_t cur_sz;
};

struct bf_cache {
	nwtreeRoot_t   cache_root;
	uint32_t       base_sz;
	uint32_t       free_cnt;
	size_t         total_sz;
	size_t         free_sz;
	slistEntry_t   cleanup_list;
};


static int unmap_wrapper(void* addr, size_t sz);

bfCache_t* bfit_bootstrapCache(void* initseg_addr, size_t sz) {
	if(!initseg_addr)
		return NULL;
	struct bf_cache* cache = (struct bf_cache*) initseg_addr;
	cache->base_sz = sz;
	cache->free_sz = cache->total_sz = sz - sizeof(struct bf_cache);
	cache->free_cnt = 0;
	nwtreeNode_t* seg_node = (nwtreeNode_t*) &cache[1];
	nwtree_rootInit(&cache->cache_root, unmap_wrapper);
	nwtree_nodeInit(seg_node, seg_node, cache->free_sz);
	nwtree_addNode(&cache->cache_root, seg_node, TRUE);
	cdsl_slistEntryInit(&cache->cleanup_list);

	return cache;
}

void bfit_addSegment(bfCache_t* cache, void* addr, size_t sz) {
	if(!addr || !sz || !cache)
		return;
	nwtreeNode_t* node = (nwtreeNode_t*) addr;
	nwtree_nodeInit(node, node, sz);
	nwtree_addNode(&((struct bf_cache*) cache)->cache_root, node,TRUE);
}

void* bfit_reclaimChunk(bfCache_t* cache, size_t sz) {
	if(!cache || !sz)
		return NULL;
	if(sz < sizeof(nwtreeNode_t))
		sz = sizeof(nwtreeNode_t);
	sz += sizeof(struct alloc_header);
	uint8_t* chunk = nwtree_reclaim_chunk(&((struct bf_cache*) cache)->cache_root, sz, TRUE);
	((uint32_t*) chunk)[0] = sz - (sizeof(struct alloc_header) >> 1);
	*((uint32_t*) &chunk[sz - (sizeof(struct alloc_header) >> 1)]) = sz - (sizeof(struct alloc_header) >> 1);
	return &((uint32_t*) chunk)[1];
}

void* bfit_growChunk(bfCache_t* cache, void* chunk, size_t sz) {
	if(!cache || !chunk || !sz)
		return NULL;
}

int bfit_freeChunk(bfCache_t* cache, void* chunk) {

}

void bfit_purgeCache(bfCache_t* cache) {

}


static int unmap_wrapper(void* addr, size_t sz) {

}
