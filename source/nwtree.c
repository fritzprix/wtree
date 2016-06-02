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

static nwtreeNode_t* insert_rc(nwtreeRoot_t* root, nwtreeNode_t* parent, nwtreeNode_t* item, BOOL compact);
static nwtreeNode_t* grows_node(nwtreeNode_t* parent, nwtreeNode_t** grown, uint32_t nsz);
static nwtreeNode_t* purge_rc(nwtreeRoot_t* root, nwtreeNode_t* node);
static void iterbase_rc(nwtreeNode_t* node, nwt_callback_t callback, void* arg);
static size_t size_rc(nwtreeNode_t* node);
static size_t fsize_rc(nwtreeNode_t* node);
static size_t count_rc(nwtreeNode_t* node);
static uint32_t level_rc(nwtreeNode_t* node);
static nwtreeNode_t* rotate_left(nwtreeNode_t* parent);
static nwtreeNode_t* rotate_right(nwtreeNode_t* parent);
static nwtreeNode_t* resolve(nwtreeRoot_t* root, nwtreeNode_t* parent, BOOL compact);
static nwtreeNode_t* merge_next(nwtreeNode_t* merger);
static nwtreeNode_t* merge_prev(nwtreeNode_t* merger);
static nwtreeNode_t* merge_from_leftend(nwtreeNode_t* left, nwtreeNode_t* merger);
static nwtreeNode_t* merge_from_rightend(nwtreeNode_t* right,nwtreeNode_t* merger);
static void print_rc(nwtreeNode_t* parent, int depth);
static void print_tab(int times);

void nwtree_rootInit(nwtreeRoot_t* root, nwt_unmap_func_t unmap_call) {
	if (!root)
		return;
	root->entry = NULL;
	root->unmap = unmap_call;
	root->sz = 0;
	root->used_sz = 0;
}


void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz) {
	if (!node)
		return;
	node->left = node->right = NULL;
	node->base = addr;
	node->size = sz;
	node->base_size = 0;
}

/*
 *  base node contains information about memory segment which is allocated from system (mmap)
 *  base_size is original size of segment and barely changes. base nodes are also kept within tree
 *  before the segment is explicitly freed
 */
void nwtree_baseNodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz) {
	if (!node)
		return;
	node->left = node->right = NULL;
	node->base = addr;
	node->base_size = node->size = sz;
}

void nwtree_purge(nwtreeRoot_t* root) {
	if (!root)
		return;
	root->entry = purge_rc(root, root->entry);
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

void nwtree_addNode(nwtreeRoot_t* root, nwtreeNode_t* node, BOOL compact) {
	if (!root)
		return;
	root->entry = insert_rc(root, root->entry, node, compact);
}

void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz, BOOL compact) {
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
	root->entry = resolve(root, root->entry, compact);
	return chunk;
}

void* nwtree_grow_chunk(nwtreeRoot_t* root, nwtreeNode_t** node, uint32_t nsz) {
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
	root->entry = grows_node(root->entry, node, nsz);
	if (*node)
		return *node;
	return nwtree_reclaim_chunk(root, nsz, TRUE);
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

static nwtreeNode_t* resolve(nwtreeRoot_t* root, nwtreeNode_t* parent, BOOL compact) {
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
				root->unmap(parent->base, parent->base_size);
				return NULL;
			}
		}
	}
	return parent;
}

static nwtreeNode_t* merge_next(nwtreeNode_t* merger) {
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
	 */
	if(!merger)
		return NULL;
	if(!merger->right)
		return merger;
	merger->right = merge_next(merger->right);
	merger->right = merge_from_leftend(merger->right, merger);
	return merger;
}

static nwtreeNode_t* merge_prev(nwtreeNode_t* merger) {
	if(!merger)
		return NULL;
	if(!merger->left)
		return merger;
	merger->left = merge_prev(merger->left);
	merger->left = merge_from_rightend(merger->left, merger);
	nwtreeNode_t* node = (nwtreeNode_t*) merger->base;
	node->base = merger->base;
	node->base_size = merger->base_size;
	node->size = merger->size;
	node->left = merger->left;
	node->right = merger->right;
	return node;
}

static nwtreeNode_t* merge_from_leftend(nwtreeNode_t* left, nwtreeNode_t* merger) {
	if(!left)
		return NULL;
	left->left = merge_from_leftend(left->left, merger);
	if(left->left)
		return left;
	while((merger->base + merger->size) == left->base) {
		if(merger->base_size)
			merger->base_size += left->base_size;
		else if(left->base_size)
			return left;
		merger->size += left->size;
		if(!left->right)
			return NULL;
		left = left->right;
	}
	return left;
}

static nwtreeNode_t* merge_from_rightend(nwtreeNode_t* right,nwtreeNode_t* merger) {
	/*
	 *          merger                   merger                merger
	 *           /  \                     /  \                 /   \
	 *        l(nm)             -->    l(nm)           -->  l(nm)       -->  stop
	 *         / \                      / \                 /  \
	 *       ll  lr(nm)               ll  lr(nm)           ll  lr(nm)
	 *             \                      /  \                 /  \
	 *             lrl(m)                   lrll(m)               lrlll(nm)
	 *            /  \                       /  \
	 *          lrll  0               lrlll(nm)  0
	 */
	if(!right)
		return NULL;
	right->right = merge_from_rightend(right->right, merger);
	if(right->right)
		return right;
	while((right->base + right->size) == merger->base) {
		if(right->base_size)
			merger->base_size += right->base_size;
		else if(merger->base_size)
			return right;
		merger->size += right->size;
		merger->base = right->base;
		if(!right->left)
			return NULL;
		right = right->left;
	}
	return right;
}


static nwtreeNode_t* insert_rc(nwtreeRoot_t* root, nwtreeNode_t* parent, nwtreeNode_t* item, BOOL compact) {
	if (!parent)
		return item;
	if (parent->base < item->base) {
		if (parent->base + parent->size == item->base) {
			/*
			 *  if       | parent  ============== | item ==== |
			 *  then     | parent  ========================== |
			 */
			if (parent->base_size) {
				parent->base_size += item->base_size;
			}
			else if(item->base_size) {
				/*
				 *  can't be merged
				 */
				parent->right = insert_rc(root,parent->right, item,compact);
				parent = merge_next(parent);
				parent = resolve(root,parent,compact);
				return parent;
			}
			parent->size += item->size;
		}else {
			parent->right = insert_rc(root,parent->right, item,compact);
			parent = merge_next(parent);
			parent = resolve(root,parent,compact);
		}
		return parent;
	} else {
		if (item->base + item->size == parent->base) {
			if (item->base_size)
				item->base_size += parent->base_size;
			else if(parent->base_size) {
				parent->left = insert_rc(root, parent->left, item,  compact);
				parent = merge_prev(parent);
				parent = resolve(root, parent,compact);
				return parent;
			}
			item->size += parent->size;
			item->left = parent->left;
			item->right = parent->right;
			parent = item;
		} else {
			parent->left = insert_rc(root, parent->left, item,  compact);
			parent = merge_prev(parent);
			parent = resolve(root, parent,compact);
		}
		return parent;
	}
}

static nwtreeNode_t* grows_node(nwtreeNode_t* parent, nwtreeNode_t** grown, uint32_t nsz) {
	if (!parent) {
		*grown = NULL;
		return NULL;
	}
	if (parent->size == 0) {
		*grown = NULL;
		return parent;
	}
	if((*grown)->base > (parent->base + parent->base_size)) {
		parent->right = grows_node(parent->right, grown, nsz);
	} else if((*grown)->base < parent->base) {
		parent->left = grows_node(parent->left, grown, nsz);
	} else {
		if(((nsz - (*grown)->size + sizeof(nwtreeNode_t)) < parent->size) && (!parent->base_size)) {
			(*grown)->size = nsz;
			parent->size -= (nsz - (*grown)->size);
			nwtreeNode_t* node = (nwtreeNode_t*) ((size_t) (*grown)->base + (*grown)->size);
			memcpy(node, parent, sizeof(nwtreeNode_t));
			return node;
		} else {
			if(parent->base_size) {
				(*grown)->right = parent;
			} else {
				(*grown)->size += parent->size;
				(*grown)->left = parent->left;
				(*grown)->right = parent->right;
			}
			parent = *grown;
		}
	}
	*grown = NULL;
	return parent;
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

static nwtreeNode_t* purge_rc(nwtreeRoot_t* root, nwtreeNode_t* node) {
	if (!node)
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
		if (node->left && (node->left->base_size != node->left->size)) {
			node = rotate_right(node);
			node->right = purge_rc(root, node->right);
		} else if (node->right
				&& (node->right->base_size != node->right->size)) {
			node = rotate_left(node);
			node->left = purge_rc(root, node->left);
		} else if (!node->left && !node->right) {
			if(root->unmap) {
				root->unmap(node->base, node->base_size);
				return NULL;
			}
			return node;
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

