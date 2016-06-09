/*
 * quantum.c
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#include "quantum.h"
#include "cdsl_nrbtree.h"
/**
 *  QUANTUM Size (bytes)
 *  2 / 4 / 6 .... 36 / 38 / 40 / 42 / 44 / 46 / 48
 *
 *  QUANTUM Pool Size
 *  2 : 4096
 *  4 : 8192
 *  6 : 12288
 *  .
 *  .
 *  .
 *  48 :  98304
 */


typedef struct quantum_node quantumNode_t;
typedef struct quantum quantum_t;

struct quantum {
	nrbtreeNode_t    qtree_node;
	quantumNode_t    *entry;
};

struct quantum_node {
	quantumNode_t   *left,*right;
	uint32_t         free_cnt;
	uint16_t         quantum;
	nrbtreeNode_t    addr_rbnode;
	uint64_t         map[32];
};

static int quantum_pool_unmap(void* addr, size_t sz);
static void quantum_init(quantum_t* quantum, uint16_t qsz);
static void quantum_add(quantumRoot_t* root, quantum_t* quantum);
static void quantum_node_init(quantumNode_t* node, uint16_t qsz,size_t hsz);
static void quantum_node_add(quantum_t* quantum, quantumNode_t* qnode);
static void quantum_node_alloc_unit(quantumNode_t* node);


void quantum_root_init(quantumRoot_t* root) {
	if(!root)
		return;
	cdsl_nrbtreeRootInit(&root->addr_rbroot);
	cdsl_nrbtreeRootInit(&root->quantum_tree);
	wtree_rootInit(&root->quantum_pool,quantum_pool_unmap);
}

void quantum_add_segment(quantumRoot_t* root, void* addr,size_t sz) {
	if(!root || !addr)
		return;
	wtreeNode_t* pool = wtree_baseNodeInit(addr,sz);
	wtree_addNode(&root->quantum_pool, pool, FALSE);
}

void* quantum_reclaim_chunk(quantumRoot_t* root, size_t sz) {
	// calculate quantum size
	if(!root || !sz)
		return NULL;
	if(QUANTUM_MAX < sz)
		return NULL;
	uint16_t qsz = QUANTUM_SPACE;
	quantumNode_t* qnode;
	while((sz = sz - QUANTUM_SPACE) > 0) qsz += QUANTUM_SPACE;
	quantum_t* quantum = cdsl_nrbtreeLookup(&root->quantum_tree, qsz);
	if(!quantum) {
		// there is no quantum for given guantum size
		qnode = wtree_reclaim_chunk(&root->quantum_pool, qsz * 64 * 32, TRUE);
		quantum_node_init(qnode, qsz, sizeof(quantumNode_t) + sizeof(quantum_t));
		quantum = (quantum_t*) &qnode[1];
		quantum_init(quantum, qsz);
		quantum_add(&root->quantum_tree,quantum);
	}
}

int quantum_return_chunk(void* chunk) {

}

void quantum_purge_cache(quantumRoot_t* root) {

}


static int quantum_pool_unmap(void* addr, size_t sz) {

}

static void quantum_init(quantum_t* quantum, uint16_t qsz) {
	if(!quantum)
		return;
	quantum->entry = NULL;
	cdsl_nrbtreeNodeInit(&quantum->qtree_node,qsz);
}


static void quantum_node_init(quantumNode_t* node, uint16_t qsz,size_t hsz) {
	if(!node)
		return;
	node->quantum = qsz;
	node->free_cnt = QUANTUM_COUNT_MAX;
	node->left = node->right = NULL;
	cdsl_nrbtreeNodeInit(&node->addr_rbnode, (size_t) node);
	memset(node->map, 0, sizeof(uint64_t) * 32);
	uint64_t msk = 1;
	uint64_t *map = node->map;

	/*
	 * clear bitmap for occupied area used by header
	 */
	while((hsz = hsz - qsz) > 0) {
		*map &= ~msk;
		msk <<= 1;
		if(!msk) {
			map = &map[1];
			msk = 1;
		}
	}
}


static void quantum_add(quantumRoot_t* root, quantum_t* quantum) {

}


