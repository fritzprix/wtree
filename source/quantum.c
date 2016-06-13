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

#define QMAP_COUNT		32
#define SEGMENT_SIZE 	(1 << 20)

#define DIR_LEFT		1
#define DIR_RIGHT       2

typedef struct quantum_node quantumNode_t;
typedef struct quantum quantum_t;

typedef uint64_t qmap_t;


struct quantum {
	nrbtreeNode_t    qtree_node;
	quantumNode_t    *entry;
};

struct quantum_node {
	quantumNode_t   *left,*right;      // child tree nodes
	quantumNode_t   *parent;           // parent
	uint16_t         free_cnt;         // number of free quantum
	uint16_t         qcnt;             // number of total available quantum
	uint16_t         quantum;          // quantum class
	void*            top;              // top
	nrbtreeNode_t    addr_rbnode;      // rbtree node for address lookup
	qmap_t           map[QMAP_COUNT];  // bitmap
};

const size_t QMAP_UNIT_OFFSET = (sizeof(qmap_t) << 3);

static DECLARE_TRAVERSE_CALLBACK(for_each_qnode);
static DECLARE_TRAVERSE_CALLBACK(for_each_quantum_print);


static void quantum_init(quantum_t* quantum, uint16_t qsz);
static void quantum_add(quantumRoot_t* root, quantum_t* quantum);
static void quantum_purge(quantum_t* quantum);

static void quantum_node_init(quantumNode_t* node, uint16_t qsz,ssize_t hsz, BOOL base);
static void quantum_node_add(quantumRoot_t* root, quantum_t* quantum, quantumNode_t* qnode);
static void* quantum_node_alloc_unit(quantumNode_t* node);
static void quantum_node_free_unit(quantumNode_t* qnode, void* chunk);
static void quantum_node_resolve_upward(quantumRoot_t* root, quantumNode_t* parent, quantumNode_t* child, quantumNode_t* child_hint);
static quantumNode_t* quantum_node_insert_rc(quantumNode_t* parent, quantumNode_t* item);
static quantumNode_t* quantum_node_rotate_right(quantumNode_t* qnode);
static quantumNode_t* quantum_node_rotate_left(quantumNode_t* qnode);
static void quantum_node_print_rc(quantumNode_t* qnode, int level);
static void quantum_node_print(quantumNode_t* qnode,int level);


void quantum_root_init(quantumRoot_t* root, wt_map_func_t mapper, wt_unmap_func_t unmapper) {
	if(!root)
		return;
	cdsl_nrbtreeRootInit(&root->addr_rbroot);
	cdsl_nrbtreeRootInit(&root->quantum_tree);
	wtree_rootInit(&root->quantum_pool,mapper, unmapper);

	size_t seg_sz;
	uint8_t* init_seg = mapper(1, &seg_sz);
	wtreeNode_t* qpool = wtree_baseNodeInit(init_seg, seg_sz);
	wtree_addNode(&root->quantum_pool,qpool,FALSE);
}

void* quantum_reclaim_chunk(quantumRoot_t* root, ssize_t sz) {
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
		quantum = wtree_reclaim_chunk(&root->quantum_pool, sizeof(quantum_t),FALSE);
		qnode = wtree_reclaim_chunk(&root->quantum_pool, qsz * QMAP_UNIT_OFFSET * QMAP_COUNT, TRUE);
		quantum_node_init(qnode, qsz, sizeof(quantumNode_t),FALSE);
		quantum_init(quantum, qsz);
		quantum_add(root,quantum);
		quantum_node_add(root, quantum, qnode);
	}

	qnode = quantum->entry;
	uint8_t *chnk = NULL;
	while(!(chnk = quantum_node_alloc_unit(qnode))) {
		qnode = wtree_reclaim_chunk(&root->quantum_pool, qsz * QMAP_UNIT_OFFSET * QMAP_COUNT, TRUE);
		quantum_node_init(qnode, qsz, sizeof(quantumNode_t), FALSE);
		quantum_node_add(root, quantum, qnode);
		quantum_print(root);
	}
	return chnk;
}

int quantum_free_chunk(quantumRoot_t* root, void* chunk) {
	if(!chunk)
		return FALSE;
	nrbtreeNode_t* cnode = cdsl_nrbtreeTop(&root->addr_rbroot);
	quantumNode_t* qnode;

	// find original quantum node by looking up address tree (red black tree)
	while(cnode) {
		qnode = container_of(cnode,quantumNode_t, addr_rbnode);
		if((size_t) chunk < (size_t) qnode) {
			cnode = cdsl_nrbtreeGoLeft(cnode);
		} else if((size_t) chunk > (size_t) qnode->top) {
			cnode = cdsl_nrbtreeGoRight(cnode);
		} else {
			/*
			 *  original quantum node found
			 *  free chunk
			 */
			quantum_node_free_unit(qnode, chunk);
			if(qnode->free_cnt == qnode->qcnt) {
				/*
				 * if quantum node is completely full
				 * and leaf node, we can free it to cache
				 */
				if(!qnode->parent) {
					quantum_t* quantum = (quantum_t*) cdsl_nrbtreeLookup(&root->quantum_tree, (trkey_t) qnode->quantum);
					if (!quantum) {
						fprintf(stderr, "Ivalid Quantum %d\n", qnode->quantum);
						exit(-1);
					}
					quantum = container_of(quantum, quantum_t, qtree_node);
					quantum->entry = NULL;
					return TRUE;
				}

				quantumNode_t* nchild = NULL;
				if(qnode->left && qnode->right) {
					return TRUE;
				} else if(qnode->right) {
					nchild = qnode->right;
					nchild->parent = qnode->parent;
				} else if(qnode->left) {
					nchild = qnode->left;
					nchild->parent = qnode->parent;
				}

				// free quantum node to quantum pool
				// delete linkage from the parent node
				if(qnode->parent->right == qnode) {
					qnode->parent->right = nchild;
				} else if(qnode->parent->left == qnode){
					qnode->parent->left = nchild;
				} else {
					// unexpected linkage from parent node
					fprintf(stderr, "Quantum Corruption happend : Bad Parent Reference");
					exit(-1);
				}

				// delete quantum node from address lookup tree
				cdsl_nrbtreeDelete(&root->addr_rbroot,(trkey_t) qnode);
				wtreeNode_t* free_q = wtree_nodeInit(qnode, (size_t) qnode->top - (size_t) qnode);
				wtree_addNode(&root->quantum_pool,free_q,TRUE);
				return TRUE;
			}
			return TRUE;
		}
	}
	fprintf(stderr, "invalid chunk is freed\n");
	exit(-1);
}



void quantum_purge_cache(quantumRoot_t* root) {
	if(!root)
		return;
	cdsl_nrbtreeTraverse(&root->quantum_tree, for_each_qnode, ORDER_INC);
}


void quantum_print(quantumRoot_t* root) {
	if(!root)
		return;
	printf("\n");
	cdsl_nrbtreeTraverse(&root->quantum_tree, for_each_quantum_print, ORDER_INC);
	printf("\n");
}



static void quantum_init(quantum_t* quantum, uint16_t qsz) {
	if(!quantum)
		return;
	quantum->entry = NULL;
	cdsl_nrbtreeNodeInit(&quantum->qtree_node,qsz);
}


static void quantum_node_init(quantumNode_t* node, uint16_t qsz, ssize_t hsz, BOOL base) {
	if(!node)
		return;
	node->quantum = qsz;
	node->parent = NULL;
	node->qcnt = QUANTUM_COUNT_MAX;
	node->left = node->right = NULL;
	node->top = (void*) (((size_t) node) + qsz * QUANTUM_COUNT_MAX);
	cdsl_nrbtreeNodeInit(&node->addr_rbnode, (trkey_t) node);
	memset(node->map, 0xff, sizeof(qmap_t) *  QMAP_COUNT);
	qmap_t msk = 1;
	qmap_t *map = node->map;

	/*
	 * clear bitmap for occupied area used by header
	 */
	uint16_t shift_cnt = 0;
	size_t boffest = QMAP_UNIT_OFFSET * qsz;
	while(hsz > boffest) {
		hsz -= boffest;
		node->qcnt -= QMAP_UNIT_OFFSET;
		*map = 0;
		map++;
	}

	while((hsz = hsz - qsz) > 0) {
		*map &= ~msk;
		msk <<= 1;
		node->qcnt--;
	}
	node->free_cnt = node->qcnt;

	if(base) {
		/*
		 * base node contains quantum meta data
		 * so it has to be preserved unless the quantum is completely empty
		 */
		node->qcnt++;
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
	quantum->entry = quantum_node_insert_rc(quantum->entry, qnode);
	quantum->entry->parent = NULL;
	cdsl_nrbtreeInsert(&root->addr_rbroot, &qnode->addr_rbnode);
}

static quantumNode_t* quantum_node_insert_rc(quantumNode_t* parent, quantumNode_t* item) {
	if(!parent)
		return item;
	if((size_t)parent < (size_t) item) {
		parent->right = quantum_node_insert_rc(parent->right, item);
		if(!parent->right)
			return parent;
		parent->right->parent = parent;
		if(parent->right->free_cnt > parent->free_cnt) {
			parent = quantum_node_rotate_left(parent);
		}
	} else if((size_t) parent > (size_t) item) {
		parent->left = quantum_node_insert_rc(parent->left, item);
		if(!parent->left)
			return parent;
		parent->left->parent = parent;
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
	top->parent = qnode->parent;

	qnode->left = top->right;
	if(top->right)
		top->right->parent = qnode;

	top->right = qnode;
	qnode->parent = top;
	return top;
}

static quantumNode_t* quantum_node_rotate_left(quantumNode_t* qnode) {
	if(!qnode || !qnode->right)
		return qnode;
	quantumNode_t* top = qnode->right;
	top->parent = qnode->parent;

	qnode->right = top->left;
	if(top->left)
		top->left->parent = qnode;

	top->left = qnode;
	qnode->parent = top;
	return top;
}


static void* quantum_node_alloc_unit(quantumNode_t* qnode) {
	if(!qnode)
		return NULL;
	if(!qnode->free_cnt)
		return NULL;
	qmap_t* cmap = qnode->map;
	uint16_t offset = 0;
	qmap_t  vmap = 0;
	qmap_t  imap = 1;
	while(!*cmap && (offset < QUANTUM_COUNT_MAX)) {
		cmap++;
		offset += QMAP_UNIT_OFFSET;
	}
	vmap = *cmap;
	while(!(vmap & 1)) {
		vmap >>= 1;
		imap <<= 1;
		offset++;
	}
	*cmap &= ~imap;
	qnode->free_cnt--;
	return &((uint8_t*) qnode)[offset * qnode->quantum];
}



static void quantum_node_free_unit(quantumNode_t* qnode, void* chunk) {
	if(!qnode)
		return;
	size_t offset = (size_t)chunk - (size_t)qnode;
	uint16_t bmoffset = offset / qnode->quantum;
	qmap_t* cmap = qnode->map;
	while(!(bmoffset < 64)) {
		cmap++;
		bmoffset -= 64;
	}
	*cmap |= ((qmap_t) 1 << bmoffset);
	qnode->free_cnt++;
}

static void quantum_node_resolve_upward(quantumRoot_t* root, quantumNode_t* parent, quantumNode_t* child, quantumNode_t* child_hint) {
	if(!root)
		return ;
	if(!parent){
		if(child)
			child->parent = NULL;
		return ;
	}
	/*
	 *  1. update child node of parent
	 *  2. if the size of node is not inorder rotate
	 */
	if(child_hint == parent->left) {
		parent->left = child;
	} else if(child_hint == parent->right) {
		parent->right = child;
	}
	if(parent->right && parent->left) {
		if((parent->right->free_cnt > parent->free_cnt) &&
		   (parent->left->free_cnt > parent->free_cnt)) {
			if(parent->right->free_cnt > parent->left->free_cnt) {
				parent = quantum_node_rotate_left(parent);
				quantum_node_resolve_upward(root, parent->parent, parent, parent->left);
			} else {
				parent = quantum_node_rotate_right(parent);
				quantum_node_resolve_upward(root, parent->parent, parent, parent->right);
			}
		} else {
			return ;
		}
	} else if(parent->right) {
		if(parent->right->free_cnt > parent->free_cnt) {
			parent = quantum_node_rotate_left(parent);
			quantum_node_resolve_upward(root, parent->parent, parent, parent->left);
		} else {
			return ;
		}
	} else if(parent->left) {
		if(parent->left->free_cnt > parent->free_cnt) {
			parent = quantum_node_rotate_right(parent);
			quantum_node_resolve_upward(root, parent->parent, parent, parent->right);
		} else {
			return ;
		}
	} else {
		if(parent->free_cnt == parent->qcnt) {
			if(!cdsl_nrbtreeDelete(&root->addr_rbroot, (trkey_t) parent))	{
				fprintf(stderr, "Heap Corrupted : Invalid qnode[%lx] in address tree\n",(size_t) parent);
				exit(-1);
			}
			quantumNode_t* pparent = parent->parent;
			quantum_node_resolve_upward(root, pparent, NULL, parent);
			wtreeNode_t* qpool = wtree_nodeInit(parent,(size_t) parent->top - (size_t) parent);
			wtree_addNode(&root->quantum_pool,qpool, TRUE);
			return ;
		}
		quantum_node_resolve_upward(root, parent->parent, parent, parent);
	}
}


static void quantum_purge(quantum_t* quantum) {

}

static DECLARE_TRAVERSE_CALLBACK(for_each_qnode) {
	if(!node)
		return FALSE;
	quantum_t* quantum = (quantum_t*) container_of(node, quantumNode_t, addr_rbnode);
	quantum_purge(quantum);
	return TRUE;
}


static DECLARE_TRAVERSE_CALLBACK(for_each_quantum_print) {
	if(!node)
		return FALSE;
	quantum_t* qt = container_of(node, quantum_t, qtree_node);
	quantum_node_print_rc(qt->entry, 0);
	return TRUE;
}


static void quantum_node_print_rc(quantumNode_t* qnode, int level) {
	if(!qnode)
		return;
	quantum_node_print_rc(qnode->right, level + 1);
	quantum_node_print(qnode, level);
	quantum_node_print_rc(qnode->left, level + 1);
}

static void quantum_node_print(quantumNode_t* qnode,int level) {
	int l = level;
	while(l--)
		printf("\t");
	printf("@ %lx {Q : %d | usage : %u /  %u } (%u)\n",(size_t) qnode, qnode->quantum, qnode->free_cnt, qnode->qcnt,level );
}



