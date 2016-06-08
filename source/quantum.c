/*
 * quantum.c
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#include "quantum.h"
#include "cdsl_nrbtree.h"


typedef struct quantum_node quantumNode_t;

struct quantum_node {
	quantumNode_t   *left,*right;
	uint32_t         free_cnt;
	uint16_t         quantum;
	nrbtreeNode_t    addr_rbnode;
	uint64_t         map[32];
};

void quantum_root_init(quantumRoot_t* root) {
	if(!root)
		return;
	cdsl_nrbtreeRootInit(&root->addr_rbroot);
	cdsl_nrbtreeRootInit(&root->quantum_tree);
}

void quantum_add_segment(quantumRoot_t* root, void* addr,size_t sz) {
	if(!root || !addr)
		return;
}

void* quantum_reclaim_chunk(quantumRoot_t* root, size_t sz) {

}

int quantum_return_chunk(void* chunk) {

}

void quantum_purge_cache(quantumRoot_t* root) {

}
