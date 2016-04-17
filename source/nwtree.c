/*
 * nwtree.c
 *
 *  Created on: Apr 16, 2016
 *      Author: innocentevil
 */




#include "nwtree.h"
#include <stddef.h>


static nwtreeNode_t* insert_rc(nwtreeNode_t* next,nwtreeNode_t* item,uint8_t* ctx);
static nwtreeNode_t* resolve_left(nwtreeNode_t* );
static nwtreeNode_t* resolve_right(nwtreeNode_t* );
static nwtreeNode_t* rotate_left(nwtreeNode_t* parent);
static nwtreeNode_t* rotate_right(nwtreeNode_t* parent);
static nwtreeNode_t* lookup_chunk_by_size_rc(nwtreeNode_t* next, uint8_t* ctx);


void nwtree_rootInit(nwtreeRoot_t* root)
{
	if(!root)
		return;
	root->free_entry = root->occupied_entry = NULL;
	root->sz = 0;
}

void nwtree_nodeInit(nwtreeNode_t* node, uaddr_t addr, uint32_t sz)
{
	if(!node)
		return;
	node->base = addr;
	node->size = sz;
	node->right = node->left = NULL;
}

void nwtree_addNode(wtreeRoot_t* root, nwtreeNode_t* node)
{
	if(!root)
		return;
	uint8_t ctx;
	root->entry = insert_rc(root->entry, node, &ctx);
}



void* nwtree_reclaim_chunk(nwtreeRoot_t* root, uint32_t sz)
{
	if(!root || !sz)
		return NULL;
	sz += sizeof(nwtreeNode_t);
	if(root->free_entry->size < sz)
		return NULL;

}

int nwtree_free_chunk(nwtreeRoot_t* root, void* chnk)
{

}

nwtreeNode_t* nwtree_delete(nwtreeRoot_t* )
{

}

void nwtree_print(nwtreeRoot_t* root)
{

}


static nwtreeNode_t* insert_rc(nwtreeNode_t* next,nwtreeNode_t* item,uint8_t* ctx)
{
	if(!next)
		return item;
	if(!next->size)
	{
		/*
		 * if this node has zero size, it means it should be deleted
		 */
		return item;
	}
	if(next->base > item->base)
	{
		next->left = insert_rc(next->left, item, ctx);
		next = resolve_left(next);
		return next;
	}
	else if((next->base + next->size) < item->base)
	{
		next->right = insert_rc(next->right, item, ctx);
		next = resolve_right(next);
		return next;
	}
		/*
		 *  this is shouldn't be
		 *  raise error or something
		 */
}

static nwtreeNode_t* resolve_left(nwtreeNode_t* parent)
{
	if(!parent->left)
		return parent;
	if(parent->left->size > parent->size)
	{
		/*
		 *  if left is larger than parent
		 *      p              l               p           lr
		 *     / \    -->     / \    -->      / \     --> /  \       --> keep this to downward
		 *    l   r          ll  p           lr  r       lrl  p
		 *                      / \         / \              / \
		 *                     lr  r      lrl lrr          lrr  r
		 *
		 */
		parent = rotate_right(parent);
		parent->right = resolve_left(parent->right);
	}

	if((parent->left->base + parent->left->size) == parent->base)
	{
		parent->base = parent->left->base;
		parent->size += parent->left->size;
		parent->left->size = 0;
	}
	if((parent->base + parent->size) == parent->right->base)
	{
		parent->right->base += parent->right->size;
		parent->size += parent->right->size;
		parent->right->size = 0;
	}
	return parent;
}

static nwtreeNode_t* resolve_right(nwtreeNode_t* parent)
{
	if(!parent->right)
		return parent;
	if(parent->right->size > parent->size)
	{
		/*
		 * if right is larger than parent
		 *    p             r             p               rl
		 *   / \    -->    / \   -->     / \       -->   /  \
		 *  l   r         p   rr        l  rl           p   rlr
		 *     / \       / \               / \         / \
		 *    rl  rr   l   rl            rll  rlr     l  rll
		 */
		parent = rotate_left(parent);
		parent->left = resolve_right(parent->left);
	}
	if((parent->left->base + parent->left->size) == parent->base)
	{
		parent->base = parent->left->base;
		parent->size += parent->left->size;
		parent->left->size = 0;
	}
	if((parent->base + parent->size) == parent->right->base)
	{
		parent->right->base += parent->right->size;
		parent->size += parent->right->size;
		parent->right->size = 0;
	}
	return parent;
}

static nwtreeNode_t* lookup_chunk_by_size_rc(nwtreeNode_t* next, uint8_t* ctx)
{

}


static nwtreeNode_t* rotate_left(nwtreeNode_t* parent)
{

}

static nwtreeNode_t* rotate_right(nwtreeNode_t* parent)
{

}
