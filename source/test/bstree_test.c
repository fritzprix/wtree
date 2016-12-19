/*
 * bstree_test.c
 *
 *  Created on: 2016. 12. 20.
 *      Author: innocentevil
 */


#include "bstree_test.h"
#include "bstree.h"

static DECLARE_BSCALLBACK(bscall_back);

int utest_bstree(void) {
	bsRoot bsroot;
	bs_rootInit(&bsroot);

	bsNode* node = malloc(sizeof(bsNode));
	if(!node) {
		return -1;
	}
	bs_nodeInit(node);
	bs_insert(&bsroot, node, bscall_back);
	if(bs_size(&bsroot) != 1) {
		return -1;
	}
	bs_remove(&bsroot, node);
	if(bs_size(&bsroot) != 0) {
		return -1;
	}

	return 0;
}


static DECLARE_BSCALLBACK(bscall_back) {
	return BS_KEEP;
}

