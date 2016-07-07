/*
 * bin.c
 *
 *  Created on: Jul 2, 2016
 *      Author: innocentevil
 */

#include <pthread.h>

#include "bin.h"


#ifndef BIN_PURGE_THRESHOLD
#define BIN_PURGE_THRESHOLD            (1 << 26)
#endif

static DECLARE_ONFREE(bin_internal_unmapper);
static pthread_mutex_t gl_lock = PTHREAD_MUTEX_INITIALIZER;

static wt_adapter cache_adapter = {
		.onallocate = NULL,
		.onfree = bin_internal_unmapper,
		.onadded = NULL,
		.onremoved = NULL
};


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
		wtree_rootInit(&bin->caches[idx].bin_cache, &bin->caches[idx], &cache_adapter, 0);
		bin->caches[idx].ref_cnt = 0;
		bin->caches[idx].root = bin;
	}

UNLOCK_RETURN:
	pthread_mutex_unlock(&gl_lock);
}

binCache_t* bin_cache_bind(binRoot_t* bin, trkey_t key) {
	if(!bin) return NULL;
	binCache_t* cache = &bin->caches[key % BIN_SPREAD_FACTOR];
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
		wtree_cleanup(&cache->bin_cache);
		wtree_rootInit(&cache->bin_cache, cache, &cache_adapter, 0);
	}
	pthread_mutex_unlock(&cache->lock);
}

void bin_root_dispose(binRoot_t* bin, binCache_t* cache, void* addr, size_t sz){
	if(!bin || !cache || !addr || !sz) return;

	wtreeNode_t* cached_chunk = wtree_baseNodeInit(&cache->bin_cache, addr, sz);
	pthread_mutex_lock(&cache->lock);
	if(!cache->ref_cnt) {
		/**
		 * cache is unbound state
		 */
		bin_internal_unmapper(addr, sz, cached_chunk, cache);
		pthread_mutex_unlock(&cache->lock);
		return;
	}
	if(wtree_totalSize(&cache->bin_cache) > BIN_PURGE_THRESHOLD) {
		wtree_purge(&cache->bin_cache);
	}
	wtree_addNode(&cache->bin_cache, cached_chunk, TRUE);
	pthread_mutex_unlock(&cache->lock);
}

void* bin_root_recycle(binRoot_t* bin, binCache_t* cache, size_t sz) {
	if(!bin || !cache || !sz) return NULL;

	void* chunk;
	pthread_mutex_lock(&cache->lock);
	chunk = wtree_reclaim_chunk(&cache->bin_cache, sz, TRUE);
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

static DECLARE_ONFREE(bin_internal_unmapper) {
	if(!addr) return FALSE;
	binCache_t* cache = (binCache_t*) ext_ctx;
	cache->root->unmapper(addr,sz,wtnode, cache->root->ext_ctx);
	return TRUE;
}
