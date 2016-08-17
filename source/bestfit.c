

#include "bestfit.h"
#include "wtree.h"
#include "test/common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#ifndef BFIT_ALIGNMENT
#define BFIT_ALIGNMENT                         4
#endif

#ifndef BFIT_CACHE_PURGE_THRESHOLD
#define BFIT_CACHE_PURGE_THRESHOLD             8
#endif

typedef struct {
	uint32_t     prev_sz;
	uint32_t     cur_sz;
}bfit_chunkHeader_t;

static DECLARE_ONALLOCATE(bfit_internal_mapper);
static DECLARE_ONFREE(bfit_internal_unmapper);

static wt_adapter cache_adapter = {
		.onallocate = bfit_internal_mapper,
		.onfree = bfit_internal_unmapper,
		.onremoved = NULL,
		.onadded = NULL
};


bfitRoot_t* bfit_root_create(void* ext_ctx, wt_map_func_t mapper, wt_unmap_func_t unmapper) {
	/**
	 *  there is mapper given, so bootstrapping is possible
	 */
	if(!mapper || !unmapper) return NULL;

	bfitRoot_t* root;
	wtreeRoot_t dummy;
	size_t rsz;
	void* chunk = mapper(1,&rsz,ext_ctx);
	wtree_rootInit(&dummy, root, &cache_adapter, 0);
	wtreeNode_t* node = wtree_baseNodeInit(&dummy,chunk, rsz);
	root = wtree_reclaim_chunk_from_node(node, sizeof(bfitRoot_t));

	root->ext_ctx = ext_ctx;
	root->mapper = mapper;
	root->unmapper = unmapper;
	root->free_sz = root->total_sz = rsz;
	root->free_sz -= sizeof(bfitRoot_t);
	wtree_rootInit(&root->bfit_cache, root, &cache_adapter, 0);
	wtree_addNode(&root->bfit_cache, node,TRUE);
	return root;
}

void* bfit_reclaim_chunk(bfitRoot_t* root, size_t sz) {

	if(!root || !sz)   return NULL;

	if(sz < sizeof(wtreeNode_t))   sz = sizeof(wtreeNode_t);

	sz += sizeof(bfit_chunkHeader_t);
	sz = (sz + BFIT_ALIGNMENT) & ~(BFIT_ALIGNMENT - 1);

	uint8_t* chunk = wtree_reclaim_chunk(&root->bfit_cache, sz, TRUE);
	if(!chunk) {
		fprintf(stderr, "Out of Memory!\n");
		exit(-1);
	}
	root->free_sz -= sz;
	*((uint32_t*) chunk) = sz - sizeof(uint32_t);
	*((uint32_t*) &chunk[sz - sizeof(uint32_t)]) = sz - sizeof(uint32_t);
	return &chunk[sizeof(uint32_t)];
}

void* bfit_reclaim_aligned_chunk(bfitRoot_t* root,size_t sz, size_t alignment) {
	if(!root || !sz || !alignment) return NULL;

	dbg_print("====== aligned chunk request @(%zu / %zu) ======= \n", sz, alignment);

	// set minimum required size for bestfit allocator
	if(sz < sizeof(wtreeNode_t)) sz = sizeof(wtreeNode_t);

	// set minimum required alignment for bestfit allocator
	if(alignment < BFIT_ALIGNMENT) alignment = BFIT_ALIGNMENT;

	sz += sizeof(bfit_chunkHeader_t);

	sz = (sz + BFIT_ALIGNMENT) & ~(BFIT_ALIGNMENT - 1);


	uint8_t* chunk, *bound;
	size_t msz;
	size_t aligned;
	if(alignment > sz) {
		msz = (alignment << 1) + alignment;
		chunk = wtree_reclaim_chunk(&root->bfit_cache, msz, FALSE);
	} else {
		msz = (sz << 1) + sz;
		chunk = wtree_reclaim_chunk(&root->bfit_cache, msz, FALSE);
	}
	dbg_print("first allocated size : %zu \n", msz);
	bound = &chunk[msz];

	aligned = ((size_t) chunk + sizeof(wtreeNode_t) + alignment) & ~(alignment - 1);
	aligned -= sizeof(uint32_t);

	dbg_print("freed head chunk size : %zu\n", aligned - (size_t) chunk);

	// free truncated chunk at the bottom
	wtreeNode_t* node = wtree_nodeInit(&root->bfit_cache,chunk, aligned - (size_t) chunk,NULL);
	wtree_addNode(&root->bfit_cache, node, TRUE);

	msz -= (aligned - (size_t) chunk);

	chunk = &((uint8_t*) aligned)[sz];
	if((size_t) chunk + sizeof(wtreeNode_t) < (size_t) bound) {
		node = wtree_nodeInit(&root->bfit_cache, chunk, (size_t) (bound - chunk), NULL);
		wtree_addNode(&root->bfit_cache, node, TRUE);
		msz -= (size_t) (bound - chunk);
		dbg_print("freed tail chunk size : %zu\n", (size_t) (bound - chunk));
	}

	root->free_sz -= msz;
	chunk = (uint8_t*) aligned;

	dbg_print("actual allocated size : %zu\n", msz);
	if(msz < sz) {
		exit(-1);
	}
	*((uint32_t*) aligned) = msz - sizeof(uint32_t);
	*((uint32_t*) &chunk[msz - sizeof(uint32_t)]) = msz - sizeof(uint32_t);
	if(&chunk[msz - sizeof(uint32_t)] > bound) {
		fprintf(stderr, "OVER BOUND\n");
		exit(-1);
	}
	dbg_print("====== aligned chunk request @(%lu) ======= \n", (size_t) &chunk[sizeof(uint32_t)]);
	return (void*) &chunk[sizeof(uint32_t)];
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
	if(root->free_sz > (((root->total_sz << BFIT_CACHE_PURGE_THRESHOLD) - root->total_sz) >> BFIT_CACHE_PURGE_THRESHOLD)) {
		wtree_purge(&root->bfit_cache);
	}
}

void bfit_purge_cache(bfitRoot_t* root) {
	if(!root) return;
	wtree_purge(&root->bfit_cache);
}

void bfit_cleanup(bfitRoot_t* root) {
	if(!root) return;
	wtree_cleanup(&root->bfit_cache);
}

size_t bfit_chunk_size(bfitRoot_t* root, void* chunk) {
	if(!root) return 0;
	return  (size_t) ((((uint32_t*) chunk - 1)[0]) - sizeof(uint32_t));
}



static DECLARE_ONALLOCATE(bfit_internal_mapper) {
	bfitRoot_t* root = (bfitRoot_t*) ext_ctx;
	void* chunk = root->mapper(total_sz, rsz, root->ext_ctx);
	root->total_sz += *rsz;
	root->free_sz += *rsz;
	return chunk;
}

static DECLARE_ONFREE(bfit_internal_unmapper) {
	if(!addr) return FALSE;
	bfitRoot_t* root = (bfitRoot_t*) ext_ctx;
	root->total_sz -= sz;
	root->free_sz -= sz;
	return root->unmapper(addr, sz, wtnode, root->ext_ctx);
}
