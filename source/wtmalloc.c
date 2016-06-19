/*
 * wtmalloc.c
 *
 *  Created on: Jun 2, 2016
 *      Author: innocentevil
 */


#include "wtmalloc.h"
#include "wtree.h"
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
	wtreeRoot_t root;
	uint32_t     base_sz; // size of base segment in which cache header itself is contained
	uint32_t     free_cnt;
	size_t total_sz;       // size of total cache
	size_t free_sz;        // size of free cache
	slistEntry_t cleanup_list;
	pthread_t pid;
	int purge_hit_cnt;
} wt_cache_t;

struct chunk_header {
	uint32_t prev_sz;
	uint32_t cur_sz;
};

typedef struct {
	slistNode_t lhead;
	wtreeNode_t node;
} cleanup_list_t;

static void wt_cache_dstr(void* cache);
static DECLARE_WTREE_TRAVERSE_CALLBACK(oncleanup);
static void* map_wrapper(size_t sz, size_t* rsz);
static int unmap_wrapper(void* addr, size_t sz);
static wt_cache_t* wt_cache_bootstrap(size_t init_sz);

static __thread wt_cache_t* ptcache = NULL;

void wt_init() {
	pthread_key_create(&cache_key, wt_cache_dstr);
}

void wt_exit() {
	pthread_key_delete(cache_key);
}

/*
 *  ****************************************************************************************************
 *  |  prev_sz  | cur_sz |  current usable memory area | prev_sz | next_sz | next usable memory area ....
 *  ****************************************************************************************************
 */
__attribute__((malloc)) void* wt_malloc(size_t sz) {
	if (!sz)
		return NULL;
	if (sz < sizeof(wtreeNode_t))
		sz = sizeof(wtreeNode_t);
	sz += sizeof(struct chunk_header); // need additional size to put chunk header for keeping track of chunk size
	sz = (sz + 8) & ~7;
	size_t seg_sz = SEGMENT_SIZE;
	uint8_t* chnk;
	if (!ptcache) {
		sz += sizeof(wt_cache_t); // to bootstrap cache out of thin air, size of cache header should be considered
		ptcache = wt_cache_bootstrap(sz);
		pthread_setspecific(cache_key, ptcache);
	}
	chnk = wtree_reclaim_chunk(&ptcache->root, sz, TRUE);
	ptcache->free_sz -= sz;
	*((uint32_t*) chnk) = sz - sizeof(uint32_t); // set current chunk size before the usable memory area
	*((uint32_t*) &chnk[sz - sizeof(uint32_t)]) = sz - sizeof(uint32_t); // set prev_chunk size at the prev_sz field in next chunk header
	ptcache->free_cnt = 0;
	return &chnk[sizeof(uint32_t)];
}

__attribute__((malloc)) void* wt_realloc(void* chnk, size_t sz) {
	if (!chnk || !sz)
		return NULL;
	if (sz < sizeof(wtreeNode_t))
		sz = sizeof(wtreeNode_t);
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	void* nchnk;
	wtreeNode_t* node;
	uint32_t *sz_chk, *cur_sz;
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
	if ((*cur_sz - sizeof(uint32_t)) >= sz) {
		/*
		 * if new requested size is not greater than original
		 * just return original chunk
		 */
		return chnk;
	}

	wtreeNode_t header_presv;
	memcpy(&header_presv, cur_sz, sizeof(wtreeNode_t));
	node = wtree_nodeInit(cur_sz, *cur_sz + sizeof(uint32_t));
	nchnk = wtree_grow_chunk(&ptcache->root, &node, sz);
	if(!node) {
		memcpy(nchnk + sizeof(wtreeNode_t), (node->top - node->size + sizeof(wtreeNode_t)),node->size - sizeof(wtreeNode_t));
	}
	memcpy(nchnk, &header_presv, sizeof(wtreeNode_t));
	ptcache->free_sz -= (sz - node->size);
	*((uint32_t*) nchnk) = sz - sizeof(uint32_t);
	*((uint32_t*) nchnk) = sz - sizeof(uint32_t);
	*((uint32_t*) &nchnk[sz - sizeof(uint32_t)]) = sz - sizeof(uint32_t); // set prev_chunk size at the prev_sz field in next chunk header
	ptcache->free_cnt = 0;
	return &((uint32_t*) nchnk)[1];
}

__attribute__((malloc)) void* wt_calloc(size_t sz, size_t cnt) {
	if(!sz || !cnt)
		return NULL;
	size_t rsz = sz * cnt;
	void* chnk = wt_malloc(rsz);
	if (!chnk)
		return NULL;
	memset(chnk, 0, rsz);
	return chnk;
}

extern void wt_print() {
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	wtree_print(&ptcache->root);
}

uint32_t wt_level() {
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	return wtree_level(&ptcache->root);
}

void wt_free(void* chnk) {
	if (!chnk)
		return;
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	wtreeNode_t* node;
	uint8_t* chk = ((uint8_t*) chnk) - sizeof(uint32_t);
	uint32_t* chnk_sz = ((uint32_t*) chk);
	if (*((uint32_t*) &chk[*chnk_sz]) != *chnk_sz) {
		fprintf(stderr, "Heap corrupted\n");
		exit(-1);
	}
	ptcache->free_sz += (*chnk_sz + sizeof(uint32_t));
	ptcache->free_cnt += (*chnk_sz + sizeof(uint32_t));
	node = wtree_nodeInit(chk, *chnk_sz + sizeof(uint32_t));
	wtree_addNode(&ptcache->root, node, TRUE);
	if(ptcache->free_cnt > ((ptcache->total_sz * 1016) >> 10)) {
		ptcache->purge_hit_cnt++;
		if(ptcache->purge_hit_cnt > ((1 << 8)) ) {
			wtree_purge(&ptcache->root);
			ptcache->purge_hit_cnt = 0;
			ptcache->free_cnt = 0;
		}
	}

}

void wt_purgeCache() {
	if (!ptcache) {
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	wtree_purge(&ptcache->root);
}

static wt_cache_t* wt_cache_bootstrap(size_t init_sz) {
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
	wt_cache_t* cache = (wt_cache_t*) &chnk[seg_sz - sizeof(wt_cache_t)];
	cache->pid = pthread_self();
	cache->base_sz = seg_sz;
	cache->free_sz = cache->total_sz = seg_sz - sizeof(wt_cache_t);
	cache->free_cnt = 0;
	wtreeNode_t* seg_node;
	wtree_rootInit(&cache->root, map_wrapper, unmap_wrapper);
	cache->purge_hit_cnt = 0;
	seg_node = wtree_nodeInit(chnk, cache->free_sz);
	wtree_addNode(&cache->root, seg_node, TRUE);
	cdsl_slistEntryInit(&cache->cleanup_list);

	return cache;
}

static void wt_cache_dstr(void* cache) {
	if (!cache)
		return;
	wt_cache_t* cachep = (wt_cache_t*) cache;
	cleanup_list_t* lh;

	wtree_traverseBaseNode(&cachep->root, oncleanup, cachep);
	while (!cdsl_slistIsEmpty(&cachep->cleanup_list)) {
		lh = (cleanup_list_t*) cdsl_slistDequeue(&cachep->cleanup_list);
		if (!lh)
			exit(-1);
		lh = container_of(lh, cleanup_list_t, lhead);
		cachep->total_sz -= lh->node.base_size;
		cachep->free_sz -= lh->node.size;
		munmap((void*) (lh->node.top - lh->node.base_size), lh->node.base_size);
	}
	if ((cachep->total_sz + sizeof(wt_cache_t)) != cachep->base_sz) {
		printf("some seg lost : %u / %lu\n", cachep->base_sz,
				cachep->total_sz);
	}
	cache = (void*) ((size_t)( cache + sizeof(wt_cache_t)) - cachep->base_sz);
	munmap(cache, cachep->base_sz);
}

static DECLARE_WTREE_TRAVERSE_CALLBACK(oncleanup) {
	wt_cache_t* cache = (wt_cache_t*) arg;
	cleanup_list_t* clhead = (cleanup_list_t*) ((uint8_t*)node - offsetof(cleanup_list_t, node));
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

static void* map_wrapper(size_t sz, size_t* rsz) {
	size_t seg_sz = SEGMENT_SIZE;
	uint8_t* chnk;
	while(seg_sz < sz) seg_sz <<= 1;
	chnk = mmap(NULL, seg_sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(!chnk) {
		fprintf(stderr, "Out Of Memory");
		exit(-1);
	}
	if(rsz)
		*rsz = seg_sz;
	ptcache->total_sz += seg_sz;
	ptcache->free_sz += seg_sz;
	return chnk;
}




