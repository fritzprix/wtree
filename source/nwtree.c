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
static nwtreeNode_t* purge_rc(nwtreeNode_t* node, purge_func_t callback,BOOL force);
static size_t size_rc(nwtreeNode_t* node);
static size_t fsize_rc(nwtreeNode_t* node);
static nwtreeNode_t* rotate_left(nwtreeNode_t* parent);
static nwtreeNode_t* rotate_right(nwtreeNode_t* parent);
static nwtreeNode_t* resolve_left(nwtreeNode_t* parent);
static nwtreeNode_t* resolve_right(nwtreeNode_t* parent);
static nwtreeNode_t* resolve(nwtreeNode_t* parent);
static nwtreeNode_t* merge_left(nwtreeNode_t* parent);
static nwtreeNode_t* merge_right(nwtreeNode_t* parent);
static void print_rc(nwtreeNode_t* parent,int depth);
static void print_tab(int times);


void nwtree_rootInit(nwtreeRoot_t* root)
{
	if(!root)
		return;
	root->entry = NULL;
	root->sz = 0;
	root->used_sz = 0;
}

/*
 * sz is size of memory chunk in bytes
 *
 */
void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz)
{
	if(!node)
		return;
	node->left = node->right = NULL;
	node->base = addr;
	node->size = sz;
}

void nwtree_baseNodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz)
{
	if(!node)
		return;
	node->left = node->right = NULL;
	node->base = addr;
	node->base_size = node->size = sz;
}

void nwtree_purgeAll(nwtreeRoot_t* root, purge_func_t callback)
{
	if(!root)
		return;
	root->entry = purge_rc(root->entry, callback, TRUE);
}

void nwtree_purge(nwtreeRoot_t* root, purge_func_t callback)
{
	if(!root)
		return;
	root->entry = purge_rc(root->entry, callback, FALSE);
}

size_t nwtree_totalSize(nwtreeRoot_t* root)
{
	if(!root)
		return 0;
	return size_rc(root->entry);
}

size_t nwtree_freeSize(nwtreeRoot_t* root)
{
	if(!root)
		return 0;
	return fsize_rc(root->entry);
}



void nwtree_addNode(nwtreeRoot_t* root, nwtreeNode_t* node)
{
	if(!root)
		return;
	root->entry = insert_rc(root->entry, node);
}

void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz)
{
	if(!root)
		return NULL;
	if(!root->entry)
		return NULL;
	nwtreeNode_t* largest = root->entry;
	uint8_t* chunk = (uint8_t*) largest->base;
	if((largest->size - sizeof(nwtreeNode_t)) < sz)
		return NULL;
	largest->size -= sz;
	chunk = &chunk[largest->size];
	root->entry = resolve(root->entry);
	return chunk;
}

void nwtree_print(nwtreeRoot_t* root)
{
	if(!root)
		return;
	printf("\n");
	print_rc(root->entry,0);
	printf("\n");
}

static void print_tab(int times)
{
	while(times--) printf("\t");
}


static void print_rc(nwtreeNode_t* parent,int depth)
{
	if(!parent)
		return;
	print_rc(parent->right, depth + 1);
	print_tab(depth);
	printf("Node@%lu {size : %u / base_size : %u }\n",(uint64_t)parent->base, parent->size, parent->base_size);
	print_rc(parent->left,depth + 1);
}


static nwtreeNode_t* rotate_left(nwtreeNode_t* parent)
{
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

static nwtreeNode_t* rotate_right(nwtreeNode_t* parent)
{
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

static nwtreeNode_t* resolve_left(nwtreeNode_t* parent)
{
	if(!parent->left)
		return parent;
	if(parent->size < parent->left->size)
	{
		parent = rotate_right(parent);
		return resolve_left(parent);
	}
	return parent;
}

static nwtreeNode_t* resolve_right(nwtreeNode_t* parent)
{
	if(!parent->right)
		return parent;
	if(parent->size < parent->right->size)
	{
		parent = rotate_left(parent);
		return resolve_right(parent);
	}
	return parent;
}

static nwtreeNode_t* resolve(nwtreeNode_t* parent)
{
	if(!parent)
		return NULL;
	if(parent->right && (parent->right->size > parent->size))
	{
		parent = rotate_left(parent);
		parent->left = resolve(parent->left);
	}
	if(parent->left && (parent->left->size > parent->size))
	{
		parent = rotate_right(parent);
		parent->right = resolve(parent->right);
	}
	if(!parent->left && !parent->right && (parent->size == 0))
		return NULL;
	return parent;

}


static nwtreeNode_t* merge_left(nwtreeNode_t* merger)
{
	/**
	 *     merger(base : 512 / size : 1024 / free_sz : 512)                   merger(base : 0 / size : 1536 / free_sz :  640)
	 *     /                                     --->                         /
	 *  mergee(base : 0 / size : 512 / free_sz  : 128)                     mergee(base:  0 / size : 0 / free_sz : 0)
	 *
	 */
	if(!merger)
		return NULL;
	if(!merger->left)
		return merger;
	if(merger->size == 0)
		return resolve(merger);
	if((merger->left->base + merger->left->size) == merger->base)
	{
		merger = rotate_right(merger);
		merger = merge_right(merger);
	}
	return merger;
}

static nwtreeNode_t* merge_right(nwtreeNode_t* merger)
{
	/**
	 *     merger(base : 512 / size : 1024)                merger(base : 512 / size : 1536)
	 *         \                                 --->           \
	 *        mergee(base : 1536 / size : 512)                  mergee(base : 1536 / size : 0)
	 *
	 */
	if(!merger)
		return NULL;
	if(!merger->right)
		return merger;
	if(merger->size == 0)
		return resolve(merger);
	if((merger->base + merger->size) == merger->right->base)
	{
		merger->size += merger->right->size;
		if(merger->right->base_size)
			merger->base_size += merger->right->base_size;
		merger->right->size = 0;
		merger->right = resolve(merger->right);
	}
	return merger;
}


static nwtreeNode_t* insert_rc(nwtreeNode_t* parent, nwtreeNode_t* item)
{
	if(!parent)
		return item;
	if(parent->base < item->base)
	{
		parent->right = insert_rc(parent->right,item);
		return resolve(merge_right(parent));
	}
	else
	{
		parent->left = insert_rc(parent->left,item);
		return resolve(merge_left(parent));
	}
}

static nwtreeNode_t* purge_rc(nwtreeNode_t* node, purge_func_t callback, BOOL force)
{
	/**
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
	if(!node)
		return NULL;
	if(node->left)
	{
		node->left = purge_rc(node->left, callback, force);
		node = merge_left(node);
	}
	if(node->right)
	{
		node->right = purge_rc(node->right, callback, force);
		node = merge_right(node);
	}
	if(!node->base_size && !node->size)
	{
		if(node->left)
		{
			node = rotate_right(node);
		}
		else if(node->right)
		{
			node = rotate_left(node);
		}
		else
		{
			return NULL;
		}
	}
	if((node->size == node->base_size) || force)
	{
		if(node->left && (node->left->base_size != node->left->size))
		{
			node = rotate_right(node);
			node->right = purge_rc(node->right, callback, force);
		}
		else if(node->right && (node->right->base_size != node->right->size))
		{
			node = rotate_left(node);
			node->left = purge_rc(node->left, callback, force);
		}
		else if(!node->left && !node->right)
		{
			callback(node);
			return NULL;
		}
	}
	return node;
}


static size_t size_rc(nwtreeNode_t* node)
{
	if(!node)
		return 0;

	size_t sz = size_rc(node->left);
	sz += node->base_size;
	sz += size_rc(node->right);
	return sz;
}


static size_t fsize_rc(nwtreeNode_t* node)
{
	if(!node)
		return 0;
	size_t sz = fsize_rc(node->left);
	sz += node->size;
	sz += fsize_rc(node->right);
	return sz;
}

