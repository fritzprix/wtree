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

static DECLARE_ONALLOCATE(mapper);
static DECLARE_ONFREE(unmapper);


#ifndef MALLOC_UNIT_SIZE
#define MALLOC_UNIT_SIZE         ((size_t) (1 << 20))
#endif


struct segment_node {
	nrbtreeNode_t    rbnode;
	size_t           sz;
};

static void* segment_test(void* arg);


int main(void) {
	pthread_t pids[10];
	int i = 0;
	for(;i < 1; i++) {
		pthread_create(&pids[i], NULL, segment_test,NULL);
	}

	for(i = 0;i < 1; i++) {
		pthread_join(pids[i], NULL);
	}
	return 0;
}


static void* segment_test(void* arg) {
	segmentRoot_t segroot;
	nrbtreeRoot_t rbroot;
	trkey_t key_large = 1;
	segment_root_init(&segroot, NULL, mapper, unmapper);
	cdsl_nrbtreeRootInit(&rbroot);

	segment_create_cache(&segroot, key_large);

	uint32_t cnt;
	void* segment;
	size_t sz;
	struct segment_node* segnode;
	for(cnt = 0; cnt < 100; cnt++) {
		printf("cnt : %d\n",cnt);
		sz = rand() % (1 << 22);
		if(!sz) {
			sz = 1;
		}
		while(sz < MALLOC_UNIT_SIZE) sz <<= 1;
		sz &= ~3;
		segment = segment_map(&segroot, key_large, sz);
		if(!segment) {
			fprintf(stderr, "segment map fail %d\n",cnt);
			exit(-1);
		}
		segnode = (struct segment_node*) segment;
		cdsl_nrbtreeNodeInit(&segnode->rbnode, cnt);
		segnode->sz = sz;
		cdsl_nrbtreeInsert(&rbroot, &segnode->rbnode);
	}
	printf("map finished\n");


	while((segnode = (struct segment_node*) cdsl_nrbtreeDeleteMax(&rbroot))) {
		segnode = container_of(segnode, struct segment_node, rbnode);
		segment_unmap(&segroot,key_large, segnode, segnode->sz);
	}
	segment_print_cache(&segroot, key_large);
	segment_cleanup(&segroot);
	printf("finished test \n");
	return NULL;
}


static DECLARE_ONALLOCATE(mapper) {
	void* segment = mmap(NULL, total_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
	if(!segment) {
		*rsz = 0;
		return NULL;
	}
	*rsz = total_sz;
	return segment;
}

static DECLARE_ONFREE(unmapper) {
	munmap(addr, sz);
	return 0;
}
