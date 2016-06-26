/*
 * main.c
 *
 *  Created on: Jun 25, 2016
 *      Author: innocentevil
 */


#include "wtree.h"
#include "segment.h"
#include "cdsl_nrbtree.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>

static DECLARE_MAPPER(mapper);
static DECLARE_UNMAPPER(unmapper);

#ifndef SEGMENT_UNIT_SIZE
#define SEGMENT_UNIT_SIZE        ((size_t) (1 << 23) + (1 << 16))
#endif

#ifndef MALLOC_UNIT_SIZE
#define MALLOC_UNIT_SIZE         ((size_t) (1 << 20))
#endif


struct segment_node {
	nrbtreeNode_t    rbnode;
	size_t           sz;
};

static void* ptr_arry[100];
static size_t sz_arry[100];


int main(void) {
	segmentRoot_t segroot;
	nrbtreeRoot_t rbroot;
	trkey_t key_huge = 1;
	segment_root_init(&segroot, mapper, unmapper);
	cdsl_nrbtreeRootInit(&rbroot);

	segment_create_cache(&segroot, key_huge);

	int cnt;
	void* segment;
	size_t sz;
	struct segment_node* segnode;
	for(cnt = 0; cnt < 100; cnt++) {
		sz = rand() % (1 << 22);
		if(!sz) {
			sz = 1;
		}
		while(sz < MALLOC_UNIT_SIZE) sz <<= 1;
		sz &= ~3;
		printf("size : %lu\n",sz);
		segment = segment_map(&segroot, key_huge, sz);
		if(!segment) {
			fprintf(stderr, "segment map fail %d\n",cnt);
			exit(-1);
		}
		segnode = (struct segment_node*) segment;
		ptr_arry[cnt] = segment;
		sz_arry[cnt] = sz;
		cdsl_nrbtreeNodeInit(&segnode->rbnode, cnt);
		segnode->sz = sz;
		cdsl_nrbtreeInsert(&rbroot, &segnode->rbnode);
	}
	printf("mapping finished\n");
/*
	for(cnt = 0;cnt < 100; cnt++) {
		segment_unmap(&segroot, key_huge, ptr_arry[cnt], sz_arry[cnt]);
	}
*/
	while((segnode = (struct segment_node*) cdsl_nrbtreeDeleteMin(&rbroot))) {
		segnode = container_of(segnode, struct segment_node, rbnode);
		printf("unmmaped %lu\n",segnode->sz);
		segment_unmap(&segroot,key_huge, segnode, segnode->sz);
//		segment_print_cache(&segroot, key_huge);
	}

	printf("finished!\n");

	return 0;
}


static DECLARE_MAPPER(mapper) {

	while(total_sz < SEGMENT_UNIT_SIZE) total_sz <<= 1;
	printf("mapped %lu\n",total_sz);
	void* segment = mmap(NULL, total_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
	if(!segment) {
		*rsz = 0;
		return NULL;
	}
	*rsz = total_sz;
	return segment;
}

static DECLARE_UNMAPPER(unmapper) {
	munmap(addr, sz);
	return 0;
}
