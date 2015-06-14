/*
 * malloc.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */

#include "wtree.h"
#include <stddef.h>

static uint8_t* heapMem;
static uint8_t* heapPos;
static uint8_t* heapLimit;
static size_t heapSize;
static wtreeRoot_t cache_root;


#ifndef container_of
#define container_of(ptr,type,member) 		 (((size_t) ptr - (size_t) offsetof(type,member)))
#endif
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

void wtreeHeap_init(void* addr,size_t sz){
	heapMem = (uint8_t*) addr;
	heapPos = heapMem;
	heapLimit = heapMem + sz;
	heapSize = sz;
	wtreeRootInit(&cache_root,sizeof(struct exter_header));
}

void * wtreeHeap_malloc(size_t sz){
	wtreeNode_t* chunk = wtreeRetrive(&cache_root,sz);
	struct heapHeader *chdr,*nhdr,*nnhdr;
	uint64_t tsz;
	if(chunk == NULL){
		if(heapPos + sz >= heapLimit)
			return NULL; // impossible to handle
		chdr = (struct heapHeader*) heapPos;
		heapPos = (size_t) &chdr->wtree_node + sz;
		nhdr = (struct heapHeader*) heapPos;
		chdr->size = sz;
		nhdr->psize = sz;
		return &chdr->wtree_node;
	}else{
		chdr = container_of(chunk,struct heapHeader,wtree_node);
		nnhdr = (struct heapHeader *) ((size_t) &chdr->wtree_node + chdr->size);
		if(chdr->size != sz){
			// update size
			tsz = chdr->size;
			chdr->size = sz;
			nhdr = ((uint8_t*) chdr + chdr->size);
			nhdr->size = tsz - chdr->size;
			nnhdr->psize = nhdr->size;
			nhdr->psize = chdr->size;
			return &chdr->wtree_node;
		}else {
			if(nnhdr->psize != chdr->size)
				error(-1,0,"Heap Corrupted\n");
			return &chdr->wtree_node;
		}
	}
}

void wtreeHeap_free(void* ptr){
	if(!ptr)
		return;
	struct heapHeader* chdr,*nhdr;
	chdr = container_of(ptr,struct heapHeader,wtree_node);
	nhdr = (struct heapHeader*) ((size_t)ptr + chdr->size);
	if(chdr->size != nhdr->psize)
		error(-1,0,"Heap Corrupted\n");
	wtreeNodeInit(&chdr->wtree_node,&chdr->wtree_node,chdr->size);
	wtreeInsert(&cache_root,&chdr->wtree_node);
}

void wtreeHeap_print(){
	printf("======================================================================================================\n");
	wtreePrint(&cache_root);
	printf("======================================================================================================\n");
}

