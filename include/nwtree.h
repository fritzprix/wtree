/*
 * nwtree.h
 *
 *  Created on: Apr 16, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_NWTREE_H_
#define INCLUDE_NWTREE_H_

#include <stdint.h>
#include <stddef.h>

#include "cdsl_slist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DECLARE_PURGE_CALLBACK(fn)          BOOL  fn(nwtreeNode_t* node,void* arg)
typedef void* uaddr_t;
typedef struct nwtree_node nwtreeNode_t;
typedef int (*nwt_unmap_func_t) (void* addr, size_t sz);

struct nwtree_node {
	nwtreeNode_t *left, *right;
	uaddr_t       base;
	uint32_t      size;
	uint32_t      base_size;
};

typedef struct  {
	nwtreeNode_t* entry;
	size_t        sz;
	size_t        used_sz;
	nwt_unmap_func_t   unmap;
}nwtreeRoot_t;

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

typedef BOOL (*nwt_callback_t) (nwtreeNode_t* node,void* arg);


extern void nwtree_rootInit(nwtreeRoot_t* root,nwt_unmap_func_t unmap_func);
extern void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz);
extern void nwtree_baseNodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz);
extern void nwtree_purge(nwtreeRoot_t* root);
extern void nwtree_iterBaseNode(nwtreeRoot_t* root, nwt_callback_t callback, void* arg);
extern void nwtree_addNode(nwtreeRoot_t* root, nwtreeNode_t* node, BOOL compact);
extern void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz,BOOL compact);
extern void* nwtree_grow_chunk(nwtreeRoot_t* root, nwtreeNode_t** node, uint32_t nsz);
extern void nwtree_print(nwtreeRoot_t* root);
extern uint32_t nwtree_level(nwtreeRoot_t* root);
extern size_t nwtree_nodeCount(nwtreeRoot_t* root);
extern size_t nwtree_totalSize(nwtreeRoot_t* root);
extern size_t nwtree_freeSize(nwtreeRoot_t* root);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_NWTREE_H_ */
