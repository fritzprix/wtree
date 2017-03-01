/*
 * bstree_test.c
 *
 *  Created on: 2016. 12. 20.
 *      Author: innocentevil
 */


#include "bstree_test.h"
#include "bstree.h"


static DECLARE_BSCALLBACK(bscall_back);
static bsNode chunks[TEST_SIZE];

int utest_bstree(void) {
	bsRoot bsroot;
	int i;
	bs_rootInit(&bsroot);

	for(i = 0;i < TEST_SIZE; i++) {
		bs_nodeInit(&chunks[i]);
		bs_insert(&bsroot,&chunks[i],bscall_back);
	}

	if(bs_size(&bsroot) != TEST_SIZE) {
		return -1;
	}

	for(i = 0;i < TEST_SIZE; i++) {
		bs_remove(&bsroot, &chunks[i]);
	}
	if(bs_size(&bsroot) != 0) {
		return -1;
	}

	return 0;
}


static DECLARE_BSCALLBACK(bscall_back) {
	return BS_KEEP;
}

