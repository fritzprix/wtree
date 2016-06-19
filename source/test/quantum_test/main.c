/*
 * main.c
 *
 *  Created on: Jun 11, 2016
 *      Author: innocentevil
 */


#include "quantum.h"
#include "cdsl_nrbtree.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>


#define SEGMENT_SIZE        (1 << 20)

static void* map_segment(size_t req_sz, size_t* res_sz);
static int unmap_segment(void* addr, size_t sz);
static void* test_runner(void* arg);
static uint16_t* mem[100000];

int main() {
	int tloop_cnt = 100;
	pthread_t pid;
	while(tloop_cnt--) {
		printf("loop %d\n",tloop_cnt);
		pthread_create(&pid,NULL,test_runner,NULL);
		pthread_join(pid,NULL);
	}

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

static void* test_runner(void* arg) {
	printf("started\n");
	quantumRoot_t root;
	nrbtreeRoot_t rbroot;


	cdsl_nrbtreeRootInit(&rbroot);
	quantum_root_init(&root, map_segment,unmap_segment);
	int i;
	for(i = 0;i < 100000;i++) {
		mem[i] = quantum_reclaim_chunk(&root, sizeof(uint64_t) * 6);
	}
	for(i = 0;i < 100000;i++) {
		quantum_free_chunk(&root, mem[i]);
	}
	size_t req_sz, base_sz = sizeof(nrbtreeNode_t);
	size_t rand_sz = QUANTUM_MAX - base_sz;
	int rn;
	clock_t end,start = clock();
	uint32_t seed = (uint32_t)start;
	nrbtreeNode_t* node;
	trkey_t key = 0;
	for(i = 0;i < 100000;i++) {
		rn  = rand_r(&seed) % 0xffffff;
		req_sz = base_sz + rn % rand_sz;
		node = quantum_reclaim_chunk(&root, base_sz + rn % rand_sz);

		if(!node) {
			fprintf(stderr,"Memory allocation fail \n");
			exit(-1);
		}
		cdsl_nrbtreeNodeInit(node, i);
		cdsl_nrbtreeInsert(&rbroot, node);
	}
	quantum_print(&root);
	end = clock();
	for(i = 0;i < 100000;i++) {
		node = cdsl_nrbtreeDeleteMax(&rbroot);
		if(!node) {
			fprintf(stderr, "Memory Chunk Lost\n");
			exit(-1);
		}
		if(!quantum_free_chunk(&root, node)) {
			fprintf(stderr, "Unexpected fail in memory free\n");
			exit(-1);
		}
	}
	printf("finished\n");
	quantum_cleanup(&root);

	return NULL;
}



