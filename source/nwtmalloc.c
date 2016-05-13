/*
 * nwtmalloc.c
 *
 *  Created on: May 8, 2016
 *      Author: innocentevil
 */


#include "nwtmalloc.h"
#include "nwtree.h"

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>

#ifndef SEGMENT_SIZE
#define SEGMENT_SIZE            ((size_t) 1 << 20)
#endif

static pthread_key_t cache_key;
typedef struct {
	nwtreeRoot_t    root;
	size_t          total_sz;
	size_t          free_sz;
} nwt_cache_t;

struct chunk_header {
	size_t prev_sz;
	size_t cur_sz;
};

static void nwt_cache_dstr(void* cache);
static DECLARE_PURGE_CALLBACK(onpurge);
static nwt_cache_t* nwt_cache_bootstrap(size_t init_sz);

void nwt_init()
{
	pthread_key_create(&cache_key, nwt_cache_dstr);
}


void nwt_exit()
{
	pthread_key_delete(cache_key);
}


/*
 *  ****************************************************************************************************
 *  |  prev_sz  | cur_sz |  current usable memory area | prev_sz | next_sz | next usable memory area ....
 *  ****************************************************************************************************
 */
void* nwt_malloc(size_t sz)
{
	if(!sz)
		return NULL;
	if(sz < sizeof(nwtreeNode_t))
		sz = sizeof(nwtreeNode_t);
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	sz += sizeof(struct chunk_header);                     // need additional size to put chunk header for keeping track of chunk size
	size_t seg_sz = SEGMENT_SIZE;
	uint8_t* chnk;
	if(!cache) {
		sz += sizeof(nwt_cache_t);                         // to bootstrap cache out of thin air, size of cache header should be considered
		cache = nwt_cache_bootstrap(sz);
		pthread_setspecific(cache_key, cache);
	}

	while(!(chnk = nwtree_reclaim_chunk(&cache->root,sz)))
	{
		while(seg_sz < sz) seg_sz <<= 1;                     // calc min seg size , which is multiple of SEGMENT_SIZE, larger than requested sz
		chnk = mmap(NULL, seg_sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(!chnk)
		{
			perror("memory depleted\n");
			exit(-1);
		}
		nwtree_baseNodeInit((nwtreeNode_t*) chnk, chnk, seg_sz);
		nwtree_addNode(&cache->root,(nwtreeNode_t*) chnk);
	}

	*((size_t*) chnk) = sz - sizeof(size_t);                        // set current chunk size before the usable memory area
	*((size_t*) &chnk[sz - sizeof(size_t)]) = sz - sizeof(size_t);   // set prev_chunk size at the prev_sz field in next chunk header
	return &chnk[sizeof(size_t)];
}

void* nwt_realloc(void* chnk, size_t sz)
{
	if(!chnk || !sz)
		return NULL;
	if(sz < sizeof(nwtreeNode_t))
		sz = sizeof(nwtreeNode_t);
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	if(!cache)
	{
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	void* nchnk;
	size_t *sz_chk, *cur_sz;
	cur_sz = ((size_t*) chnk - 1);
	sz_chk = (size_t*) &(((uint8_t*) cur_sz)[*cur_sz]);
	if(*cur_sz != *sz_chk)
	{
		/*
		 * if current size in chunk header is not identical to prev size in at the tail
		 * header is assumed to be corrupted
		 */
		fprintf(stderr, "Heap corrupted %lu : %lu\n",*cur_sz, *sz_chk);
		exit(-1);
	}
	if((*cur_sz - sizeof(size_t)) >= sz)
	{
		/*
		 * if new requested size is not greater than original
		 * just return original chunk
		 */
		return chnk;
	}

	nchnk = nwt_malloc(sz);
	memcpy(nchnk, chnk, *cur_sz);
	nwtree_nodeInit((nwtreeNode_t*) cur_sz, cur_sz, *cur_sz + sizeof(size_t));
	nwtree_addNode(&cache->root, (nwtreeNode_t*)  cur_sz);
	return nchnk;
}

void* nwt_calloc(size_t sz)
{
	void* chnk = nwt_malloc(sz);
	if(!chnk)
		return NULL;
	memset(chnk,0,sz);
	return chnk;
}

extern void nwt_print()
{
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	if(!cache)
	{
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	nwtree_print(&cache->root);
}


void nwt_free(void* chnk)
{
	if(!chnk)
		return;
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	if(!cache)
	{
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	uint8_t* chk = ((uint8_t*) chnk) - sizeof(size_t);
	size_t* chnk_sz = ((size_t*) chk);
	if(*((size_t*) &chk[*chnk_sz]) != *chnk_sz)
	{
		fprintf(stderr, "Heap corrupted\n");
		exit(-1);
	}
	nwtree_nodeInit((nwtreeNode_t*) chk, chk, *chnk_sz + sizeof(size_t));
	nwtree_addNode(&cache->root, (nwtreeNode_t*) chk);
}

void nwt_purgeCache()
{
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	if(!cache)
	{
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	nwtree_purge(&cache->root,onpurge);
}

void nwt_purgeCacheForce()
{
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	if(!cache)
	{
		fprintf(stderr, "Heap uninitialized\n");
		exit(-1);
	}
	nwt_cache_dstr(cache);
	cache = nwt_cache_bootstrap(SEGMENT_SIZE);
	pthread_setspecific(cache_key, cache);
}

static nwt_cache_t* nwt_cache_bootstrap(size_t init_sz)
{
	size_t seg_sz = SEGMENT_SIZE;
	while(seg_sz < init_sz) seg_sz <<= 1;		// grow seg_sz unless it is greater than requested size
	size_t cache_hdr_offset = seg_sz - sizeof(nwt_cache_t);
	uint8_t* chnk = mmap(NULL, seg_sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
	if(!chnk)
	{
		fprintf(stderr, "System memory depleted!\n");
		exit(-1);
	}
	nwt_cache_t* cache = (nwt_cache_t*) &chnk[cache_hdr_offset];
	cache->free_sz = cache_hdr_offset;
	cache->total_sz = cache_hdr_offset;
	nwtree_rootInit(&cache->root);
	nwtree_nodeInit((nwtreeNode_t*) chnk, chnk, cache_hdr_offset);
	nwtree_addNode(&cache->root, (nwtreeNode_t*) chnk);
	return cache;
}



static void nwt_cache_dstr(void* cache)
{
	if(!cache)
		return;
	nwt_cache_t* cachep = (nwt_cache_t*) cache;
	nwtree_purge(&cachep->root, onpurge);
}



static DECLARE_PURGE_CALLBACK(onpurge)
{
	munmap(node->base,node->base_size);
	return TRUE;
}
