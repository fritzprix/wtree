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

struct chunk_hdr {
	uint32_t      prev_sz;
	uint32_t      sz;
};

static nwtreeNode_t* insert_rc(nwtreeNode_t* parent, nwtreeNode_t* item);
static nwtreeNode_t* rotate_left(nwtreeNode_t* parent);
static nwtreeNode_t* rotate_right(nwtreeNode_t* parent);
static nwtreeNode_t* resolve_left(nwtreeNode_t* parent);
static nwtreeNode_t* resolve_right(nwtreeNode_t* parent);
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

void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz)
{
	if(!node)
		return;
	node->base = addr;
	node->left = node->right = NULL;
	node->size = node->free_sz = sz;
}

void nwtree_addNode(nwtreeRoot_t* root, nwtreeNode_t* node)
{
	if(!root)
		return;
	root->entry = insert_rc(root->entry, node);
}

void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz)
{

}

int nwtree_free_chunk(nwtreeRoot_t* root, void* chnk)
{

}

nwtreeNode_t* nwtree_delete(nwtreeRoot_t* root)
{

}

void nwtree_print(nwtreeRoot_t* root)
{
	if(!root)
		return;
	print_rc(root->entry,0);
}

static void print_tab(int times)
{
	while(times--) printf("\t");
}


static void print_rc(nwtreeNode_t* parent,int depth)
{
	if(!parent)
		return;
	print_rc(parent->left,depth + 1);
	print_tab(depth);
	printf("Node@%lu {size : %u}\n",(uint64_t)parent->base, parent->size);
	print_rc(parent->right, depth + 1);
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
	nparent->left = parent;
	return nparent;
}

static nwtreeNode_t* resolve_left(nwtreeNode_t* parent)
{
	if(!parent->left)
		return parent;
	if(parent->free_sz < parent->left->free_sz)
	{
		return rotate_right(parent);
	}
	return parent;
}

static nwtreeNode_t* resolve_right(nwtreeNode_t* parent)
{
	if(!parent->right)
		return parent;
	if(parent->free_sz < parent->right->free_sz)
	{
		return rotate_left(parent);
	}
	return parent;
}

static nwtreeNode_t* insert_rc(nwtreeNode_t* parent, nwtreeNode_t* item)
{
	if(!parent)
		return item;
	if(parent->base < item->base)
	{
		parent->right = insert_rc(parent->right,item);
		return resolve_right(parent);
	}
	else
	{
		parent->left = insert_rc(parent->left,item);
		return resolve_left(parent);
	}
}

