/*
 * wtalloc.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */

#include "wtree.h"
#include "wtalloc.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


#ifndef container_of
#define container_of(ptr,type,member) 		 (((size_t) ptr - (size_t) offsetof(type,member)))
#endif

#define NULL_CACHE 					&null_node

struct exter_header {
	size_t psize;				///			prev chunk			///
/// ===========================================================	///
	size_t size;				///         current chunk
/// ===========================================================	///
};
struct heapHeader {
	size_t psize;				///			prev chunk			///
/// ===========================================================	///
	size_t size;				///         current chunk
/// ===========================================================	///
	wtreeNode_t wtree_node;		///			free cache header 	///
};



static void * cache_malloc(wt_alloc_t* alloc,size_t sz);
static wt_alloc_t* cache_free(wt_alloc_t* alloc,void* ptr);
static size_t cache_size(wt_alloc_t* alloc);
static void cache_print(wt_alloc_t* alloc);

static wt_alloc_t* rotateLeft(wt_alloc_t* rot_pivot);
static wt_alloc_t* rotateRight(wt_alloc_t* rot_pivot);
static wt_alloc_t* add_cache_r(wt_alloc_t* current,wt_alloc_t* nu);
static wt_alloc_t* free_cache_r(wt_alloc_t* current,BOOL* freed);
static wt_alloc_t* free_r(wt_alloc_t* current,void* ptr,wt_alloc_t** purgeable);
static void print_tab(int k);
static void print_r(wt_alloc_t* current,int k);

/*
 *     p
 *    / \
 *   n0 n1
 */


/*
 * 	wt_alloc_t *left,*right;
	void*	 	base;
	void* 		pos;
	void* 		limit;
	size_t 		size;
	wtreeRoot_t	entry;
 */
static wt_alloc_t null_node = {
		.__node = {0,0},
		.pos = NULL,
		.limit = NULL,
		.size = 0,
		.entry = {0}
};

void wtreeHeap_initCacheRoot(wt_heaproot_t* root){
	if(!root)
		return;
	root->cache = NULL_CACHE;
	root->size = 0;
}

void wtreeHeap_initCacheNode(wt_alloc_t* alloc,void* addr,size_t sz){
	alloc->base = alloc->pos = (uint8_t*) addr;
	alloc->limit = (void*) ((size_t) addr + sz);
	alloc->size = sz;
	alloc->left = alloc->right = NULL_CACHE;
	wtreeRootInit(&alloc->entry,sizeof(struct exter_header));
}


void wtreeHeap_addCache(wt_heaproot_t* heap,wt_alloc_t* cache){
	if(!heap || !cache)
		return;
	heap->cache = add_cache_r(heap->cache,cache);
	heap->size += cache->size;
}

BOOL wtreeHeap_removeCache(wt_heaproot_t* heap, wt_alloc_t* cache){
	if(!heap || !cache)
		return FALSE;

	return TRUE;
}

void* wtreeHeap_malloc(wt_heaproot_t* heap,size_t sz){
	if(!heap || !sz)
		return NULL;
	void* chunk_ptr = cache_malloc(heap->cache,sz);
	if((heap->cache->left->size > heap->cache->size) || (heap->cache->right->size > heap->cache->size)){
		if(heap->cache->left->size > heap->cache->right->size){
			heap->cache = rotateRight(heap->cache);
		}else{
			heap->cache = rotateLeft(heap->cache);
		}
	}
	return chunk_ptr;
}

void wtreeHeap_free(wt_heaproot_t* heap,void* ptr,wt_alloc_t** purgeable){
	if(!heap || !ptr)
		return;
	if(heap->cache == NULL_CACHE)
		exit(-1);
	wt_alloc_t* _purge = NULL;
	if(!purgeable)
		purgeable = &_purge;

	heap->cache = free_r(heap->cache,ptr,purgeable);
	if(*purgeable == NULL_CACHE)
	{
		*purgeable = NULL;
	}
}


size_t wtreeHeap_size(wt_heaproot_t* heap){
	if(!heap)
		return 0;
	return heap->size;
}

void wtreeHeap_print(wt_heaproot_t* heap){
	if(!heap)
		return;
	print_r(heap->cache,0);
}


static wt_alloc_t* add_cache_r(wt_alloc_t* current,wt_alloc_t* nu){
	if(current == NULL_CACHE)
		return nu;
	if(current->base < nu->base){
		current->right = add_cache_r(current->right,nu);
		if(current->size < current->right->size)
			return rotateLeft(current);
	}else{
		current->left = add_cache_r(current->left,nu);
		if(current->size < current->left->size)
			return rotateRight(current);
	}
	return current;
}

static wt_alloc_t* free_cache_r(wt_alloc_t* current,BOOL* freed){
	return NULL;
}



/**
 *
 */
static wt_alloc_t* free_r(wt_alloc_t* current,void* ptr, wt_alloc_t** purgeable){
	if(!purgeable)
	{
		return current;
	}
	if(current->base > ptr){
		current->left = free_r(current->left,ptr,purgeable);
		if(current->left->size > current->size)
			return rotateRight(current);
	}else if((current->right->base > ptr) || (current->right == NULL_CACHE)){
		*purgeable = (wt_alloc_t*) cache_free(current,ptr);
	}else {
		current->right = free_r(current->right,ptr,purgeable);
		if(current->right->size > current->size)
			return rotateLeft(current);
	}
	return current;
}

static void print_tab(int k){
	while(k--)	printf("\t");
}


static void print_r(wt_alloc_t* current,int k){
	if(current == NULL_CACHE)
		return;
	print_r(current->left,k + 1);
	print_tab(k);printf("{cache : base %d , size %ld @ %d}\n",(int)current->base,current->size,k);
	print_r(current->right,k + 1);
}




static wt_alloc_t* rotateLeft(wt_alloc_t* rot_pivot){
	wt_alloc_t* nparent = rot_pivot->right;
	rot_pivot->right = nparent->left;
	nparent->left = rot_pivot;
	return nparent;
}

static wt_alloc_t* rotateRight(wt_alloc_t* rot_pivot){
	wt_alloc_t* nparent = rot_pivot->left;
	rot_pivot->left = nparent->right;
	nparent->right = rot_pivot;
	return nparent;
}

static void * cache_malloc(wt_alloc_t* alloc,size_t sz){
	if(!sz)
		return NULL;
	wtreeNode_t* chunk = wtreeRetrive(&alloc->entry,&sz);
	struct heapHeader *chdr,*nhdr,*nnhdr;
	uint64_t tsz;
	if(chunk == NULL){
		if(alloc->pos + sz + sizeof(struct heapHeader) >= alloc->limit)
			return NULL; // impossible to handle
		chdr = (struct heapHeader*) alloc->pos;
		alloc->pos = (void*) ((size_t) &chdr->wtree_node + sz);
		nhdr = (struct heapHeader*) alloc->pos;
		chdr->size = sz;
		nhdr->psize = sz;
	}else{
		chdr = (struct heapHeader *) container_of(chunk,struct heapHeader,wtree_node);  //?
		nnhdr = (struct heapHeader *) ((size_t) &chdr->wtree_node + sz);
		chdr->size = sz;
		nnhdr->psize = sz;
	}
	alloc->size -= sz;
	return &chdr->wtree_node;
}

static wt_alloc_t* cache_free(wt_alloc_t* alloc,void* ptr){
	if(!ptr)
		return NULL_CACHE;
	struct heapHeader* chdr,*nhdr;
	chdr = (struct heapHeader*) container_of(ptr,struct heapHeader,wtree_node);
	nhdr = (struct heapHeader*) ((size_t)ptr + chdr->size);
	if(chdr->size != nhdr->psize)
	{
		fprintf(stderr,"Heap Corrupted\n");
		exit(1);
	}
	wtreeNodeInit(&chdr->wtree_node,(uint64_t)&chdr->wtree_node,chdr->size);
	wtreeInsert(&alloc->entry,&chdr->wtree_node);
	alloc->size += chdr->size;
	if(alloc->size == ((size_t)alloc->limit - (size_t) alloc->base))
	{
		return alloc;
	}
	return NULL_CACHE;
}

static size_t cache_size(wt_alloc_t* alloc){
	return wtreeTotalSpan(&alloc->entry);
}


static void cache_print(wt_alloc_t* alloc){
	printf("======================================================================================================\n");
	wtreePrint(&alloc->entry);
	printf("======================================================================================================\n");
}


