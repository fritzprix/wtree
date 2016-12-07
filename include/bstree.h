/*
 * bstree.h
 *
 *  Created on: 2016. 12. 5.
 *      Author: innocentevil
 */

#ifndef INCLUDE_BSTREE_H_
#define INCLUDE_BSTREE_H_

#include "wtree.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 *   this is binary search tree implemented for satisfying specific need of memory allocator
 *   typically memory chunks (or segment what ever it is) is managed with data structure like list or tree
 *   when chunk is required to be looked up to check it's availability, tree is more beneficial over list.
 *   however tree needs key value in its node. so for 32 bit system (assume both address & key size are 32 bit).
 *   each tree node has to have 12 byte at the minimum.
 *
 *   this binary search tree implementation address those node size issue, so this tree use the chunk address as a key.
 */

#define BS_KEEP             ((int) 0x00)
#define BS_STOP             ((int) 0x01)
#define BS_REMOVE_THIS      ((int) 0x02)


typedef struct bs_node bsNode;
typedef struct bs_root bsRoot;
typedef int (*bs_traverse_callback) (bsNode* node);


struct bs_node {
	bsNode *left, *right;
};

struct bs_root {
	bsNode *entry;
};

/**
 *  initialize bstree root
 */
extern void bs_rootInit(bsRoot* root);

/**
 *  initialize bstree node
 */
extern void bs_nodeInit(bsNode* node);

/**
 * insert node into tree
 * optionally, traverse all the nodes recursively in the process of insertion
 * and slightly modify the insert behavior as below
 *  - removing any node(s) encountered in the process of insertion (BS_REMOVE_THIS)
 *  - cancel insertion operation (BS_STOP)
 *
 */
extern void bs_insert(bsRoot* root, bsNode* node, bs_traverse_callback callback);

/**
 * traverse tree with target branch
 */
extern void bs_traverse(bsRoot* root, bsNode* target, bs_traverse_callback callback);
extern int bs_lookup(bsRoot* root, bsNode* node);
extern void bs_remove(bsRoot* root, bsNode* node);


#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_BSTREE_H_ */
