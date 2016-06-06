/*
 * wtree.c
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#include "wtree.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cdsl_slist.h"

static wtreeNode_t* insert_rc(wtreeRoot_t* root, wtreeNode_t* parent, wtreeNode_t* item, BOOL compact);
static wtreeNode_t* grows_node(wtreeNode_t* parent, wtreeNode_t** grown, uint32_t nsz);
static wtreeNode_t* purge_rc(wtreeRoot_t* root, wtreeNode_t* node);
static void iterbase_rc(wtreeNode_t* node, wt_callback_t callback, void* arg);
static size_t size_rc(wtreeNode_t* node);
static size_t fsize_rc(wtreeNode_t* node);
static size_t count_rc(wtreeNode_t* node);
static uint32_t level_rc(wtreeNode_t* node);
static wtreeNode_t* rotate_left(wtreeNode_t* parent);
static wtreeNode_t* rotate_right(wtreeNode_t* parent);
static wtreeNode_t* resolve(wtreeRoot_t* root, wtreeNode_t* parent, BOOL compact);
static wtreeNode_t* merge_next(wtreeNode_t* merger);
static wtreeNode_t* merge_prev(wtreeNode_t* merger);
static wtreeNode_t* merge_from_leftend(wtreeNode_t* left, wtreeNode_t* merger);
static wtreeNode_t* merge_from_rightend(wtreeNode_t* right,wtreeNode_t* merger);
static void print_rc(wtreeNode_t* parent, int depth);
static void print_tab(int times);

void wtree_rootInit(wtreeRoot_t* root,wt_unmap_func_t unmap_func) {
	if(!root || !unmap_func)
		return;
	root->entry = NULL;
	root->sz = root->used_sz = 0;
	root->unmap = unmap_func;
}

wtreeNode_t* wtree_nodeInit(uaddr_t addr, uint32_t sz) {
	if(!addr || !sz)
		return NULL;
	uint8_t* chunk = (uint8_t*) addr;
	wtreeNode_t* node = (wtreeNode_t*) &chunk[sz - sizeof(wtreeNode_t)];
	node->size = sz;
	node->base_size = 0;
	node->left = node->right = NULL;
	node->top = (uaddr_t) &chunk[sz];
	return node;
}

wtreeNode_t* wtree_baseNodeInit(uaddr_t addr, uint32_t sz) {
	if(!addr || !sz)
		return NULL;
	uint8_t* chunk = (uint8_t*) addr;
	wtreeNode_t* node = (wtreeNode_t*) &chunk[sz - sizeof(wtreeNode_t)];
	node->size = node->base_size = sz;
	node->left = node->right = NULL;
	node->top = (uaddr_t) &chunk[sz];
	return node;
}

void wtree_purge(wtreeRoot_t* root) {
	if(!root)
		return;
	root->entry = purge_rc(root,root->entry);
}

void wtree_iterBaseNode(wtreeRoot_t* root, wt_callback_t callback, void* arg) {
	if(!root)
		return;
	iterbase_rc(root->entry, callback, arg);
}

void wtree_addNode(wtreeRoot_t* root, wtreeNode_t* node, BOOL compact) {
	if(!root || !node)
		return;
	wtreeNode_t cache;
	memcpy(&cache, node, sizeof(wtreeNode_t));
	root->entry = insert_rc(root,root->entry,node,compact);
}


void* wtree_reclaim_chunk(wtreeRoot_t* root, uint32_t sz,BOOL compact) {
	if (!root || (sz <= 0))
		return NULL;
	if (!root->entry)
		return NULL;
	wtreeNode_t* largest = root->entry;
	uint8_t* chunk = (uint8_t*) largest->top;
	if((largest->size - sizeof(wtreeNode_t)) < sz)
		return NULL;
	chunk = chunk - largest->size;
	largest->size -= sz;
	root->entry = resolve(root, root->entry, compact);
	return chunk;
}

void* wtree_grow_chunk(wtreeRoot_t* root, wtreeNode_t* node, uint32_t nsz) {
	if(!root || !node || !nsz)
		return NULL;
	if(!root->entry)
		return NULL;
	uint8_t* chunk = (uint8_t*) (node->top - node->size);
	wtreeNode_t* cur = root->entry;
	int32_t dsz = nsz - node->size;
	if(dsz < 0)
		return chunk;

	while(cur) {
		if(cur->top < node->top) {
			cur = cur->right;
		} else if((cur->top - cur->size) > node->top) {
			cur = cur->left;
		} else {
			// origin node is found
			chunk = &chunk[(int) (cur->top - cur->size)];
			cur->size -= dsz;
			return chunk;
		}
	}
	return NULL;
}

void wtree_print(wtreeRoot_t* root) {
	if(!root)
		return;
	if(!root->entry)
		return;
	printf("\n");
	print_rc(root->entry, 0);
	printf("\n");
}

uint32_t wtree_level(wtreeRoot_t* root) {
	if(!root)
		return 0;
	if(!root->entry)
		return 0;
	return level_rc(root->entry);
}

size_t wtree_nodeCount(wtreeRoot_t* root) {
	if(!root)
		return 0;
	if(!root->entry)
		return 0;
	return count_rc(root->entry);
}

size_t wtree_totalSize(wtreeRoot_t* root) {
	if(!root)
		return 0;
	if(!root->entry)
		return 0;
	return size_rc(root->entry);
}

size_t wtree_freeSize(wtreeRoot_t* root) {
	if(!root)
		return 0;
	if(!root->entry)
		return 0;
	return fsize_rc(root->entry);
}

static wtreeNode_t* insert_rc(wtreeRoot_t* root, wtreeNode_t* parent, wtreeNode_t* item, BOOL compact) {
	if(!root || !item)
		return parent;
	if(!parent)
		return item;
	if(parent->top < item->top) {
		if((item->top - item->size) == parent->top) {
			if(item->base_size) {
				item->base_size += parent->base_size;
			}
			else if(parent->base_size) {
				parent->right = insert_rc(root, parent->right, item, compact);
				parent = merge_next(parent);
				parent = resolve(root, parent, compact);
				return parent;
			}
			item->size += parent->size;
			item->left = parent->left;
			item->right = parent->right;
			parent = item;
		} else {
			parent->right = insert_rc(root, parent->right, item, compact);
			parent = merge_next(parent);
			parent = resolve(root, parent, compact);
		}
		return parent;
	} else {
		if((parent->top - parent->size) == item->top) {
			if(parent->base_size) {
				parent->base_size += item->base_size;
			}
			else if(item->base_size) {
				parent->left = insert_rc(root, parent->left, item, compact);
				parent = merge_prev(parent);
				parent = resolve(root, parent, compact);
				return parent;
			}
			parent->size += item->size;
		} else {
			parent->left = insert_rc(root, parent->left, item, compact);
			parent = merge_prev(parent);
			parent = resolve(root, parent, compact);
			return parent;
		}
		return parent;
	}
}

static wtreeNode_t* grows_node(wtreeNode_t* parent, wtreeNode_t** grown, uint32_t nsz) {
	if(!parent) {
		grown = NULL;
		return NULL;
	}
	if((*grown)->top == (parent->top - parent->size)) {
		if((parent->size + sizeof(wtreeNode_t)) > (nsz - (*grown)->size)) {
			parent->size -= (nsz - (*grown)->size);
			return parent;
		}
		// return requested node to its base node or neighbor
		parent->size += (*grown)->size;
		grown = NULL;
		return parent;
	} else if((*grown)->top > parent->top) {
		parent->right = grows_node(parent->right, grown, nsz);
	} else {
		parent->left = grows_node(parent->left, grown, nsz);
	}
	return parent;

}


static wtreeNode_t* purge_rc(wtreeRoot_t* root, wtreeNode_t* node) {
	if(!root)
		return NULL;
	if(node->right) {
		node->right = purge_rc(root, node->right);
		node = merge_next(node);
	}
	if(node->left) {
		node->left = purge_rc(root, node->left);
		node = merge_prev(node);
	}
	if (node->size == node->base_size) {
		if(node->left && (node->left->base_size != node->left->size)) {
			node = rotate_right(node);
			node->right = purge_rc(root, node->right);
		} else if(node->right && (node->right->base_size != node->right->size)) {
			node = rotate_left(node);
			node->left = purge_rc(root, node->left);
		} else if(!node->left && !node->right) {
			if(root->unmap) {
				root->unmap(node->top - node->base_size, node->base_size);
				return NULL;
			}
		}
	}
	return node;
}

static void iterbase_rc(wtreeNode_t* node, wt_callback_t callback, void* arg) {
	if (!node)
		return;
	iterbase_rc(node->right, callback, arg);
	if (node->base_size)
		callback(node, arg);
	iterbase_rc(node->left, callback, arg);
}

static size_t size_rc(wtreeNode_t* node) {
	if (!node)
		return 0;
	size_t sz = size_rc(node->left);
	sz += node->base_size;
	sz += size_rc(node->right);
	return sz;
}

static size_t fsize_rc(wtreeNode_t* node) {
	if (!node)
		return 0;
	size_t sz = fsize_rc(node->left);
	sz += node->size;
	sz += fsize_rc(node->right);
	return sz;
}

static size_t count_rc(wtreeNode_t* node) {
	if (!node)
		return 0;
	if (!node->right && !node->left)
		return 1;
	if (node->right && node->left)
		return count_rc(node->left) + count_rc(node->right) + 1;
	if (!node->right)
		return count_rc(node->left) + 1;
	if (!node->left)
		return count_rc(node->right) + 1;
	return 0;
}

static uint32_t level_rc(wtreeNode_t* node) {
	if (!node)
		return 0;
	uint32_t l, r = level_rc(node->right) + 1;
	l = level_rc(node->left) + 1;
	return r > l ? r : l;
}

static wtreeNode_t* rotate_left(wtreeNode_t* parent) {
	wtreeNode_t* nparent = parent->right;
	parent->right = nparent->left;
	nparent->left = parent;
	return nparent;
}

static wtreeNode_t* rotate_right(wtreeNode_t* parent) {
	wtreeNode_t* nparent = parent->left;
	parent->left = nparent->right;
	nparent->right = parent;
	return nparent;
}

static wtreeNode_t* resolve(wtreeRoot_t* root, wtreeNode_t* parent, BOOL compact) {
	if (!parent)
		return NULL;
	if (parent->right && parent->left) {
		if (!(parent->right->size > parent->size)
				&& !(parent->left->size > parent->size))
			return parent;
		if (parent->right->size > parent->left->size) {
			parent = rotate_left(parent);
			parent->left = resolve(root,parent->left,compact);
			parent = merge_prev(parent);
		} else {
			parent = rotate_right(parent);
			parent->right = resolve(root,parent->right,compact);
			parent = merge_next(parent);
		}
	} else if (parent->right) {
		if (parent->right->size > parent->size) {
			parent = rotate_left(parent);
			parent->left = resolve(root,parent->left,compact);
			parent = merge_prev(parent);
		}
	} else if (parent->left) {
		if (parent->left->size > parent->size) {
			parent = rotate_right(parent);
			parent->right = resolve(root, parent->right,compact);
			parent = merge_next(parent);
		}
	} else {
		if (!parent->size && !parent->base_size){
			return NULL;
		}else if(parent->size == parent->base_size) {
			if(root->unmap && compact) {
				root->unmap(parent->top - parent->base_size, parent->base_size);
				return NULL;
			}
		}
	}
	return parent;
}

static wtreeNode_t* merge_next(wtreeNode_t* merger) {
	/*
	 *    merger                merger                   merger
	 *        \                     \                       \
	 *         r (nm)     -->        r (nm)     ->           r (nm)   ->    stop
	 *        /                     /                       /
	 *      rl (nm)               rl (nm)                 rl (m)
	 *      /                     /                       /  \
	 *    rll (m)               rllr (m)           rllrr(nm)  rlr
	 *    / \                  /    \
	 *   0  rllr              0    rllrr

	 *
	 */
	if(!merger)
		return NULL;
	if(!merger->right)
		return merger;
	merger->right = merge_next(merger->right);
	merger->right = merge_from_leftend(merger->right, merger);
	wtreeNode_t* node = (wtreeNode_t*) (merger->top - sizeof(wtreeNode_t));
	node->top = merger->top;
	node->base_size = merger->base_size;
	node->size = merger->size;
	node->left = merger->left;
	node->right = merger->right;
	return node;
}

static wtreeNode_t* merge_prev(wtreeNode_t* merger) {
	if(!merger)
		return NULL;
	if(!merger->left)
		return merger;
	merger->left = merge_prev(merger->left);
	merger->left = merge_from_rightend(merger->left, merger);
	return merger;
}


static wtreeNode_t* merge_from_leftend(wtreeNode_t* left, wtreeNode_t* merger) {
	/*
	 *   |--------------|header 1|
	 *                                          |-----------|header 2|
	 *                           |-----|header 3|
	 */
	if(!left)
		return NULL;
	left->left = merge_from_leftend(left->left, merger);
	if(left->left)
		return left;
	while((left->top - left->size) == merger->top) {
		if(left->base_size) {
			merger->base_size += left->base_size;
		}
		else if(merger->base_size) {
			return left;
		}
		merger->size += left->size;
		merger->top = left->top;
		if(!left->right)
			return NULL;
		left = left->right;
	}
	return left;
}


static wtreeNode_t* merge_from_rightend(wtreeNode_t* right,wtreeNode_t* merger) {
	/*
	 *                                   |------------|header 1|
	 *    |-----|header 2|
	 *                   |------|header 3|
	 */
	if(!right)
		return NULL;
	right->right = merge_from_rightend(right->right, merger);
	if(right->right)
		return right;
	while((merger->top - merger->size) == right->top) {
		if(merger->base_size){
			merger->base_size += right->base_size;
		}
		else if(right->base_size) {
			return right;
		}
		merger->size += right->size;
		if(!right->left)
			return NULL;
		right = right->left;
	}
	return right;
}

static void print_rc(wtreeNode_t* parent, int depth) {
	if (!parent)
		return;
	print_rc(parent->right, depth + 1);
	print_tab(depth);
	printf("Node@%lx (bottom : %lx / base :%lx) {size : %u / base_size : %u / top : %lx} (%d)\n",
			(uint64_t) parent, (uint64_t)parent->top - parent->size, (uint64_t)parent->top - parent->base_size, parent->size, parent->base_size, (uint64_t) parent->top, depth);
	print_rc(parent->left, depth + 1);
}

static void print_tab(int times) {
	while (times--)
		printf("\t");
}
