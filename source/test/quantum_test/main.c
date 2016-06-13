/*
 * main.c
 *
 *  Created on: Jun 11, 2016
 *      Author: innocentevil
 */


#include "quantum.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>


#define SEGMENT_SIZE        (1 << 20)

static void* map_segment(size_t req_sz, size_t* res_sz);
static int unmap_segment(void* addr, size_t sz);
static uint16_t* mem[100000];

int main() {
	printf("started\n");
	quantumRoot_t root;
	nrbtreeRoot_t rbroot;
	cdsl_nrbtreeRootInit(&rbroot);
	quantum_root_init(&root, map_segment,unmap_segment);
	int i;
	for(i = 0;i < 100000;i++) {
		mem[i] = quantum_reclaim_chunk(&root, sizeof(uint16_t));
	}
	quantum_print(&root);
	for(i = 0;i < 100000;i++) {
		if(i == 99999)
			quantum_free_chunk(&root, mem[i]);
		quantum_free_chunk(&root, mem[i]);
	}

	size_t base_sz = sizeof(nrbtreeNode_t);
	size_t rand_sz = QUANTUM_MAX - base_sz;
	int rn;
	clock_t end,start = clock();
	uint32_t seed = (uint32_t)start;
	nrbtreeNode_t* node;
	for(i = 0;i < 10000;i++) {
		rn  = base_sz + rand_r(&seed);
		node = quantum_reclaim_chunk(&root, base_sz + rn % rand_sz);

		if(!node) {
			fprintf(stderr,"Memory allocation fail \n");
			exit(-1);
		}
		cdsl_nrbtreeNodeInit(node, rn);
		cdsl_nrbtreeInsert(&rbroot, node);
	}
	end = clock();

	printf("finished\n");
	quantum_print(&root);
	printf("loop : %d\n",i);

	return 0;
}

static void* map_segment(size_t req_sz, size_t* res_sz) {
	printf("req_sz : %lu\n",req_sz);
	if(res_sz)
		*res_sz = SEGMENT_SIZE;
	return mmap(NULL, SEGMENT_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static int unmap_segment(void* addr, size_t sz) {
	munmap(addr,sz);
	return 0;
}
