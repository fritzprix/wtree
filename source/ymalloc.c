/*
 * ymalloc.c
 *
 *  Created on: Mar 30, 2016
 *      Author: innocentevil
 */


#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "autogen.h"
#include "ymalloc.h"
#include "wtalloc.h"
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



struct ymem_cb {
	nrbtreeNode_t node;
	wt_alloc_t	  base_cache;
	pthread_key_t key;
	wt_heaproot_t heap_root;
	size_t		  total_sz;
	size_t		  free_sz;
};



struct ymem_root {
	pthread_mutex_t lock;
	nrbtreeRoot_t __root;
};

static DECLARE_TRAVERSE_CALLBACK(ymem_for_each);


static void destroy_heap(void* heap);
static struct ymem_cb* create_heap_node(struct ymem_root* rootp, size_t sz);
static int increase_heap_node(struct ymem_cb* cb, size_t sz);
static void shrink_heap_node(struct ymem_cb* cb, wt_alloc_t* cache);


/*
 * single static data to track per-thread heap node
 *
 */
static struct ymem_root ROOT = {
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.__root = {0,}
};


void* ymalloc(size_t sz)
{
	void* chnk = NULL;
	pthread_t tid = pthread_self();
	struct ymem_cb* wt_heap = (struct ymem_cb*) cdsl_nrbtreeLookup(&ROOT.__root,tid);
	if(!wt_heap){
		/*
		 * there is no per-thread heap root
		 * initialize per-thread heap
		 */
		wt_heap = create_heap_node(&ROOT,sz);
		/*
		 * add new heap node into static single node
		 */
		cdsl_nrbtreeNodeInit(&wt_heap->node, tid);
		cdsl_nrbtreeInsert(&ROOT.__root, &wt_heap->node);
	}else{
		wt_heap = container_of(wt_heap, struct ymem_cb, node);
	}
	chnk = wtreeHeap_malloc(&wt_heap->heap_root,sz);
	if(!(chnk = wtreeHeap_malloc(&wt_heap->heap_root,sz)))
	{
		/*
		 * try to increase heap
		 */
		if(increase_heap_node(wt_heap,sz) == EXIT_FAILURE)
		{
			/*
			 *  fail to increase heap
			 */
			exit(EXIT_FAILURE);
		}
	}
	return chnk;
}

void yfree(void* ptr)
{
	if(!ptr)
		return;
	pthread_t tid = pthread_self();
	struct ymem_cb* wt_heap = (struct ymem_cb* ) cdsl_nrbtreeLookup(&ROOT.__root,tid);

	if (!wt_heap) {
		/*
		 * error : heap never been created or raise exception
		 */
		return;
	}

	wt_heap = container_of(wt_heap, struct ymem_cb, node);
	wt_alloc_t* purgeable = NULL;
	wtreeHeap_free(&wt_heap->heap_root,ptr,&purgeable);
	if((purgeable != NULL) && (purgeable != &wt_heap->base_cache )) {
		/**
		 *  release cache to system
		 */
		if(wt_heap->total_sz > PURGE_THRESHOLD)
		{
			shrink_heap_node(wt_heap, purgeable);
		}
	}
}

static struct ymem_cb* create_heap_node(struct ymem_root* rootp, size_t sz)
{
	size_t bsz = PAGE_SIZE;
	struct ymem_cb* heap_node;
	sz += sizeof(heap_node);
	while(bsz < sz) bsz <<= 1;
	heap_node = mmap(NULL,bsz, (PROT_READ|PROT_WRITE),(MAP_PRIVATE | MAP_ANONYMOUS),-1,0);
	if(!heap_node)
	{
		exit(EXIT_FAILURE);
	}

	/*
	 * register destructor for per-thread heap
	 */
	pthread_key_create(&heap_node->key,destroy_heap);
	pthread_setspecific(heap_node->key, heap_node);

	wtreeHeap_initCacheRoot(&heap_node->heap_root);
	wtreeHeap_initCacheNode(&heap_node->base_cache,&heap_node[1],bsz - sizeof(heap_node));
	heap_node->free_sz = heap_node->total_sz = bsz - sizeof(heap_node);
	wtreeHeap_addCache(&heap_node->heap_root, &heap_node->base_cache);
	cdsl_nrbtreeNodeInit(&heap_node->node,heap_node->total_sz);
	return heap_node;
}

static int increase_heap_node(struct ymem_cb* cb, size_t sz)
{
	if(!cb)
		return EXIT_FAILURE;

	sz += sizeof(wt_alloc_t);
	size_t bsz = PAGE_SIZE;
	while(bsz < sz) bsz <<= 1;
	wt_alloc_t* cache_node = mmap(NULL,bsz, (PROT_READ|PROT_WRITE),(MAP_PRIVATE | MAP_ANONYMOUS),-1,0);
	if(!cache_node)
	{
		exit(EXIT_FAILURE);
	}
	wtreeHeap_initCacheNode(cache_node, &cache_node[1], bsz - sizeof(cache_node));
	wtreeHeap_addCache(&cb->heap_root, cache_node);
	return TRUE;
}

static void shrink_heap_node(struct ymem_cb* cb, wt_alloc_t* cache)
{
	if(!cb || !cache)
		return;
}

static void destroy_heap(void* heap)
{
	if(!heap)
		return;

	struct ymem_cb* cb = (struct ymem_cb*) heap;
	pthread_key_delete(cb->key);
	// unmap all cached segment
	wtreeTraverse(&cb->heap_root,ymem_for_each,ORDER_INC);
	// remove heap node from global ROOT
	cdsl_nrbtreeDelete(&ROOT.__root, cb->node.key);
	// finally unmap heap base segment
	munmap((void*) cb, cb->total_sz + sizeof(cb));

}

static DECLARE_TRAVERSE_CALLBACK(ymem_for_each)
{
	// TODO : fix traverse callback to access root of tree
	return 0;
}


