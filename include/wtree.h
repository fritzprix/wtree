/*
 * wtree.h
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */

#ifndef __WTREE_H
#define __WTREE_H

#include <stdint.h>
#include <stddef.h>
#include "base_tree.h"

typedef struct wtreeNode wtreeNode_t;
typedef struct wtreeRoot wtreeRoot_t;
typedef int (*wtreeMerge)(wtreeNode_t* a,wtreeNode_t* b);

/*
 * extern void tree_traverse(base_treeRoot_t* rootp, base_tree_callback_t cb,int order);
extern int tree_size(base_treeRoot_t* rootp);
extern void tree_print(base_treeRoot_t* rootp,cdsl_generic_printer_t prt);
extern int tree_max_depth(base_treeRoot_t* rootp);
 */
#define wtreeTraverse(root,cb,order)    tree_traverse((base_treeRoot_t*) root,cb,order)
#define wtreeSize(root)                 tree_size((base_treeRoot_t*) root)
#define wtreeMaxDepth(root)             tree_max_depth((base_treeRoot_t*) root)

struct wtreeNode {
	union {
		base_treeNode_t __node;
		wtreeNode_t *left,*right;
	};
	uint32_t base;
	uint32_t span;
};

struct wtreeRoot {
	union {
		base_treeRoot_t __root;
		wtreeNode_t*	entry;
	};
	uint32_t		ext_gap;
};

extern void wtreeRootInit(wtreeRoot_t* root,uint32_t ext_gap);
extern void wtreeNodeInit(wtreeNode_t* node,uint64_t base,uint64_t span);
extern void wtreeInsert(wtreeRoot_t* root,wtreeNode_t* item);
extern wtreeNode_t* wtreeRetrive(wtreeRoot_t* root,uint64_t* span);
extern size_t wtreeTotalSpan(wtreeRoot_t* root);

extern void wtreePrint(wtreeRoot_t* root);

#endif
