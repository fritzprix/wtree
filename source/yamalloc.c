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

#define SEGMENT_UNIT_SIZE         (1 << 24)
#define BFIT_UNIT_SIZE            (1 << 21)
#define Q_UNIT_SIZE               (1 << 20)

typedef struct {
	quantumRoot_t   quantum_alloc;
	segmentRoot_t   segment_cache;
	bfitRoot_t     *bestfit_alloc;
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

static __thread pt_cache_t pt_cache = {0,};

static void yam_bootstrap(void);
static void yam_destroy(void*);


void* yam_malloc(size_t sz) {
	if(!pt_cache.bestfit_alloc) {
		yam_bootstrap();
	}
	if(QUANTUM_MAX < sz) {
		return bfit_reclaim_chunk(pt_cache.bestfit_alloc, sz);
	}
	return quantum_reclaim_chunk(&pt_cache.quantum_alloc, sz);
}

void* yam_realloc(void* chunk, size_t sz) {
	if(!pt_cache.bestfit_alloc) {
		fprintf(stderr, "heap invalid state\n");
		exit(-1);
	}
	void* nchunk;
	if(segment_is_from_cache(&pt_cache.segment_cache, SEGMENT_SMALL_KEY, chunk)) {
		size_t osz = quantum_get_chunk_size(&pt_cache.quantum_alloc, chunk);
		if(osz >= sz)
			return chunk;
		if(sz > QUANTUM_MAX) {
			nchunk = bfit_reclaim_chunk(pt_cache.bestfit_alloc, sz);
		} else {
			nchunk = quantum_reclaim_chunk(&pt_cache.quantum_alloc, sz);
		}
		memcpy(nchunk, chunk, osz);
		quantum_free_chunk(&pt_cache.quantum_alloc, chunk);
		return nchunk;
	}
	return bfit_grows_chunk(pt_cache.bestfit_alloc, chunk, sz);
}

void* yam_calloc(size_t sz, size_t cnt) {
	if(!sz || !cnt) return NULL;
	if(!pt_cache.bestfit_alloc) {
		yam_bootstrap();
	}
	size_t tsz = sz * cnt;
	void* chunk;
	if(QUANTUM_MAX < tsz) {
		return memset(bfit_reclaim_chunk(pt_cache.bestfit_alloc, tsz),0,tsz);
	}
	return memset(quantum_reclaim_chunk(&pt_cache.quantum_alloc, tsz),0,tsz);
}

void* yam_memalign(size_t alignment, size_t sz) {
	if(!sz || !alignment) return NULL;
	if(!pt_cache.bestfit_alloc) {
		yam_bootstrap();
	}
	return bfit_reclaim_aligned_chunk(pt_cache.bestfit_alloc, sz, alignment);
}


void yam_free(void* chunk) {
	if(!chunk) return;
	if(!pt_cache.bestfit_alloc) {
		fprintf(stderr, "Invalid State : Per thread cache should be initialized first\n");
		exit(-1);
	}
	if(segment_is_from_cache(&pt_cache.segment_cache, SEGMENT_SMALL_KEY, chunk)){
		quantum_free_chunk(&pt_cache.quantum_alloc, chunk);
	} else {
		bfit_free_chunk(pt_cache.bestfit_alloc, chunk);
	}
}

__attribute__((constructor)) void yam_init() {
	bin_root_init(&bin_root, YAM_BIN_MAX_SIZE, bin_unmapper, NULL);
	pthread_key_create(&yam_key, yam_destroy);
}

__attribute__((destructor)) void yam_exit() {
	pthread_key_delete(yam_key);
}


static void yam_bootstrap(void) {
	pthread_t self = pthread_self();
	// init ptcache in stack variable and assign it to thread local storage variable for bootstrapping
//	pt_cache.bin_cache = bin_cache_bind(&bin_root, self);

	segment_root_init(&pt_cache.segment_cache, &pt_cache,segment_mapper, segment_unmapper);
	segment_create_cache(&pt_cache.segment_cache, SEGMENT_SMALL_KEY);
	segment_create_cache(&pt_cache.segment_cache, SEGMENT_LARGE_KEY);

	pt_cache.bestfit_alloc = bfit_root_create(&pt_cache.segment_cache, bfit_mapper, bfit_unmapper);
	// init quantum allocator
	quantum_root_init(&pt_cache.quantum_alloc, quantum_mapper, quantum_unmapper);
	// bind share bin cache
	pthread_setspecific(yam_key, &pt_cache);
}

static void yam_destroy(void* chunk) {
	binCache_t* bin_cache = pt_cache.bin_cache;
	segment_cleanup(&pt_cache.segment_cache);
//	bin_cache_unbind(&bin_root, bin_cache);
}


static DECLARE_ONFREE(bin_unmapper) {
	return munmap(addr, sz);
}

static DECLARE_ONALLOCATE(segment_mapper) {
	if(!total_sz) return NULL;

	size_t seg_sz = SEGMENT_UNIT_SIZE;
	while(seg_sz < total_sz) seg_sz <<= 1;
	*rsz = seg_sz;

//	void* chunk = bin_root_recycle(&bin_root, pt_cache.bin_cache, total_sz);
//	if(chunk){
//		return chunk;
//	}

	return mmap(NULL, seg_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NONBLOCK, -1, 0);
}

static DECLARE_ONFREE(segment_unmapper) {
	if(!addr) return -1;
//	bin_root_dispose(&bin_root, pt_cache.bin_cache, addr, sz);
//	return 0;
	return munmap(addr, sz);
}

static DECLARE_ONALLOCATE(quantum_mapper) {
	if(!total_sz) return NULL;
	size_t q_sz = Q_UNIT_SIZE;
	while(q_sz < total_sz) q_sz <<= 1;
	*rsz = q_sz;
	return segment_map(&pt_cache.segment_cache, SEGMENT_SMALL_KEY, q_sz);
}

static DECLARE_ONFREE(quantum_unmapper) {
	if(!addr) return -1;
	segment_unmap(&pt_cache.segment_cache, SEGMENT_SMALL_KEY, addr, sz);
	return 0;
}

static DECLARE_ONALLOCATE(bfit_mapper) {
	if(!total_sz) return NULL;
	size_t bf_sz = BFIT_UNIT_SIZE;
	while(bf_sz < total_sz) bf_sz <<= 1;
	*rsz = bf_sz;
	return segment_map(&pt_cache.segment_cache, SEGMENT_LARGE_KEY, bf_sz);
}

static DECLARE_ONFREE(bfit_unmapper) {
	if(!addr) return -1;
	segment_unmap(&pt_cache.segment_cache, SEGMENT_LARGE_KEY, addr, sz);
	return 0;
}


