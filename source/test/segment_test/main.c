/*
 * main.c
 *
 *  Created on: Jun 25, 2016
 *      Author: innocentevil
 */


#include "wtree.h"
#include "segment.h"
#include "cdsl_nrbtree.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>

static DECLARE_ONALLOCATE(mapper);
static DECLARE_ONFREE(unmapper);


#ifndef MALLOC_UNIT_SIZE
#define MALLOC_UNIT_SIZE          (1 << 20)
#endif

#define TH_CNT                    6


struct segment_node {
	nrbtreeNode_t    rbnode;
	size_t           sz;
};

static void* segment_test(void* arg);


int main(void) {
	pthread_t pids[TH_CNT];
	pthread_t thread;
	int j,i = 0;

	for (j = 0; j < 100; j++) {
		for (i = 0; i < TH_CNT; i++) {
			pthread_create(&pids[i], NULL, segment_test, NULL);
		}
		for (i = 0; i < TH_CNT; i++) {
			pthread_join(pids[i], NULL);
			printf("all finished  %d @ %d\n",i,j);
		}
	}
	return 0;
}


static void* segment_test(void* arg) {
	segmentRoot_t segroot;
	nrbtreeRoot_t rbroot;
	trkey_t key_large = 1;
	setbuf(stdout, NULL);
	segment_root_init(&segroot, NULL, mapper, unmapper);
	cdsl_nrbtreeRootInit(&rbroot);

	segment_create_cache(&segroot, key_large);

	uint32_t cnt;
	void* segment;
	size_t sz;
	struct segment_node* segnode;
	clock_t clk = clock();
	uint32_t seed = (uint32_t) clk;
	for(cnt = 0; cnt < 50; cnt++) {
		sz = rand_r(&seed) % (1 << 22);
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
//	segment_print_cache(&segroot, key_large);
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
	printf("unmapped? (%lx, %lu)\n", (size_t) addr, (size_t) sz);
	munmap(addr, sz);
	printf("unmapped!! (%lx, %lu)\n", (size_t) addr, (size_t) sz);
	return 0;
}
