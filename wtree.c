/*
 * wtree.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */
#include "wtree.h"
#include <stddef.h>


static wtreeNode_t null_node = {
	.left = NULL,
	.right = NULL,
	.base = 0,
	.span = 0
};

#define NULL_NODE   &null_node


static wtreeNode_t* insert_r(wtreeNode_t* current,wtreeNode_t* item,uint32_t gap);
static wtreeNode_t* rotateLeft(wtreeNode_t* rot_pivot);
static wtreeNode_t* rotateRight(wtreeNode_t* rot_pivot);
static wtreeNode_t* swapDown_r(wtreeNode_t* current,wtreeNode_t* next);	//return to
static void print_r(wtreeNode_t* node,int depth);
static void print_tab(int k);

void wtreeRootInit(wtreeRoot_t* root,uint32_t ext_gap){
	root->entry = NULL_NODE;
	root->ext_gap = ext_gap;
}

void wtreeNodeInit(wtreeNode_t* node,uint64_t base,uint64_t span){
	node->left = node->right = NULL_NODE;
	node->base = base;
	node->span = span;
}


void wtreeInsert(wtreeRoot_t* root,wtreeNode_t* item){
	if(!root)
		return;
	root->entry = insert_r(root->entry,item,root->ext_gap);
}

wtreeNode_t* wtreeRetrive(wtreeRoot_t* root,uint64_t span){
	if(!root || !root->entry)
		return NULL;
	if(root->entry->span < span)
		return NULL;

	uint64_t nspan;
	wtreeNode_t* retrived = NULL;
	if(root->entry->span >= (span + sizeof(wtreeNode_t))){
		// build up new chunk
		retrived = root->entry;
		nspan = retrived->span - span;
		root->entry->span = span;
		root->entry = (wtreeNode_t*)(((uint8_t*) root->entry) + span);
		wtreeNodeInit(root->entry,root->entry,nspan);
		root->entry->left = retrived->left;
		root->entry->right = retrived->right;
	}else if(root->entry->span >= span){
		retrived = root->entry;
	}else{
		return NULL;
	}
	root->entry = swapDown_r(NULL_NODE,root->entry);
	null_node.left = null_node.right = NULL;
	if(root->entry == NULL_NODE)
		return retrived;
	if((root->entry->span >= root->entry->right->span) && root->entry->span >= root->entry->left->span)
		return retrived;
	return retrived;
}


void wtreePrint(wtreeRoot_t* root){
	print_r(root->entry,0);
}


static wtreeNode_t* insert_r(wtreeNode_t* current,wtreeNode_t* item,uint32_t gap){
	if(current == NULL_NODE)
		return item;
	if(current->base < item->base){
		current->right = insert_r(current->right,item,gap);
		if(current->base + current->span + gap == current->right->base){
			// if two condecutive chunk is mergeable, then merge them
			current->span += current->right->span + gap;
			current->right = NULL;
			return current;
		}
			// if not mergeable, compare two consecutive size
		if(current->span < current->right->span){
			return rotateLeft(current);
		}
	}else{
		current->left = insert_r(current->left,item,gap);
		if(current->left->base + current->left->span + gap == current->base){
			// if two condecutive chunk is mergeable, then merge them
			current->base = current->left->base;
			current->span += current->left->span + gap;
			current->left = NULL;
			return current;
		}
		if(current->span < current->left->span){
			return rotateRight(current);
		}
	}
	return current;
}

static void print_r(wtreeNode_t* node,int depth){
	if(node == NULL_NODE)
		return;
	print_r(node->left,depth + 1);
	print_tab(depth);printf("{base : %d , span : %d} @ depth %d\n",node->base,node->span,depth);
	print_r(node->right,depth + 1);
}

static void print_tab(int k){
	while(k--)printf("\t");
}

static wtreeNode_t* rotateLeft(wtreeNode_t* rot_pivot){
	uint8_t color;
	wtreeNode_t* nparent = rot_pivot->right;
	rot_pivot->right = nparent->left;
	nparent->left = rot_pivot;
	return nparent;
}

static wtreeNode_t* rotateRight(wtreeNode_t* rot_pivot){
	uint8_t color;
	wtreeNode_t* nparent = rot_pivot->left;
	rot_pivot->left = nparent->right;
	nparent->right = rot_pivot;
	return nparent;
}
/*
 * 	*root = swap_r(null_node,*root);
 *
 */
static wtreeNode_t* swapDown_r(wtreeNode_t* current,wtreeNode_t* next){
	if(next == NULL_NODE)
		return current;
	wtreeNode_t *l,*r;
	r = next->right;
	l = next->left;
	if(next == current->right){
		next->right = current;
		next->left = current->left;
		current->right = r;
		current->left = l;
	}
	else if(next == current->left){
		next->right = current->right;
		next->left = current;
		current->right = r;
		current->left = l;
	}else {
		next->right = current->right;
		next->left = current->left;
		current->right = r;
		current->left = l;
	}

	if((current->right->span > current->span) || (current->left->span > current->span)){
		if(current->right->span > current->left->span){
			current->right = swapDown_r(current,current->right);
		}else{
			current->left = swapDown_r(current,current->left);
		}
	}
	return current;
}

