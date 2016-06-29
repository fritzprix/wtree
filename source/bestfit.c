

#include "bestfit.h"
#include "wtree.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#ifndef BFIT_ALIGNMENT
#define BFIT_ALIGNMENT                         4
#endif

typedef struct {
	slistNode_t  cleanup_lhead;
	wtreeNode_t  cache_node;
}bfit_cleanupHeader_t;

typedef struct {
	uint32_t     prev_sz;
	uint32_t     cur_sz;
}bfit_chunkHeader_t;

static DECLARE_WTREE_TRAVERSE_CALLBACK(bfit_oncleanup);
static DECLARE_ONALLOCATE(bfit_internal_mapper);
static DECLARE_ONFREE(bfit_internal_unmapper);

static wt_adapter cache_adapter = {
		.onallocate = bfit_internal_mapper,
		.onfree = bfit_internal_unmapper,
		.onremoved = NULL,
		.onadded = NULL
};


void bfit_root_init(bfitRoot_t* root, void* ext_ctx, wt_map_func_t mapper, wt_unmap_func_t unmapper) {

	if(!root || !mapper || !unmapper) return;

	root->ext_ctx = ext_ctx;
	root->mapper = mapper;
	root->unmapper = unmapper;

	root->free_sz = root->total_sz = 0;
	wtree_rootInit(&root->bfit_cache, root, &cache_adapter, 0);
	size_t rsz;
	void* chunk = root->mapper(1,&rsz,ext_ctx);
	wtreeNode_t* node = wtree_baseNodeInit(&root->bfit_cache,chunk, rsz);
	wtree_addNode(&root->bfit_cache, node,TRUE);
	wtree_reclaim_chunk_from_node(node, 4);
}

void* bfit_reclaim_chunk(bfitRoot_t* root, size_t sz) {

	if(!root || !sz)   return NULL;

	if(sz < sizeof(wtreeNode_t))   sz = sizeof(wtreeNode_t);

	sz += sizeof(bfit_chunkHeader_t);
	sz = (sz + BFIT_ALIGNMENT) & ~(BFIT_ALIGNMENT);

	uint8_t* chunk = wtree_reclaim_chunk(&root->bfit_cache,sz , TRUE);
	if(!chunk) {
		fprintf(stderr, "Out of Memory!\n");
		exit(-1);
	}
	root->free_sz -= sz;
	*((uint32_t*) chunk) = sz - sizeof(uint32_t);
	*((uint32_t*) &chunk[sz - sizeof(uint32_t)]) = sz - sizeof(uint32_t);
	return &chunk[sizeof(uint32_t)];
}


void* bfit_grows_chunk(bfitRoot_t* root, void* chunk, size_t nsz) {

	if(!root || !nsz) return chunk;

	if(nsz < sizeof(wtreeNode_t)) return chunk;
	nsz += sizeof(bfit_chunkHeader_t);
	nsz = (nsz + BFIT_ALIGNMENT) & ~(BFIT_ALIGNMENT - 1);

	uint32_t *sz_chk, *cur_sz, dsz;
	cur_sz = ((uint32_t*) chunk - 1);
	sz_chk = (uint32_t*) &(((uint8_t*) cur_sz)[*cur_sz]);
	dsz = nsz - *cur_sz;
	if(*cur_sz != *sz_chk) {
		fprintf(stderr, "Heap corrupted %u : %u\n", *cur_sz, *sz_chk);
		exit(-1);
	}
	if((*cur_sz - sizeof(uint32_t)) >= nsz) {
		return chunk;
	}
	uint8_t* nchunk;
	wtreeNode_t header_presv, *node;
	node = wtree_nodeInit(&root->bfit_cache, cur_sz, *cur_sz + sizeof(uint32_t), &header_presv);
	nchunk = wtree_grow_chunk(&root->bfit_cache, &node, nsz);

	if(!node) {
		memcpy(nchunk, cur_sz, *cur_sz + sizeof(uint32_t) - sizeof(wtreeNode_t));
	}
	wtree_restorePreserved(&root->bfit_cache, nchunk, *cur_sz + sizeof(uint32_t), &header_presv);
	root->free_sz -= dsz;
	*((uint32_t*) nchunk) = nsz - sizeof(uint32_t);
	*((uint32_t*) &nchunk[nsz - sizeof(uint32_t)]) = nsz - sizeof(uint32_t);
	return &((uint32_t*) nchunk)[1];
}

void bfit_free_chunk(bfitRoot_t* root, void* chunk) {
	if(!root || !chunk) return;

	uint32_t *cur_sz, *sz_chk;
	cur_sz = ((uint32_t*) chunk - 1);
	sz_chk = (uint32_t*) &(((uint8_t*) cur_sz)[*cur_sz]);
	if(*cur_sz != *sz_chk) {
		fprintf(stderr, "Heap corrupted %u : %u\n",*cur_sz, *sz_chk);
		exit(-1);
	}

	root->free_sz += *cur_sz + sizeof(uint32_t);
	wtreeNode_t* node = wtree_nodeInit(&root->bfit_cache, cur_sz, *cur_sz + sizeof(uint32_t), NULL);
	wtree_addNode(&root->bfit_cache, node, TRUE);
	if(root->free_sz == root->total_sz) {
		wtree_purge(&root->bfit_cache);
	}
}

void bfit_purge_cache(bfitRoot_t* root) {
	if(!root) return;
	wtree_purge(&root->bfit_cache);
}

void bfit_clean_up(bfitRoot_t* root) {
	if(!root) return;
	slistEntry_t cleanup_list;
	cdsl_slistEntryInit(&cleanup_list);
	wtree_traverseBaseNode(&root->bfit_cache, bfit_oncleanup, &cleanup_list);
}

static DECLARE_WTREE_TRAVERSE_CALLBACK(bfit_oncleanup) {
	if(node) return FALSE;

	bfit_cleanupHeader_t* cleanup_hdr = container_of(node, bfit_cleanupHeader_t, cache_node);
	slistEntry_t* cleanup_list = (slistEntry_t*) arg;
	cdsl_slistNodeInit(&cleanup_hdr->cleanup_lhead);
	cdsl_slistPutHead(cleanup_list, &cleanup_hdr->cleanup_lhead);
	return TRUE;
}

static DECLARE_ONALLOCATE(bfit_internal_mapper) {
	bfitRoot_t* root = (bfitRoot_t*) ext_ctx;
	void* chunk = root->mapper(total_sz, rsz, NULL);
	root->total_sz += *rsz;
	root->free_sz += *rsz;
	return chunk;
}

static DECLARE_ONFREE(bfit_internal_unmapper) {
	if(!addr) return FALSE;
	bfitRoot_t* root = (bfitRoot_t*) ext_ctx;
	root->total_sz -= sz;
	root->free_sz -= sz;
	return root->unmapper(addr, sz, wtnode, NULL);
}
