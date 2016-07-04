/*
 * yamalloc.c
 *
 *  Created on: Jul 5, 2016
 *      Author: innocentevil
 */


#include "yamalloc.h"
#include "segment.h"
#include "bestfit.h"
#include "quantum.h"
#include "bin.h"

#include <pthread.h>
#include <stdio.h>

#ifndef YAM_BIN_MAX_SIZE
#define YAM_BIN_MAX_SIZE          (1 << 26)
#endif

#define SEGMENT_SMALL_KEY         ((trkey_t) 1)
#define SEGMENT_LARGE_KEY         ((trkey_t) 2)

typedef struct {
	quantumRoot_t   quantum_alloc;
	bfitRoot_t     *bestfit_alloc;
	segmentRoot_t   segment_cache;
	binCache_t     *bin_cache;
} pt_cache_t;

static DECLARE_ONFREE(bin_unmapper);
static DECLARE_ONALLOCATE(segment_mapper);
static DECLARE_ONFREE(segment_unmapper);
static DECLARE_ONALLOCATE(quantum_mapper);
static DECLARE_ONFREE(quantum_unmapper);
static DECLARE_ONALLOCATE(bfit_mapper);
static DECLARE_ONFREE(bfit_unmapper);

static binRoot_t      bin_root;
static pthread_key_t  yam_key;

static __thread pt_cache_t* ptcache = NULL;

static void yam_bootstrap(void);
static void yam_destroy(void*);


__attribute__((malloc)) void* yam_malloc(size_t sz) {
	if(!ptcache) {
		yam_bootstrap();
	}
	if(QUANTUM_MAX < sz) {
		return bfit_reclaim_chunk(ptcache->bestfit_alloc, sz);
	}
	return quantum_reclaim_chunk(&ptcache->quantum_alloc, sz);
}

__attribute__((malloc)) void* yam_realloc(void* chunk, size_t sz) {
	if(!ptcache) {
		fprintf(stderr, "heap invalid state\n");
		exit(-1);
	}
	void* nchunk;
	if(segment_is_from_cache(&ptcache->segment_cache, SEGMENT_SMALL_KEY, chunk)) {
		size_t osz = quantum_get_chunk_size(&ptcache->quantum_alloc, chunk);
		if(osz >= sz)  return chunk;
		if(sz > QUANTUM_MAX) {
			nchunk = bfit_reclaim_chunk(ptcache->bestfit_alloc, sz);

		} else {
			nchunk = quantum_reclaim_chunk(&ptcache->quantum_alloc, sz);
		}
		memcpy(nchunk, chunk, osz);
		quantum_free_chunk(&ptcache->quantum_alloc, chunk);
		return nchunk;
	}
	return bfit_grows_chunk(ptcache->bestfit_alloc, chunk, sz);
}

__attribute__((malloc)) void* yam_calleoc(size_t sz, size_t cnt) {
	if(!sz || !cnt) return NULL;
}

void yam_free(void* chunk) {

}

void yam_init() {
	bin_root_init(&bin_root, YAM_BIN_MAX_SIZE, bin_unmapper, NULL);
	pthread_key_create(&yam_key, yam_destroy);
}

static void yam_bootstrap(void) {
	pthread_t self = pthread_self();
	pt_cache_t tmp_ptcache;
	// init ptcache in stack variable and assign it to thread local storage variable for bootstrapping
	ptcache = &tmp_ptcache;

	segment_root_init(&tmp_ptcache.segment_cache, ptcache,segment_mapper, segment_unmapper);
	segment_create_cache(&tmp_ptcache.segment_cache, SEGMENT_SMALL_KEY);
	segment_create_cache(&tmp_ptcache.segment_cache, SEGMENT_LARGE_KEY);
	tmp_ptcache.bestfit_alloc = bfit_root_create(&tmp_ptcache, bfit_mapper, bfit_unmapper);

	// allocate ptcache from temporal ptcache
	ptcache = bfit_reclaim_chunk(tmp_ptcache.bestfit_alloc, sizeof(pt_cache_t));

	// copy temporal ptcache into new one
	memcpy(ptcache, &tmp_ptcache, sizeof(pt_cache_t));

	// change ext. context variable to new ptcache
	ptcache->bestfit_alloc->ext_ctx = ptcache;
	ptcache->segment_cache.ext_ctx = ptcache;

	// init quantum allocator
	quantum_root_init(&ptcache->quantum_alloc, quantum_mapper, quantum_unmapper);

	// bind share bin cache
	ptcache->bin_cache = bin_cache_bind(&bin_root, self % BIN_SPREAD_FACTOR);
}

static void yam_destroy(void* chunk) {
	segment_cleanup(&ptcache->segment_cache);
}


static DECLARE_ONFREE(bin_unmapper) {

}

static DECLARE_ONALLOCATE(segment_mapper) {

}

static DECLARE_ONFREE(segment_unmapper) {

}

static DECLARE_ONALLOCATE(quantum_mapper) {

}

static DECLARE_ONFREE(quantum_unmapper) {

}

static DECLARE_ONALLOCATE(bfit_mapper) {

}

static DECLARE_ONFREE(bfit_unmapper) {

}


