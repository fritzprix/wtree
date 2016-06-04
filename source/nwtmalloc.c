/*
 * nwtmalloc.c
 *
 *  Created on: May 8, 2016
 *      Author: innocentevil
 */

#include "nwtmalloc.h"
#include "nwtree.h"
#include "cdsl_slist.h"
#include "cdsl_nrbtree.h"

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>

#ifndef SEGMENT_SIZE
#define SEGMENT_SIZE            ((size_t) 1 << 21)
#endif



static pthread_key_t cache_key;
typedef struct {
	nwtreeRoot_t root;
	uint32_t     base_sz; // size of base segment in which cache header itself is contained
	uint32_t     free_cnt;
	size_t total_sz;       // size of total cache
	size_t free_sz;        // size of free cache
	slistEntry_t cleanup_list;
	pthread_t pid;
	int purge_hit_cnt;
} nwt_cache_t;

struct chunk_header {
	uint32_t prev_sz;
	uint32_t cur_sz;
};

typedef struct {
	nwtreeNode_t node;
	slistNode_t lhead;
} cleanup_list_t;

static void nwt_cache_dstr(void* cache);
static DECLARE_PURGE_CALLBACK(oncleanup);
static int unmap_wrapper(void* addr, size_t sz);
static nwt_cache_t* nwt_cache_bootstrap(size_t init_sz);

static __thread nwt_cache_t* ptcache = NULL;

void nwt_init() {
	pthread_key_create(&cache_key, nwt_cache_dstr);
}

void nwt_exit() {
	pthread_key_delete(cache_key);
}

/*
 *  ****************************************************************************************************
 *  |  prev_sz  | cur_sz |  current usable memory area | prev_sz | next_sz | next usable memory area ....
 *  ****************************************************************************************************
 */
void* nwt_malloc(size_t sz) {
	if (!sz)
		return NULL;
	if (sz < sizeof(nwtreeNode_t))
		sz = sizeof(nwtreeNode_t);
	sz += sizeof(struct chunk_header); // need additional size to put chunk header for keeping track of chunk size
	size_t seg_sz = SEGMENT_SIZE;
	uint8_t* chnk;
	if (!ptcache) {
		sz += sizeof(nwt_cache_t); // to bootstrap cache out of thin air, size of cache header should be considered
		ptcache = nwt_cache_bootstrap(sz);
		pthread_setspecific(cache_key, ptcache);
	}
	while (!(chnk = nwtree_reclaim_chunk(&ptcache->root, sz, TRUE))) {
		while (seg_sz < sz)
			seg_sz <<= 1; // calc min seg size , which is multiple of SEGMENT_SIZE, larger than requested sz
		chnk = mmap(NULL, seg_sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (!chnk) {
			perror("memory depleted\n");
			exit(-1);
		}
		nwtree_baseNodeInit((nwtreeNode_t*) chnk, chnk, seg_sz);
		nwtree_addNode(&ptcache->root, (nwtreeNode_t*) chnk,TRUE);
		ptcache->total_sz += seg_sz;
		ptcache->free_sz += seg_sz;
	}
	ptcache->free_sz -= sz;
	*((uint32_t*) chnk) = sz - sizeof(uint32_t); // set current chunk size before the usable memory area
	*((uint32_t*) &chnk[sz - sizeof(uint32_t)]) = sz - sizeof(uint32_t); // set prev_chunk size at the prev_sz field in next chunk header
	ptcache->free_cnt = 0;
	return &chnk[sizeof(uint32_t)];
}

void* nwt_realloc(void* chnk, size_t sz) {
	if (!chnk || !sz)
		return NULL;
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}

	uint8_t* nchnk;
	uint32_t *sz_chk, *cur_sz;

	if (sz < sizeof(nwtreeNode_t))
		sz = sizeof(nwtreeNode_t);
	sz += sizeof(struct chunk_header);

	cur_sz = ((uint32_t*) chnk - 1);
	sz_chk = (uint32_t*) &(((uint8_t*) cur_sz)[*cur_sz]);

	if (*cur_sz != *sz_chk) {
		/*
		 * if current size in chunk header is not identical to prev size in at the tail
		 * header is assumed to be corrupted
		 */
		fprintf(stderr, "Heap corrupted %u : %u\n", *cur_sz, *sz_chk);
		exit(-1);
	}
	size_t osz = *cur_sz;
	if ((*cur_sz + sizeof(uint32_t)) >= sz) {
		/*
		 * if new requested size is not greater than original
		 * just return original chunk
		 */
		return chnk;
	}

	nwtreeNode_t preserved;
	nwtreeNode_t* grows = (nwtreeNode_t*) cur_sz;

	memcpy(&preserved, grows, sizeof(nwtreeNode_t));
	nwtree_nodeInit((nwtreeNode_t*) grows, grows, *cur_sz + sizeof(uint32_t));
	nchnk = nwtree_grow_chunk(&ptcache->root, &grows,sz);
	if(!grows) {
		// fail to grow chunk
		memcpy(nchnk + sizeof(nwtreeNode_t),((uint8_t*) cur_sz + sizeof(nwtreeNode_t)), osz - sizeof(nwtreeNode_t));
	}
	memcpy(nchnk, &preserved, sizeof(nwtreeNode_t));
	ptcache->free_sz -= (sz - osz);
	*((uint32_t*) nchnk) = sz - sizeof(uint32_t);
	*((uint32_t*) &nchnk[sz - sizeof(uint32_t)]) = sz - sizeof(uint32_t); // set prev_chunk size at the prev_sz field in next chunk header
	ptcache->free_cnt = 0;
	return ((uint32_t*)nchnk + 1);
}

void* nwt_calloc(size_t sz) {
	void* chnk = nwt_malloc(sz);
	if (!chnk)
		return NULL;
	memset(chnk, 0, sz);
	return chnk;
}

extern void nwt_print() {
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	nwtree_print(&ptcache->root);
}

uint32_t nwt_level() {
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	return nwtree_level(&ptcache->root);
}

void nwt_free(void* chnk) {
	if (!chnk)
		return;
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	uint8_t* chk = ((uint8_t*) chnk) - sizeof(uint32_t);
	uint32_t* chnk_sz = ((uint32_t*) chk);
	if (*((uint32_t*) &chk[*chnk_sz]) != *chnk_sz) {
		fprintf(stderr, "Heap corrupted\n");
		exit(-1);
	}
	ptcache->free_sz += (*chnk_sz + sizeof(uint32_t));
	ptcache->free_cnt += (*chnk_sz + sizeof(uint32_t));
	nwtree_nodeInit((nwtreeNode_t*) chk, chk, *chnk_sz + sizeof(uint32_t));
	nwtree_addNode(&ptcache->root, (nwtreeNode_t*) chk, TRUE);
//	if(ptcache->free_sz > ((ptcache->total_sz * 15) >> 4)) {
	if(ptcache->free_cnt > ((ptcache->total_sz * 1016) >> 10)) {
		ptcache->purge_hit_cnt++;
		if(ptcache->purge_hit_cnt > ((1 << 8)) ) {
			nwtree_purge(&ptcache->root);
			ptcache->purge_hit_cnt = 0;
			ptcache->free_cnt = 0;
		}
	}

}

void nwt_purgeCache() {
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	nwtree_purge(&ptcache->root);
}

static nwt_cache_t* nwt_cache_bootstrap(size_t init_sz) {
	size_t seg_sz = SEGMENT_SIZE;
	pthread_t pid = pthread_self();
	while (seg_sz < init_sz)
		seg_sz <<= 1;	// grow seg_sz unless it is greater than requested size
	uint8_t* chnk = mmap(NULL, seg_sz, PROT_WRITE | PROT_READ,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_NONBLOCK, -1, 0);
	if (!chnk) {
		fprintf(stderr, "System memory depleted!\n");
		exit(-1);
	}
	nwt_cache_t* cache = (nwt_cache_t*) chnk;
	cache->pid = pthread_self();
	cache->base_sz = seg_sz;
	cache->free_sz = cache->total_sz = seg_sz - sizeof(nwt_cache_t);
	cache->free_cnt = 0;
	nwtreeNode_t* seg_node = (nwtreeNode_t*) &cache[1];
	nwtree_rootInit(&cache->root, unmap_wrapper);
	cache->purge_hit_cnt = 0;
	nwtree_nodeInit((nwtreeNode_t*) seg_node, seg_node, cache->free_sz);
	nwtree_addNode(&cache->root, seg_node, TRUE);
	cdsl_slistEntryInit(&cache->cleanup_list);

	return cache;
}

static void nwt_cache_dstr(void* cache) {
	if (!cache)
		return;
	nwt_cache_t* cachep = (nwt_cache_t*) cache;
	cleanup_list_t* lh;
	nwtree_iterBaseNode(&cachep->root, oncleanup, cachep);
	while (!cdsl_slistIsEmpty(&cachep->cleanup_list)) {
		lh = (cleanup_list_t*) cdsl_slistDequeue(&cachep->cleanup_list);
		if (!lh)
			exit(-1);
		lh = container_of(lh, cleanup_list_t, lhead);
		cachep->total_sz -= lh->node.base_size;
		cachep->free_sz -= lh->node.size;
		munmap((void*) lh, lh->node.base_size);
	}
	if ((cachep->total_sz + sizeof(nwt_cache_t)) != cachep->base_sz) {
		printf("some seg lost : %u / %lu\n", cachep->base_sz,
				cachep->total_sz);
	}
	munmap(cachep, cachep->base_sz);
}

static DECLARE_PURGE_CALLBACK(oncleanup) {
	nwt_cache_t* cache = (nwt_cache_t*) arg;
	cleanup_list_t* clhead = (cleanup_list_t*) node;
	cache->purge_hit_cnt++;
	cdsl_slistNodeInit(&clhead->lhead);
	cdsl_slistPutHead(&cache->cleanup_list, &clhead->lhead);
	return TRUE;
}

static int unmap_wrapper(void* addr, size_t sz) {
	ptcache->total_sz -= sz;
	ptcache->free_sz -= sz;
	munmap(addr,sz);
	return 0;
}




