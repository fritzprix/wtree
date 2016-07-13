/*
 * bin.c
 *
 *  Created on: Jul 2, 2016
 *      Author: innocentevil
 */

#include <pthread.h>
#include <stdlib.h>
#include "bin.h"


#ifndef BIN_PURGE_THRESHOLD
#define BIN_PURGE_THRESHOLD            (1 << 26)
#endif

static void bin_print(void*);
static DECLARE_ONFREE(bin_internal_unmapper);
static DECLARE_ONADDED(bin_internal_on_node_added);
static DECLARE_ONREMOVED(bin_internal_on_node_removoed);
static DECLARE_TRAVERSE_CALLBACK(bin_is_chunk_contained);
static pthread_mutex_t gl_lock = PTHREAD_MUTEX_INITIALIZER;

static wt_adapter cache_adapter = {
		.onallocate = NULL,
		.onfree = bin_internal_unmapper,
		.onadded = bin_internal_on_node_added,
		.onremoved = bin_internal_on_node_removoed
};
typedef struct {
	BOOL       contained;
	void*      addr;
}bin_contain_t;

typedef struct {
	wtreeNode_t     cache_node;
	nrbtreeNode_t   addr_node;
}bin_cacheNode_t;


void bin_root_init(binRoot_t* bin, size_t max_sz, wt_unmap_func_t unmapper, void* ext_ctx) {
	if(!bin || !unmapper || !max_sz) return;

	pthread_mutex_lock(&gl_lock);
	if(bin->cache_max_sz) {
		goto UNLOCK_RETURN;
	}
	bin->ext_ctx = ext_ctx;
	bin->unmapper = unmapper;
	bin->cache_max_sz = max_sz;
	int idx = 0;
	for(;idx < BIN_SPREAD_FACTOR;idx++) {
		pthread_mutex_init(&bin->caches[idx].lock,NULL);
		wtree_rootInit(&bin->caches[idx].bin_cache, &bin->caches[idx], &cache_adapter, sizeof(bin_cacheNode_t));
		cdsl_nrbtreeRootInit(&bin->caches[idx].addr_root);
		bin->caches[idx].ref_cnt = 0;
		bin->caches[idx].root = bin;
		bin->caches[idx].total_sz = bin->caches[idx].free_sz = 0;
		bin->caches[idx].idx = idx;
	}

UNLOCK_RETURN:
	pthread_mutex_unlock(&gl_lock);
}

binCache_t* bin_cache_bind(binRoot_t* bin, trkey_t key) {
	if(!bin) return NULL;
	int idx = key % BIN_SPREAD_FACTOR;
	binCache_t* cache = &bin->caches[idx];
	pthread_mutex_lock(&cache->lock);
	cache->ref_cnt++;
	pthread_mutex_unlock(&cache->lock);
	return cache;
}

void bin_cache_unbind(binRoot_t* bin, binCache_t* cache) {
	if(!bin || !cache) return;
	pthread_mutex_lock(&cache->lock);
	cache->ref_cnt--;
	if(!cache->ref_cnt) {
		printf("CLEAN UP BEGIN -------\n");
		printf("NOW SIZE (%zu / %zu) @ %d\n",cache->free_sz, cache->total_sz, cache->idx);
		wtree_print(&cache->bin_cache);
		wtree_cleanup(&cache->bin_cache);
//		wtree_purge(&cache->bin_cache);
		wtree_rootInit(&cache->bin_cache, cache, &cache_adapter, sizeof(bin_cacheNode_t));
		cdsl_nrbtreeRootInit(&cache->addr_root);
		printf("CLEAN UP END   -------\n");
		printf("NOW SIZE (%zu / %zu) @ %d\n",cache->free_sz, cache->total_sz, cache->idx);
		cache->free_sz = cache->total_sz = 0;
	}
	pthread_mutex_unlock(&cache->lock);
}

void bin_root_dispose(binRoot_t* bin, binCache_t* cache, void* addr, size_t sz){
	if(!bin || !cache || !addr || !sz) return;

	bin_contain_t contained_check;
	contained_check.addr = addr;
	contained_check.contained = FALSE;

	wtreeNode_t* cache_chunk;
	bin_cacheNode_t *cache_node;
	pthread_t self = pthread_self();
	if(!cache->ref_cnt) {
		printf("WTF\n this is not bound cache\n");
		exit(-1);
	}

	pthread_mutex_lock(&cache->lock);

	cdsl_nrbtreeTraverseTarget(&cache->addr_root, bin_is_chunk_contained, (trkey_t) addr, &contained_check);
	cache->free_sz += sz;
	if(contained_check.contained) {
		if(cache->total_sz > BIN_PURGE_THRESHOLD) {
		//		bin->unmapper(addr, sz, NULL, bin->ext_ctx);
		//		wtree_print(&cache->bin_cache);
		//		printf("PURGE BEGIN @ %d by %lu----------------\n",cache->idx,self);
				wtree_purge(&cache->bin_cache);
		//		printf("PURGE END   @ %d by %lu----------------\n",cache->idx,self);
		//		pthread_mutex_unlock(&cache->lock);
		//		return;
		}
		cache_chunk = wtree_nodeInit(&cache->bin_cache, addr, sz, NULL);
//		printf("(+) PARTIAL CHUNK %zu @ %d\n",sz, cache->idx);
//		printf("NOW SIZE (%zu / %zu) @ %d\n",cache->free_sz, cache->total_sz, cache->idx);
	} else {
		if(cache->total_sz > BIN_PURGE_THRESHOLD) {
				bin->unmapper(addr, sz, NULL, bin->ext_ctx);
		//		wtree_print(&cache->bin_cache);
		//		printf("PURGE BEGIN @ %d by %lu----------------\n",cache->idx,self);
//				wtree_purge(&cache->bin_cache);
		//		printf("PURGE END   @ %d by %lu----------------\n",cache->idx,self);
				pthread_mutex_unlock(&cache->lock);
				return;
		}
		cache_chunk = wtree_baseNodeInit(&cache->bin_cache, addr, sz);
		cache->total_sz += sz;
//		printf("(+) NEW SEGMENT %zu @ %d\n", sz, cache->idx);
//		printf("NOW SIZE (%zu / %zu) @ %d\n",cache->free_sz, cache->total_sz, cache->idx);
	}
	size_t tsz;
	wtree_addNode(&cache->bin_cache, cache_chunk, TRUE);
	pthread_mutex_unlock(&cache->lock);
}

void* bin_root_recycle(binRoot_t* bin, binCache_t* cache, size_t sz) {
	if(!bin || !cache || !sz) return NULL;

	void* chunk;
	pthread_mutex_lock(&cache->lock);
	if(!cache->ref_cnt) {
		printf("WTF\n this is not bound cache\n");
		exit(-1);
	}
	chunk = wtree_reclaim_chunk(&cache->bin_cache, sz, TRUE);
	if(chunk) {
		cache->free_sz -= sz;
//		printf("NOW SIZE (%zu / %zu) @ %d\n",cache->free_sz, cache->total_sz, cache->idx);
//		printf("(-) RECYCLE CHUNK %zu @ %d\n",sz,cache->idx);
	}
	pthread_mutex_unlock(&cache->lock);
	return chunk;
}

void bin_purge(binRoot_t* bin) {
	if(!bin) return;

	int i = 0;
	for(;i < BIN_SPREAD_FACTOR; i++) {
		pthread_mutex_lock(&bin->caches[i].lock);
		wtree_purge(&bin->caches[i].bin_cache);
		pthread_mutex_unlock(&bin->caches[i].lock);
	}
}

void bin_cleanup(binRoot_t* bin) {
	if(!bin) return;
	int i = 0;
	for(;i < BIN_SPREAD_FACTOR; i++) {
		pthread_mutex_lock(&bin->caches[i].lock);
		wtree_cleanup(&bin->caches[i].bin_cache);
		pthread_mutex_unlock(&bin->caches[i].lock);
	}
}

static DECLARE_TRAVERSE_CALLBACK(bin_is_chunk_contained) {
	bin_contain_t* contain_arg = (bin_contain_t*) arg;
	if(!node) {
		contain_arg->contained = FALSE;
		return TRAVERSE_BREAK;
	}

	bin_cacheNode_t* cache = container_of(node, bin_cacheNode_t, addr_node);
	if(((size_t) cache->cache_node.top - cache->cache_node.base_size) > (size_t)contain_arg->addr) {
		return TRAVERSE_OK;
	}
	if((size_t) cache->cache_node.top <= (size_t) contain_arg->addr) {
		return TRAVERSE_OK;
	}
	contain_arg->contained = TRUE;
	return TRAVERSE_BREAK;
}

static void bin_print(void* node) {
	bin_cacheNode_t* bcache = container_of(node, bin_cacheNode_t, addr_node);
	printf("node : %u @%lx\n", bcache->cache_node.base_size, (size_t) bcache->cache_node.top - bcache->cache_node.base_size);
}


static DECLARE_ONFREE(bin_internal_unmapper) {
	if(!addr) return FALSE;
	binCache_t* cache = (binCache_t*) ext_ctx;
	pthread_t self = pthread_self();
	cache->total_sz -= sz;
	cache->free_sz -= sz;
//	printf("UNMAPPED %zu @ %d by %lu\n",sz,cache->idx,self);
	cache->root->unmapper(addr,sz,wtnode, cache->root->ext_ctx);
	return TRUE;
}

static DECLARE_ONADDED(bin_internal_on_node_added) {
	binCache_t* cache = (binCache_t*) ext_ctx;
	bin_cacheNode_t* cache_node = container_of(node, bin_cacheNode_t, cache_node);
	cdsl_nrbtreeNodeInit(&cache_node->addr_node, (trkey_t) node->top - node->base_size);

	if(node->base_size) {
		cdsl_nrbtreeInsert(&cache->addr_root, &cache_node->addr_node);
	}
}
static DECLARE_ONREMOVED(bin_internal_on_node_removoed) {
	binCache_t* cache = (binCache_t*) ext_ctx;
	bin_cacheNode_t* cache_node = container_of(node, bin_cacheNode_t, cache_node);
	if(!node->base_size)
		return;
	if(!cdsl_nrbtreeDelete(&cache->addr_root, cache_node->addr_node.key)) {
		fprintf(stderr, "unexpected null %lx\n",cache_node->addr_node.key);
//		exit(-1);
	}
}
