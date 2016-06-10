/*
 * quantum.c
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#include "quantum.h"
#include "cdsl_nrbtree.h"

#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

/**
 *  QUANTUM Size (bytes)
 *  2 / 4 / 6 .... 36 / 38 / 40 / 42 / 44 / 46 / 48
 *
 *  QUANTUM Base Size
 *  2 : 4096
 *  4 : 8192
 *  6 : 12288
 *  .
 *  .
 *  .
 *  48 :  98304
 *
 *  QUANTUM Node Size in 64bit machine
 *  304 byte
 *
 *  INTERNAL FRAGMENTATION RATE for QUANTUM Node
 *  2 : 7.4%
 *  4 : 3.7%
 *  6 : 2.4%
 *  .
 *  .
 *  .
 *  48 : 0.3%
 *
 */

#define SEGMENT_SIZE 	(1 << 20)

typedef struct quantum_node quantumNode_t;
typedef struct quantum quantum_t;



struct quantum {
	nrbtreeNode_t    qtree_node;
	quantumNode_t    *entry;
};

struct quantum_node {
	quantumNode_t   *left,*right;      // child tree nodes
	uint16_t         free_cnt;         // number of free quantum
	uint16_t         qcnt;             // number of total available quantum
	uint16_t         quantum;          // quantum class
	nrbtreeNode_t    addr_rbnode;      // rbtree node for address lookup
	uint64_t         map[32];          // bitmap
};

static int quantum_pool_unmap(void* addr, size_t sz);
static void quantum_init(quantum_t* quantum, uint16_t qsz);
static void quantum_add(quantumRoot_t* root, quantum_t* quantum);
static void quantum_node_init(quantumNode_t* node, uint16_t qsz,size_t hsz);
static void quantum_node_add(quantumRoot_t* root, quantum_t* quantum, quantumNode_t* qnode);
static quantumNode_t* insert_rc(quantumNode_t* parent, quantumNode_t* item);
static void* quantum_node_alloc_unit(quantumNode_t* node);
static quantumNode_t* quantum_node_rotate_right(quantumNode_t* qnode);
static quantumNode_t* quantum_node_rotate_left(quantumNode_t* qnode);


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
	/*
	 *  direct casting is possible, because rbnode is first element in quantum struct
	 */
	quantum_t* quantum = (quantum_t*) cdsl_nrbtreeLookup(&root->quantum_tree, qsz);
	if(!quantum) {
		// there is no quantum for given guantum size
		while(!(qnode = wtree_reclaim_chunk(&root->quantum_pool, qsz * 64 * 32, TRUE))) {
			uint8_t* seg = mmap(NULL, SEGMENT_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			quantum_add_segment(root, seg, SEGMENT_SIZE);
		}
		quantum_node_init(qnode, qsz, sizeof(quantumNode_t) + sizeof(quantum_t));
		quantum = (quantum_t*) &qnode[1];
		quantum_init(quantum, qsz);
		quantum_add(root,quantum);
		quantum_node_add(root, quantum, qnode);
	}
	qnode = quantum->entry;
	uint8_t *chnk = NULL;
	while(!(chnk = quantum_node_alloc_unit(qnode))) {
		while(!(qnode = wtree_reclaim_chunk(&root->quantum_pool, qsz * 64 * 32, TRUE))) {
			uint8_t* seg = mmap(NULL, SEGMENT_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			quantum_add_segment(root, seg, SEGMENT_SIZE);
		}
		quantum_node_init(qnode, qsz, sizeof(quantumNode_t));
		quantum_node_add(root, quantum, qnode);
	}
	return chnk;
}

int quantum_return_chunk(void* chunk) {
	if(!chunk)
		return FALSE;
	return TRUE;
}

void quantum_purge_cache(quantumRoot_t* root) {

}


static int quantum_pool_unmap(void* addr, size_t sz) {
	return 0;
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
	node->free_cnt = node->qcnt = QUANTUM_COUNT_MAX;
	node->left = node->right = NULL;
	cdsl_nrbtreeNodeInit(&node->addr_rbnode, (size_t) node);
	memset(node->map, 0, sizeof(uint64_t) * 32);
	uint64_t msk = 1;
	uint64_t *map = node->map;

	/*
	 * clear bitmap for occupied area used by header
	 */
	uint16_t shift_cnt = 0;
	while((hsz = hsz - qsz) > 0) {
		*map &= ~msk;
		msk <<= 1;
		node->free_cnt--;
		node->qcnt--;
		if(!msk) {
			map = &map[1];
			msk = 1;
		}
	}
}


static void quantum_add(quantumRoot_t* root, quantum_t* quantum) {
	if(!root || !quantum)
		return;
	cdsl_nrbtreeInsert(&root->quantum_tree, &quantum->qtree_node);
}

static void quantum_node_add(quantumRoot_t* root, quantum_t* quantum, quantumNode_t* qnode) {
	if(!quantum || !qnode || !root)
		return;
	quantum->entry = insert_rc(quantum->entry, qnode);
	cdsl_nrbtreeInsert(&root->addr_rbroot, &qnode->addr_rbnode);
}

static quantumNode_t* insert_rc(quantumNode_t* parent, quantumNode_t* item) {
	if(!parent)
		return item;
	if((size_t)parent < (size_t) item) {
		parent->right = insert_rc(parent->right, item);
		if(!parent->right)
			return parent;
		if(parent->right->free_cnt > parent->free_cnt) {
			parent = quantum_node_rotate_left(parent);
		}
	} else if((size_t) parent > (size_t) item) {
		parent->left = insert_rc(parent->left, item);
		if(!parent->left)
			return parent;
		if(parent->left->free_cnt > parent->free_cnt) {
			parent = quantum_node_rotate_right(parent);
		}
	} else {
		fprintf(stderr, "Quantum Node Collision\n");
		exit(-1);
	}
	return parent;

}

static quantumNode_t* quantum_node_rotate_right(quantumNode_t* qnode) {
	if(!qnode || !qnode->left)
		return qnode;
	quantumNode_t* top = qnode->left;
	qnode->left = top->right;
	top->right = qnode;
	return top;
}

static quantumNode_t* quantum_node_rotate_left(quantumNode_t* qnode) {
	if(!qnode || !qnode->right)
		return qnode;
	quantumNode_t* top = qnode->right;
	qnode->right = top->left;
	top->left = qnode;
	return top;
}


static void* quantum_node_alloc_unit(quantumNode_t* qnode) {
	if(!qnode)
		return NULL;
	uint16_t offset = 0;
	uint64_t* cmap = qnode->map;
	uint64_t  vmap = 0;
	uint64_t  imap = 1;
	while(*cmap && (offset < QUANTUM_COUNT_MAX)) {
		cmap++;
		offset += 64;
	}
	vmap = *cmap;
	while(!(vmap & 1)) {
		vmap >>= 1;
		imap <<= 1;
		offset++;
	}
	*cmap &= ~imap;
	return &((uint8_t*) qnode)[offset];
}
