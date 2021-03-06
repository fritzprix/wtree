/*
 * segmapper.c
 *
 *  Created on: Jun 23, 2016
 *      Author: innocentevil
 */


#include <stdio.h>
#include <stdlib.h>

#include "segment.h"

#ifndef SEGMENT_MIN_SIZE
#define SEGMENT_MIN_SIZE                ((size_t) 1 << 24)
#endif

#ifndef SEGMENT_PURGE_DEPTH_THRESHOLD
#define SEGMENT_PURGE_DEPTH_THRESHOLD   16
#endif

#ifndef SEGMENT_PURGE_SIZE_THRESHOLD
#define SEGMENT_PURGE_SIZE_THRESHOLD    8
#endif


typedef struct {
	rbtreeRoot_t   lookup_rtree;
	pthread_mutex_t lock;
} segmentLookup_t;

typedef struct {
	wtreeNode_t    cache_node;
	rbtreeNode_t  addr_node;
} segment_t;

typedef struct {
	slistNode_t   clr_list;
	segment_t     segment_hdr;
} segmentClr_t;

typedef struct {
	BOOL          found;
	void*         addr;
} contain_arg_t;


static DECLARE_TRAVERSE_CALLBACK(segment_for_each_contains);
static DECLARE_ONALLOCATE(segment_internal_mapper);
static DECLARE_ONFREE(segment_internal_unmapper);
static DECLARE_ONADDED(segment_internal_onadd);
static DECLARE_ONREMOVED(segment_internal_onremoved);
static DECLARE_TRAVERSE_CALLBACK(for_each_segcache_try_purge);
static DECLARE_TRAVERSE_CALLBACK(for_each_segcache_cleanup);
static DECLARE_TRAVERSE_CALLBACK(for_each_segment_cleanup);
static void print_segnode(void* node) ;

static wt_adapter adapter = {
		.onremoved = segment_internal_onremoved,
		.onadded = segment_internal_onadd,
		.onallocate = segment_internal_mapper,
		.onfree = segment_internal_unmapper
};

void segment_root_init(segmentRoot_t* root,void* ext_ctx,wt_map_func_t mapper, wt_unmap_func_t unmapper) {
	if(!mapper)   return;
	cdsl_rbtreeRootInit(&root->cache_root);
	root->mapper = mapper;
	root->unmapper = unmapper;
	root->ext_ctx = ext_ctx;
}

void segment_create_cache(segmentRoot_t* root, trkey_t cache_id) {
	if(!root)  return;

	size_t rsz;
	void* chunk = root->mapper(SEGMENT_MIN_SIZE, &rsz, root->ext_ctx);

	wtreeRoot_t dummy_root;
	wtree_rootInit(&dummy_root, NULL, &adapter,  sizeof(segment_t));
	wtreeNode_t* node = wtree_baseNodeInit(&dummy_root, chunk, rsz);
	segmentCache_t* segment_cache = wtree_reclaim_chunk_from_node(node, sizeof(segmentCache_t));

	cdsl_rbtreeRootInit(&segment_cache->addr_rbroot);
	cdsl_rbtreeNodeInit(&segment_cache->rbnode, cache_id);
	segment_cache->bootstrap_seg = chunk;

	segment_cache->free_sz = segment_cache->total_sz = rsz;
	segment_cache->free_sz -= sizeof(segmentCache_t);
	segment_cache->root = root;
	segment_cache->ext_ctx = root->ext_ctx;

	wtree_rootInit(&segment_cache->seg_pool, segment_cache, &adapter, sizeof(segment_t));
	wtree_addNode(&segment_cache->seg_pool, node, TRUE,NULL);
	cdsl_rbtreeInsert(&root->cache_root, &segment_cache->rbnode, FALSE);
}

BOOL segment_is_from_cache(segmentRoot_t* root, trkey_t cache_id, void* addr) {
	if(!root || !addr) return FALSE;

	segmentCache_t* cache = (segmentCache_t*) cdsl_rbtreeLookup(&root->cache_root,cache_id);

	if(!cache) return FALSE;

	cache = container_of(cache, segmentCache_t, rbnode);
	contain_arg_t arg;
	arg.found = FALSE;
	arg.addr = addr;
	cdsl_rbtreeTraverseTarget(&cache->addr_rbroot, segment_for_each_contains, (trkey_t) addr, &arg);
	return arg.found;
}


void* segment_map(segmentRoot_t* root, trkey_t cache_id, size_t sz) {

	if(!root || !sz)  return NULL;

	segmentCache_t* cache = (segmentCache_t*) cdsl_rbtreeLookup(&root->cache_root, cache_id);
	if(!cache) return NULL;

	cache = container_of(cache, segmentCache_t, rbnode);
	void* chunk = wtree_reclaim_chunk(&cache->seg_pool, sz, TRUE);
	if(!chunk) {
		fprintf(stderr, "Out of Memory!\n");
		exit(-1);
	}
	cache->free_sz -= sz;
	return chunk;
}

void segment_unmap(segmentRoot_t* root, trkey_t cache_id, void* addr, size_t sz) {

	if(!root || !sz) return;

	segmentCache_t* cache = (segmentCache_t*) cdsl_rbtreeLookup(&root->cache_root, cache_id);
	if(!cache) return;

	cache = container_of(cache, segmentCache_t, rbnode);
	segment_t* segment = (segment_t*) wtree_nodeInit(&cache->seg_pool, addr, sz, NULL);
	segment = container_of(segment, segment_t, cache_node);
	int depth = 0;
	wtree_addNode(&cache->seg_pool, &segment->cache_node, TRUE,&depth);
	cache->free_sz += sz;
	if((depth > SEGMENT_PURGE_DEPTH_THRESHOLD) ||
			(cache->free_sz > (((cache->total_sz << SEGMENT_PURGE_SIZE_THRESHOLD) - cache->total_sz) >> SEGMENT_PURGE_SIZE_THRESHOLD))) {
		wtree_purge(&cache->seg_pool);
	}
}

void segment_print_cache(segmentRoot_t* root, trkey_t cache_id) {
	if(!root) return;

	segmentCache_t* cache = (segmentCache_t*) cdsl_rbtreeLookup(&root->cache_root, cache_id);
	if(!cache) return;

	cache = container_of(cache, segmentCache_t, rbnode);
	wtree_print(&cache->seg_pool);
}

void segment_purge_target_cache(segmentRoot_t* root, trkey_t cache_id) {
	if(!root)  return;

	segmentCache_t* seg_cache = (segmentCache_t*) cdsl_rbtreeLookup(&root->cache_root, cache_id);
	if(!seg_cache) return;

	seg_cache = container_of(seg_cache, segmentCache_t, rbnode);
	wtree_purge(&seg_cache->seg_pool);
}

void segment_purge_caches(segmentRoot_t* root) {
	if(!root) return;

	cdsl_rbtreeTraverse(&root->cache_root, for_each_segcache_try_purge, ORDER_INC,root);
}

void segment_cleanup(segmentRoot_t* root) {

	if(!root) return;

	cdsl_slistEntryInit(&root->clr_lentry);

	segmentCache_t* cache;
	segment_t* seg;

	cdsl_rbtreeTraverse(&root->cache_root,for_each_segcache_cleanup, ORDER_INC, root);
	segmentClr_t* clr_node;
	while((clr_node = (segmentClr_t*) cdsl_slistRemoveHead(&root->clr_lentry))) {
		clr_node = container_of(clr_node, segmentClr_t, clr_list);
		root->unmapper(clr_node->segment_hdr.cache_node.top - clr_node->segment_hdr.cache_node.base_size,
				clr_node->segment_hdr.cache_node.base_size, &clr_node->segment_hdr.cache_node, root->ext_ctx);
	}
}

static DECLARE_TRAVERSE_CALLBACK(segment_for_each_contains) {
	segment_t* seg_node = container_of(node, segment_t, addr_node);
	contain_arg_t* targ = (contain_arg_t*) arg;
	if((size_t) targ->addr > (size_t) seg_node->cache_node.top)
		return TRAVERSE_OK;
	if((size_t) targ->addr < (size_t) (seg_node->cache_node.top - seg_node->cache_node.base_size))
		return TRAVERSE_OK;
	targ->found = TRUE;
	return TRAVERSE_BREAK;
}

static DECLARE_ONADDED(segment_internal_onadd) {
	if(!ext_ctx || !node) return;

	segment_t* segment = container_of(node, segment_t, cache_node);
	segmentCache_t* segcache = (segmentCache_t*) ext_ctx;
	cdsl_rbtreeNodeInit(&segment->addr_node, (trkey_t) segment->cache_node.top - segment->cache_node.base_size);
	if(segment->cache_node.base_size) {
		cdsl_rbtreeInsert(&segcache->addr_rbroot, &segment->addr_node, FALSE);
	}
}

static DECLARE_ONREMOVED(segment_internal_onremoved) {
	if(!ext_ctx || !node) return;

	segment_t* segment = container_of(node, segment_t, cache_node);
	segmentCache_t* segcache = (segmentCache_t*) ext_ctx;
	cdsl_rbtreeDelete(&segcache->addr_rbroot, segment->addr_node.key);
}

static DECLARE_ONALLOCATE(segment_internal_mapper) {
	if(!ext_ctx || !total_sz) return NULL;

	size_t seg_sz = SEGMENT_MIN_SIZE;
	segmentCache_t* seg_cache = (segmentCache_t*) ext_ctx;
	while(seg_sz < total_sz) seg_sz <<= 1;
	void* chunk= (segment_t*) seg_cache->root->mapper(seg_sz, rsz, seg_cache->root->ext_ctx);
	seg_cache->total_sz += *rsz;
	seg_cache->free_sz += *rsz;
	return chunk;
}

static DECLARE_ONFREE(segment_internal_unmapper) {

	if(!ext_ctx || !wtnode) return -1;

	segmentCache_t* seg_cache = (segmentCache_t*) ext_ctx;
	segment_t* segment = container_of(wtnode, segment_t, cache_node);
	seg_cache->total_sz -= sz;
	seg_cache->free_sz -= sz;
	return seg_cache->root->unmapper(addr, sz, wtnode, seg_cache->root->ext_ctx);
}

static DECLARE_TRAVERSE_CALLBACK(for_each_segcache_try_purge) {

	if(!node) return TRAVERSE_OK;

	segmentCache_t* seg_cache = container_of(node, segmentCache_t, rbnode);
	wtree_purge(&seg_cache->seg_pool);
	return TRAVERSE_OK;
}

static DECLARE_TRAVERSE_CALLBACK(for_each_segcache_cleanup) {

	if(!node) return TRAVERSE_OK;


	segmentRoot_t* seg_root = (segmentRoot_t*) arg;
	segmentCache_t* seg_cache = container_of(node, segmentCache_t, rbnode);
	slistEntry_t cleanup_list;
	cdsl_slistEntryInit(&cleanup_list);
	cdsl_rbtreeTraverse(&seg_cache->addr_rbroot, for_each_segment_cleanup, ORDER_INC, &cleanup_list);
	void* seg_base = seg_cache->bootstrap_seg;
	void* clr_base;
	segmentClr_t* seg_clr;
	while((seg_clr = (segmentClr_t*)cdsl_slistRemoveHead(&cleanup_list))) {
		seg_clr = container_of(seg_clr, segmentClr_t, clr_list);
		clr_base = (void*) ((size_t)seg_clr->segment_hdr.cache_node.top - seg_clr->segment_hdr.cache_node.base_size);
		if(clr_base != seg_base) {
			cdsl_rbtreeDelete(&seg_cache->addr_rbroot, seg_clr->segment_hdr.addr_node.key);
			seg_root->unmapper(clr_base, seg_clr->segment_hdr.cache_node.base_size, &seg_clr->segment_hdr.cache_node,seg_root->ext_ctx);
		} else {
			cdsl_slistNodeInit(&seg_clr->clr_list);
			cdsl_slistPutHead(&seg_root->clr_lentry, &seg_clr->clr_list);
		}
	}
	return TRAVERSE_OK;

}


static DECLARE_TRAVERSE_CALLBACK(for_each_segment_cleanup) {

	if(!node) return TRAVERSE_OK;

	slistEntry_t* clr_list  = (slistEntry_t*) arg;
	segmentClr_t* seg_tobclr = NULL;

	segment_t* segment = container_of(node, segment_t, addr_node);
	if(segment->cache_node.base_size) {
		segmentClr_t* seg_tobclr = container_of(segment, segmentClr_t, segment_hdr);
		cdsl_slistNodeInit(&seg_tobclr->clr_list);
		cdsl_slistPutHead(clr_list, &seg_tobclr->clr_list);
	}
	return TRAVERSE_OK;
}

void print_segnode(void* node) {
	segment_t* segment = container_of(node, segment_t, addr_node);
	printf("@[%lx ~ %lx] (%u)\n",(size_t) segment->cache_node.top - segment->cache_node.base_size, (size_t) segment->cache_node.top, segment->cache_node.base_size);
}



