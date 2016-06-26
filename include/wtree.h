/*
 * wtree.h
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_WTREE_H_
#define INCLUDE_WTREE_H_



#include <stdint.h>
#include <stddef.h>

#include "cdsl_slist.h"

#define DECLARE_WTREE_TRAVERSE_CALLBACK(fn)             BOOL  fn(wtreeNode_t* node,void* arg)
#define DECLARE_WTREE_ONMERGE(fn)                       void  fn(wtreeNode_t* merger, wtreeNode_t* mergee, void* ext_ctx)
#define DECLARE_MAPPER(fn)                              void* fn(size_t total_sz, size_t* rsz, void* ext_ctx)
#define DECLARE_UNMAPPER(fn)                            int fn(void* addr, size_t sz, wtreeNode_t* wtnode, void* ext_ctx)

typedef void* uaddr_t;
typedef struct wtree_node wtreeNode_t;


typedef int (*wt_unmap_func_t) (void* addr, size_t total_sz, wtreeNode_t* wtnode, void* ext_ctx);
typedef void* (*wt_map_func_t) (size_t total_sz, size_t* rsz, void* ext_ctx);
typedef void (*wt_on_merge_t)(wtreeNode_t* merger, wtreeNode_t* mergee, void* ext_ctx);
typedef BOOL (*wt_callback_t) (wtreeNode_t* node,void* arg);

struct wtree_node {
	wtreeNode_t *left, *right;
	uaddr_t       top;
	uint32_t      size;
	uint32_t      base_size;
};

typedef struct  {
	wtreeNode_t*      entry;
	size_t            total_sz;
	size_t            used_sz;
	size_t            hdr_sz;
	wt_unmap_func_t   unmapper;
	wt_map_func_t     mapper;
	wt_on_merge_t     on_merge;
	void*             ext_ctx;
}wtreeRoot_t;

/*          |   @ address   128  |
 *          | p @ base_size 1024 |
 *          |   @ size      512  |
 *          ----------------------
 *         /                      \
 *  |    @ address   0   |     |    @ address   1152  |
 *  |  l @ base_size 0   |     |  r @ base_size 512   |
 *  |    @ size      128 |     |    @ size      256   |
 *
 *  base node keeps information of original allocated segment size  ( base_size != 0 )
 *  normal node doesn't have information of segment   ( base_size = 0 )
 *  if cache should be purged due to any thread termination
 *
 *
 */




extern void wtree_rootInit(wtreeRoot_t* root, void* ext_ctx, wt_map_func_t mapper, wt_unmap_func_t unmapper,wt_on_merge_t merge_cb, size_t cust_hdr_sz);
extern wtreeNode_t* wtree_nodeInit(wtreeRoot_t* root, uaddr_t addr, uint32_t sz);
extern wtreeNode_t* wtree_baseNodeInit(wtreeRoot_t* root, uaddr_t addr, uint32_t sz);
extern void wtree_purge(wtreeRoot_t* root);
extern void wtree_traverseBaseNode(wtreeRoot_t* root, wt_callback_t callback, void* arg);
extern void wtree_addNode(wtreeRoot_t* root, wtreeNode_t* node, BOOL compact);
extern void* wtree_reclaim_chunk(wtreeRoot_t* root, uint32_t sz,BOOL compact);
extern void* wtree_reclaim_chunk_from_node(wtreeNode_t* node, uint32_t sz);
extern void* wtree_grow_chunk(wtreeRoot_t* root, wtreeNode_t** node, uint32_t nsz);
extern void wtree_print(wtreeRoot_t* root);
extern uint32_t wtree_level(wtreeRoot_t* root);
extern size_t wtree_nodeCount(wtreeRoot_t* root);
extern size_t wtree_totalSize(wtreeRoot_t* root);
extern size_t wtree_freeSize(wtreeRoot_t* root);



#endif /* INCLUDE_WTREE_H_ */
