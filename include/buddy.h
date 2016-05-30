/*
 * nwtbuddy.h
 *
 *  Created on: May 29, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_BUDDY_H_
#define INCLUDE_BUDDY_H_

#include <stddef.h>
#include <stdint.h>
#include "cdsl_nrbtree.h"


#ifdef __cplusplus
extern "C" {
#endif

/*
 *
 *           root
 *           /  \
 *    gran_root  gran_root
 *
 */

#define GRAN_SPACE           ((uint16_t) 2)
#define GRAN_MAX             ((uint16_t) 64)
#define MAP_OFFSET           ((uint16_t) 16)

typedef struct nwtBuddyNode nwtBuddyNode_t;

struct nwtBuddyNode{
	nwtBuddyNode_t *left,*right;
	uint32_t        free_cnt;
	uint16_t        gran;
	nrbtreeNode_t   glb_rbnode;
	uint16_t        map[32];             // start of alloc bitmap, 1bit = 2 byte (1 : 16 bitmap)  4kbyte  64kbyte
};

typedef struct {
	nrbtreeRoot_t   glb_rbroot;
	nrbtreeRoot_t   gran_tree;
} nwtBuddyRoot_t;


extern void nwtbuddy_rootInit(nwtBuddyRoot_t* root);
extern void nwtbuddy_addSegment(nwtBuddyRoot_t* root, void* addr, size_t sz);
extern void* nwtbuddy_reclaim_chunk(nwtBuddyRoot_t* root, size_t sz);
extern int nwtbuddy_return_chunk(void* chunk);
extern void nwtbuddy_purgeCache();



#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_BUDDY_H_ */
