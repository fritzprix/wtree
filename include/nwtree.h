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

typedef void* uaddr_t;
typedef struct nwtree_node nwtreeNode_t;

/*
 * _________________________________________________
 * | left | right | base | size | memory (bytes of size)  ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
struct nwtree_node {
	nwtreeNode_t *left, *right;
	uaddr_t       base;
	uint32_t      size;
	uint32_t      free_sz;
};

typedef struct  {
	nwtreeNode_t* entry;
	size_t        sz;
	size_t        used_sz;
}nwtreeRoot_t;


extern void nwtree_rootInit(nwtreeRoot_t* root);
extern void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz);
extern void nwtree_addNode(nwtreeRoot_t* root, nwtreeNode_t* node);
extern void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz);
extern int nwtree_free_chunk(nwtreeRoot_t* root, void* chnk);
extern nwtreeNode_t* nwtree_delete(nwtreeRoot_t* root);
extern void nwtree_print(nwtreeRoot_t* root);





#endif /* INCLUDE_NWTREE_H_ */
