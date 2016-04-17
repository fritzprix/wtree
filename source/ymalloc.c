/*
 * ymalloc.c
 *
 *  Created on: Mar 30, 2016
 *      Author: innocentevil
 */


#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "autogen.h"
#include "ymalloc.h"
#include "wtmalloc.h"
#include "cdsl_nrbtree.h"


#ifndef ___YMALLOC_CONFIG
#error "ymalloc has not been configured"
#endif

#ifndef PAGE_SIZE
#warning "ymalloc has not been configured properly"
#define PAGE_SIZE			((size_t) 1 << 12)
#endif

#ifndef  PURGE_THRESHOLD
#warning "ymalloc has not been configured properly"
#define PURGE_THRESHOLD      0x4000000       // 64MB
#endif



struct ymem_thread_cache {
	nrbtreeNode_t ptcache_node;
	pthread_key_t key;
	wt_cache_t    cache;
};


struct ymem_root {
	nrbtreeRoot_t ptcache_root;
	wt_heapRoot_t heap;
	pthread_mutex_t lock;
#define ROOT_INIT              ((uint32_t) 1)
	uint32_t flag;
};

static DECLARE_TRAVERSE_CALLBACK(ymem_for_each);



/*
 *  increase heap size
 *  new heap node created from mmap api
 *  and added to heap root node
 *
 *  thread-safe : false
 */
static int increase_heap(struct ymem_root* heap, size_t sz);
static void cleanup_cache(void*);

/*
 * single static data to track per-thread heap node
 *
 */
static struct ymem_root ROOT = {
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.heap = {0,},
		.ptcache_root = {0,},
		.flag = 0
};

void* ymalloc(size_t sz)
{
	if(!sz)
		return NULL;
	size_t bsz = PAGE_SIZE;
	void* chnk = NULL;
	wt_heapNode_t* heap_node = NULL;
	pthread_t tid = pthread_self();
	struct ymem_thread_cache* ptcache = (struct ymem_thread_cache*) cdsl_nrbtreeLookup(&ROOT.ptcache_root, tid);
	if(!ptcache)
	{
		/*
		 * null ptcache indicates current ymalloc call is initial request from given thread
		 */
		if(pthread_mutex_lock(&ROOT.lock) == EXIT_FAILURE)
		{
			exit(EXIT_FAILURE);
		}
		if(!(ROOT.flag & ROOT_INIT))
		{
			wt_initRoot(&ROOT.heap);
			ROOT.flag |= ROOT_INIT;
		}
		while((ptcache = wt_malloc(&ROOT.heap, sizeof(struct ymem_thread_cache))) == NULL)
		{
			/*
			 *  null ptcache here means heap is not initialized
			 *  add heap node into heap root
			 */
			if(increase_heap(&ROOT, sz) == EXIT_FAILURE)
			{
				exit(EXIT_FAILURE);
			}
		}
		wt_initCache(&ptcache->cache, PURGE_THRESHOLD);
		cdsl_nrbtreeNodeInit(&ptcache->ptcache_node, tid);
		cdsl_nrbtreeInsert(&ROOT.ptcache_root, &ptcache->ptcache_node);

		pthread_key_create(&ptcache->key, cleanup_cache);
		pthread_setspecific(ptcache->key, ptcache);

		chnk = wt_malloc(&ROOT.heap,sz);
		if(pthread_mutex_unlock(&ROOT.lock) == EXIT_FAILURE)
		{
			exit(EXIT_FAILURE);
		}
		return chnk;
	}
	else
	{
		chnk = wt_cacheMalloc(&ptcache->cache,sz);
		if(chnk)
			return chnk;

		if(pthread_mutex_lock(&ROOT.lock) == EXIT_FAILURE){
			exit(EXIT_FAILURE);
		}
		while((chnk = wt_malloc(&ROOT.heap,sz)) == NULL)
		{
			if(increase_heap(&ROOT ,sz) == EXIT_FAILURE)
			{
				exit(EXIT_FAILURE);
			}
		}
		if(pthread_mutex_unlock(&ROOT.lock) == EXIT_FAILURE){
			exit(EXIT_FAILURE);
		}
		return chnk;
	}
}



void yfree(void* ptr)
{
	if(!ptr)
		return;
	pthread_t tid = pthread_self();
	struct ymem_thread_cache* ptcache = (struct ymem_thread_cache*) cdsl_nrbtreeLookup(&ROOT.ptcache_root,tid);
	if(ptcache) {
		ptcache = container_of(ptcache, struct ymem_thread_cache, ptcache_node);
		switch(wt_cacheFree(&ptcache->cache, ptr))
		{
		case WT_OK:
			return;
		case WT_ERROR:
			exit(EXIT_FAILURE);
			break;		// might unreachable
		case WT_FULL:
			break;
		}
	}

	if(pthread_mutex_lock(&ROOT.lock) == EXIT_FAILURE)
	{
		exit(EXIT_FAILURE);
	}
	wt_free(&ROOT.heap,ptr);
	if(pthread_mutex_unlock(&ROOT.lock) == EXIT_FAILURE)
	{
		exit(EXIT_FAILURE);
	}
}

static int increase_heap(struct ymem_root* heap, size_t sz)
{
	if(!heap)
		return EXIT_FAILURE;
	size_t bsz = PAGE_SIZE;
	sz += sizeof(wt_heapNode_t);

	while(bsz < sz) bsz <<= 1;
	wt_heapNode_t* nchunk = mmap(NULL, bsz, PROT_WRITE | PROT_READ,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
	if(!nchunk)
	{
		fprintf(stderr,"system has no avaiable memory !!");
		exit(EXIT_FAILURE);
	}
	wt_initNode(nchunk, &nchunk[1] , bsz - sizeof(wt_heapNode_t));
	wt_addNode(&heap->heap,nchunk);
	return EXIT_SUCCESS;
}

static void cleanup_cache(void* cache)
{
	pthread_t pid = pthread_self();

	struct ymem_thread_cache* _cache = (struct ymem_thread_cache*) cdsl_nrbtreeDelete(&ROOT.ptcache_root, pid);
	if(!cache)
	{
		printf("what the fuck!!\n");
		return;
	}
	pthread_mutex_lock(&ROOT.lock);



	pthread_mutex_unlock(&ROOT.lock);
}



static DECLARE_TRAVERSE_CALLBACK(ymem_for_each)
{
	wt_free(&ROOT.heap, node);
//	wt_cacheFlush(&ROOT.heap, node);
	return 0;
}


