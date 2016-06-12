/*
 * main.c
 *
 *  Created on: Jun 11, 2016
 *      Author: innocentevil
 */


#include "quantum.h"
#include <stdio.h>
#include <sys/mman.h>


#define SEGMENT_SIZE        (1 << 20)

static void* map_segment(size_t req_sz, size_t* res_sz);
static int unmap_segment(void* addr, size_t sz);
static uint16_t* mem[10000];

int main() {
	printf("started\n");
	quantumRoot_t root;
	quantum_root_init(&root, map_segment,unmap_segment);
	int i;
	for(i = 0;i < 10000;i++) {
		mem[i] = quantum_reclaim_chunk(&root, sizeof(uint16_t));
	}
	quantum_print(&root);
	for(i = 0;i < 10000;i++) {
		quantum_free_chunk(&root, mem[i]);
	}
	quantum_print(&root);
	printf("finished\n");
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
