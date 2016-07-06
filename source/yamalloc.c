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
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>

#ifndef YAM_BIN_MAX_SIZE
#define YAM_BIN_MAX_SIZE          (1 << 26)
#endif

#define SEGMENT_SMALL_KEY         ((trkey_t) 1)
#define SEGMENT_LARGE_KEY         ((trkey_t) 2)

#define SEGMENT_UNIT_SIZE        (1 << 24)
#define BFIT_UNIT_SIZE           (1 << 21)
#define Q_UNIT_SIZE              (1 << 20)

typedef struct {
	quantumRoot_t   quantum_alloc;
	bfitRoot_t     *bestfit_alloc;
	segmentRoot_t  *segment_cache;
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
static __thread segmentRoot_t segroot;

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
	if(segment_is_from_cache(ptcache->segment_cache, SEGMENT_SMALL_KEY, chunk)) {
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

__attribute__((malloc)) void* yam_calloc(size_t sz, size_t cnt) {
	if(!sz || !cnt) return NULL;
	if(!ptcache) {
		yam_bootstrap();
	}
	size_t tsz = sz * cnt;
	void* chunk;
	if(QUANTUM_MAX < tsz) {
		return memset(bfit_reclaim_chunk(ptcache->bestfit_alloc, tsz),0,tsz);
	}
	return memset(quantum_reclaim_chunk(&ptcache->quantum_alloc, tsz),0,tsz);
}

void yam_free(void* chunk) {
	if(!chunk) return;
	if(!ptcache) {
		fprintf(stderr, "Invalid State : Per thread cache should be initialized first\n");
		exit(-1);
	}
	if(segment_is_from_cache(ptcache->segment_cache, SEGMENT_SMALL_KEY, chunk)){
		quantum_free_chunk(&ptcache->quantum_alloc, chunk);
	} else {
		bfit_free_chunk(ptcache->bestfit_alloc, chunk);
	}
}

void yam_init() {
	bin_root_init(&bin_root, YAM_BIN_MAX_SIZE, bin_unmapper, NULL);
	pthread_key_create(&yam_key, yam_destroy);
}

static void yam_bootstrap(void) {
	pthread_t self = pthread_self();
	// init ptcache in stack variable and assign it to thread local storage variable for bootstrapping
	bfitRoot_t* bfitroot;
	pt_cache_t tmpcache;
	segment_root_init(&segroot, ptcache,segment_mapper, segment_unmapper);
	segment_create_cache(&segroot, SEGMENT_SMALL_KEY);
	segment_create_cache(&segroot, SEGMENT_LARGE_KEY);
	tmpcache.segment_cache = &segroot;
	ptcache = &tmpcache;

	bfitroot = bfit_root_create(&segroot, bfit_mapper, bfit_unmapper);

	// allocate ptcache from temporal ptcache
	ptcache = bfit_reclaim_chunk(bfitroot, sizeof(pt_cache_t));

	// copy temporal ptcache into new one
	ptcache->segment_cache = &segroot;
	ptcache->bestfit_alloc = bfitroot;

	// init quantum allocator
	quantum_root_init(&ptcache->quantum_alloc, quantum_mapper, quantum_unmapper);

	// bind share bin cache
	ptcache->bin_cache = bin_cache_bind(&bin_root, self % BIN_SPREAD_FACTOR);
}

static void yam_destroy(void* chunk) {
	segment_cleanup(ptcache->segment_cache);
}


static DECLARE_ONFREE(bin_unmapper) {
	return munmap(addr, sz);
}

static DECLARE_ONALLOCATE(segment_mapper) {
	if(!total_sz) return NULL;
//	void* chunk = bin_root_recycle(&bin_root,ptcache->bin_cache, total_sz);
//	if(chunk) return chunk;
	size_t seg_sz = SEGMENT_UNIT_SIZE;
	while(seg_sz < total_sz) seg_sz <<= 1;

	*rsz = seg_sz;
	return mmap(NULL, seg_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NONBLOCK, -1, 0);
}

static DECLARE_ONFREE(segment_unmapper) {
	if(!addr) return -1;
//	bin_root_dispose(&bin_root, ptcache->bin_cache, addr, sz);
	return munmap(addr, sz);
	return 0;
}

static DECLARE_ONALLOCATE(quantum_mapper) {
	if(!total_sz) return NULL;
	size_t q_sz = Q_UNIT_SIZE;
	while(q_sz < total_sz) q_sz <<= 1;
	*rsz = q_sz;
	return segment_map(ptcache->segment_cache, SEGMENT_SMALL_KEY, q_sz);
}

static DECLARE_ONFREE(quantum_unmapper) {
	if(!addr) return -1;
	segment_unmap(ptcache->segment_cache, SEGMENT_SMALL_KEY, addr, sz);
	return 0;
}

static DECLARE_ONALLOCATE(bfit_mapper) {
	if(!total_sz) return NULL;
	size_t bf_sz = BFIT_UNIT_SIZE;
	while(bf_sz < total_sz) bf_sz <<= 1;
	*rsz = bf_sz;
	return segment_map(ptcache->segment_cache, SEGMENT_LARGE_KEY, bf_sz);
}

static DECLARE_ONFREE(bfit_unmapper) {
	if(!addr) return -1;
	segment_unmap(ptcache->segment_cache, SEGMENT_LARGE_KEY, addr, sz);
	return 0;
}


