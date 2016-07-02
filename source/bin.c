/*
 * bin.c
 *
 *  Created on: Jul 2, 2016
 *      Author: innocentevil
 */

#include <pthread.h>

#include "bin.h"

static DECLARE_ONFREE(bin_internal_unmapper);
static pthread_mutex_t gl_lock = PTHREAD_MUTEX_INITIALIZER;

static wt_adapter cache_adapter = {
		.onallocate = NULL,
		.onremoved = bin_internal_unmapper,
		.onadded = NULL,
		.onremoved = NULL
};


void bin_root_init(binRoot_t* bin, size_t max_sz, wt_unmap_func_t unmapper) {
	if(!bin || !unmapper || !max_sz) return;

	pthread_mutex_lock(&gl_lock);
	if(bin->cache_max_sz) {
		goto UNLOCK_RETURN;
	}
	bin->unmapper = unmapper;
	bin->cache_max_sz = max_sz;
	int idx = 0;
	for(;idx < BIN_SPREAD_FACTOR;idx++) {
		pthread_mutex_init(&bin->caches[idx].lock,NULL);
		wtree_rootInit(&bin->caches[idx].bin_cache, &bin->caches[idx], &cache_adapter, 0);
		bin->caches[idx].ref_cnt = 0;
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
	}
	pthread_mutex_unlock(&cache->lock);
}

void bin_root_dispose(binRoot_t* bin, binCache_t* cache, void* addr, size_t sz){

}

void* bin_root_recycle(binRoot_t* bin, binCache_t* cache, size_t sz) {

}

void bin_purge(binRoot_t* bin) {

}

void bin_cleanup(binRoot_t* bin) {

}

static DECLARE_ONFREE(bin_internal_unmapper) {

}
