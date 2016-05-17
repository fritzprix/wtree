/*
 * nwtree.c
 *
 *  Created on: Apr 16, 2016
 *      Author: innocentevil
 */

#include "nwtree.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "cdsl_slist.h"

static nwtreeNode_t* insert_rc(nwtreeNode_t* parent, nwtreeNode_t* item);
static nwtreeNode_t* grows_node(nwtreeNode_t* parent, nwtreeNode_t** grown, uint32_t nsz);
static nwtreeNode_t* purge_rc(nwtreeNode_t* node, nwt_callback_t callback, void* arg);
static void iterbase_rc(nwtreeNode_t* node, nwt_callback_t callback, void* arg);
static size_t size_rc(nwtreeNode_t* node);
static size_t fsize_rc(nwtreeNode_t* node);
static size_t count_rc(nwtreeNode_t* node);
static uint32_t level_rc(nwtreeNode_t* node);
static nwtreeNode_t* rotate_left(nwtreeNode_t* parent);
static nwtreeNode_t* rotate_right(nwtreeNode_t* parent);
static nwtreeNode_t* resolve(nwtreeNode_t* parent);
static nwtreeNode_t* merge_left(nwtreeNode_t* parent);
static nwtreeNode_t* merge_right(nwtreeNode_t* parent);
static void print_rc(nwtreeNode_t* parent, int depth);
static void print_tab(int times);

void nwtree_rootInit(nwtreeRoot_t* root) {
	if (!root)
		return;
	root->entry = NULL;
	root->sz = 0;
	root->used_sz = 0;
}

/*
 * sz is size of memory chunk in bytes
 *
 */
void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz) {
	if (!node)
		return;
	node->left = node->right = NULL;
	node->base = addr;
	node->size = sz;
	node->base_size = 0;
}

void nwtree_baseNodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz) {
	if (!node)
		return;
	node->left = node->right = NULL;
	node->base = addr;
	node->base_size = node->size = sz;
}

void nwtree_purge(nwtreeRoot_t* root, nwt_callback_t callback, void* arg) {
	if (!root)
		return;
	root->entry = purge_rc(root->entry, callback, arg);
}

void nwtree_iterBaseNode(nwtreeRoot_t* root, nwt_callback_t callback, void* arg) {
	if (!root)
		return;
	iterbase_rc(root->entry, callback, arg);
}

size_t nwtree_totalSize(nwtreeRoot_t* root) {
	if (!root)
		return 0;
	return size_rc(root->entry);
}

uint32_t nwtree_level(nwtreeRoot_t* root) {
	if (!root)
		return 0;
	return level_rc(root->entry);
}

size_t nwtree_nodeCount(nwtreeRoot_t* root) {
	if (!root)
		return 0;
	return count_rc(root->entry);
}

size_t nwtree_freeSize(nwtreeRoot_t* root) {
	if (!root)
		return 0;
	return fsize_rc(root->entry);
}

void nwtree_addNode(nwtreeRoot_t* root, nwtreeNode_t* node) {
	if (!root)
		return;
	root->entry = insert_rc(root->entry, node);
}

void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz) {
	if (!root || (sz <= 0))
		return NULL;
	if (!root->entry)
		return NULL;
	nwtreeNode_t* largest = root->entry;
	uint8_t* chunk = (uint8_t*) largest->base;
	if ((largest->size - sizeof(nwtreeNode_t)) < sz)
		return NULL;
	largest->size -= sz;
	chunk = &chunk[largest->size];
	root->entry = resolve(root->entry);
	return chunk;
}

void* nwtree_grow_chunk(nwtreeRoot_t* root, nwtreeNode_t* node, uint32_t nsz) {
	/*
	 *  grow provides way to increase chunk without copy when contiguous memory is available.
	 *  otherwise, it will return new avaiable chunk. caller should compare return value to
	 *  check whether it is unchanged or not. if it has been changed new chunk caller has to
	 *  copy if it's required.
	 */
	if (!root || (nsz <= 0))
		return NULL;
	if (!root->entry)
		return NULL;
	root->entry = grows_node(root->entry, &node, nsz);
	if (node)
		return node;
	return nwtree_reclaim_chunk(root, nsz);
}

void nwtree_print(nwtreeRoot_t* root) {
	if (!root)
		return;
	printf("\n");
	print_rc(root->entry, 0);
	printf("\n");
}

static void print_tab(int times) {
	while (times--)
		printf("\t");
}

static void print_rc(nwtreeNode_t* parent, int depth) {
	if (!parent)
		return;
	print_rc(parent->right, depth + 1);
	print_tab(depth);
	printf("Node@%lu {size : %u / base_size : %u } (%d)\n",
			(uint64_t) parent->base, parent->size, parent->base_size, depth);
	print_rc(parent->left, depth + 1);
}

static nwtreeNode_t* rotate_left(nwtreeNode_t* parent) {
	/*
	 *     p             r
	 *    / \           / \
	 *   l   r         p   rr
	 *      / \       / \
	 *     rl  rr    l  rl
	 */
	nwtreeNode_t* nparent = parent->right;
	parent->right = nparent->left;
	nparent->left = parent;
	return nparent;
}

static nwtreeNode_t* rotate_right(nwtreeNode_t* parent) {
	/*
	 *        p             l
	 *       / \           / \
	 *      l   r         ll  p
	 *     / \               / \
	 *    ll lr             lr  r
	 */
	nwtreeNode_t* nparent = parent->left;
	parent->left = nparent->right;
	nparent->right = parent;
	return nparent;
}

static nwtreeNode_t* resolve(nwtreeNode_t* parent) {
	if (!parent)
		return NULL;
	if (parent->right && parent->left) {
		if (!(parent->right->size > parent->size)
				&& !(parent->left->size > parent->size))
			return parent;
		if (parent->right->size > parent->left->size) {
			parent = rotate_left(parent);
			parent->left = resolve(parent->left);
			parent = merge_left(parent);
		} else {
			parent = rotate_right(parent);
			parent->right = resolve(parent->right);
			parent = merge_right(parent);
		}
	} else if (parent->right) {
		if (parent->right->size > parent->size) {
			parent = rotate_left(parent);
			parent->left = resolve(parent->left);
			parent = merge_left(parent);
		}
	} else if (parent->left) {
		if (parent->left->size > parent->size) {
			parent = rotate_right(parent);
			parent->right = resolve(parent->right);
			parent = merge_right(parent);
		}
	} else {
		if (!parent->size && !parent->base_size){
			return NULL;
		}
	}
	return parent;
}

static nwtreeNode_t* merge_left(nwtreeNode_t* merger) {
	/**
	 *     merger(base : 512 / size : 1024 / free_sz : 512)                   merger(base : 0 / size : 1536 / free_sz :  640)
	 *     /                                     --->                         /
	 *  mergee(base : 0 / size : 512 / free_sz  : 128)                     mergee(base:  0 / size : 0 / free_sz : 0)
	 *
	 */
	if (!merger)
		return NULL;
	if (!merger->left)
		return merger;
	if (merger->size == 0) {
		if (!merger->left && !merger->right)
			return NULL;
		return resolve(merger);
	}
	if ((merger->left->base + merger->left->size) == merger->base) {
		merger = rotate_right(merger);
//		merger = merge_right(merger);
		if (merger->base_size) {
			merger->base_size += merger->right->base_size;
		} else if (merger->right->base_size) {
			// not able to merge
			return merger;
		}
		merger->size += merger->right->size;
		merger->right->size = 0;
		merger->right = resolve(merger->right);
	}
	return merger;
}

static nwtreeNode_t* merge_right(nwtreeNode_t* merger) {
	/**
	 *     merger(base : 512 / size : 1024)                merger(base : 512 / size : 1536)
	 *         \                                 --->           \
	 *        mergee(base : 1536 / size : 512)                  mergee(base : 1536 / size : 0)
	 *
	 */
	if (!merger)
		return NULL;
	if (!merger->right)
		return merger;
	if (merger->size == 0) {
		if (!merger->left && !merger->right)
			return NULL;
		return resolve(merger);
	}
	if (((merger->base + merger->size) == merger->right->base)) {
		if (merger->base_size) {
			merger->base_size += merger->right->base_size;
		} else if (merger->right->base_size) {
			// not able to merge
			return merger;
		}
		merger->size += merger->right->size;
		merger->right->size = 0;
		merger->right = resolve(merger->right);
	}
	return merger;
}


static nwtreeNode_t* insert_rc(nwtreeNode_t* parent, nwtreeNode_t* item) {
	if (!parent)
		return item;
	if (parent->base < item->base) {
		if (parent->base + parent->size == item->base) {
			if (parent->base_size)
				parent->base_size += item->base_size;
			parent->size += item->size;
		}else {
			parent->right = insert_rc(parent->right, item);
			parent = merge_right(parent);
			if (parent->right && (parent->right->size > parent->size)) {
				parent = rotate_left(parent);
			}
		}
		return parent;
	} else {
		if (item->base + item->size == parent->base) {
			if (item->base_size)
				item->base_size += parent->base_size;
			item->size += parent->size;
			item->left = parent->left;
			item->right = parent->right;
			parent = item;
		} else {
			parent->left = insert_rc(parent->left, item);
			parent = merge_left(parent);
			if (parent->left && (parent->left->size > parent->size)) {
				parent = rotate_right(parent);
			}
		}
		return parent;
	}
}

static nwtreeNode_t* grows_node(nwtreeNode_t* parent, nwtreeNode_t** grown,
		uint32_t nsz) {
	if (!parent)
		return NULL;
	if (parent->size == 0)
		return NULL;
	return NULL;
}

/*
 *   CAUTION : force option is not recomended in normal case. because it assumes any allocated chunk will not
 *             be referenced and after this operation any allocated pointer can become dangled.
 *             force option is intended to be used only at thread cleanup stage instead of normal operation.
 *
 *   basic stratedgy is push purgeable node down so that it can be easily removed from the tree
 *      node (base_size == size)  -> purgeable  (p)
 *      node (base_size != size)  -> non purgeable (np)
 *      node (purgeable && (node->left == (purgeable or null)) && (node->right == (purgeable or null)))  -> removable
 *   by rotating subtree(s) following below rules, purgeable node can be removable node.
 *
 *
 *   case i.
 *          p(p)                               r(np)
 *         /   \      -- rotate left -->       /   \
 *      l(p) r(np)                          p(p)    rr
 *            /  \                          /  \
 *          rl    rr                      l(p) rl
 *
 *   case ii.
 *          p(p)                               l(np)
 *         /   \      -- rotate right -->      /   \
 *      l(np)  r(p)                           ll   p(p)
 *       /  \                                      /  \
 *     ll   lr                                    lr  r(p)
 *
 *   case iii.
 *          p(p)                               r(np)                                    r(np)
 *         /   \      -- rotate left -->       /   \    -- rotate right (pivot p) -->   /   \
 *     l(np)   r(np)                        p(p)    rr                               l(np)   rr
 *      /  \    /  \                        /  \                                      /  \
 *    ll   lr  rl   rr                   l(np) rl                                    ll  p(p)
 *                                       /  \                                            /  \
 *                                      ll  lr                                          lr  rl
 *
 *   case iv.
 *            p(p or np)                       p(p or np)
 *           /   \     -- purge child -->      /    \
 *  [l(p) or 0]  [r(p) or 0]                  0      0
 *
 */

static nwtreeNode_t* purge_rc(nwtreeNode_t* node, nwt_callback_t callback,
		void* arg) {
	if (!node)
		return NULL;
	if (node->right) {
		node->right = purge_rc(node->right, callback, arg);
		node = merge_right(node);
	}
	if (node->left) {
		node->left = purge_rc(node->left, callback, arg);
		node = merge_left(node);
	}
	if (node->size == node->base_size) {
		if (node->left && (node->left->base_size != node->left->size)) {
			node = rotate_right(node);
			node->right = purge_rc(node->right, callback, arg);
		} else if (node->right
				&& (node->right->base_size != node->right->size)) {
			node = rotate_left(node);
			node->left = purge_rc(node->left, callback, arg);
		} else if (!node->left && !node->right) {
			callback(node, arg);
			return NULL;
		}
	}
	return node;
}


static void iterbase_rc(nwtreeNode_t* node, nwt_callback_t callback, void* arg) {
	if (!node)
		return;
	iterbase_rc(node->right, callback, arg);
	if (node->base_size)
		callback(node, arg);
	iterbase_rc(node->left, callback, arg);
}

static size_t size_rc(nwtreeNode_t* node) {
	if (!node)
		return 0;

	size_t sz = size_rc(node->left);
	sz += node->base_size;
	sz += size_rc(node->right);
	return sz;
}

static size_t fsize_rc(nwtreeNode_t* node) {
	if (!node)
		return 0;
	size_t sz = fsize_rc(node->left);
	sz += node->size;
	sz += fsize_rc(node->right);
	return sz;
}

static size_t count_rc(nwtreeNode_t* node) {
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

static uint32_t level_rc(nwtreeNode_t* node) {
	if (!node)
		return 0;
	uint32_t l, r = level_rc(node->right) + 1;
	l = level_rc(node->left) + 1;
	return r > l ? r : l;
}

