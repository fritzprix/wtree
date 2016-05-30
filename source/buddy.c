/*
 * nwtbuddy.c
 *
 *  Created on: May 29, 2016
 *      Author: innocentevil
 */


#include "buddy.h"
#include "cdsl_nrbtree.h"
#include <stdio.h>
#include <stdlib.h>

#define BUDDY_TOTAL_COUNT       128


typedef struct {
	nrbtreeNode_t      rbnode;
	nwtBuddyNode_t*    entry;
}nwtBuddyGranulRoot_t;


static void* nwtbuddy_reclaim_chunk_from_node(nwtBuddyNode_t* node);
static void nwtbuddy_granRootInit(nwtBuddyGranulRoot_t* root,uint16_t gran);
static nwtBuddyNode_t* insert_rc(nwtBuddyNode_t* cur, nwtBuddyNode_t* item);
static nwtBuddyNode_t* rotate_left(nwtBuddyNode_t* node);
static nwtBuddyNode_t* rotate_right(nwtBuddyNode_t* node);


void nwtbuddy_rootInit(nwtBuddyRoot_t* root) {
	if(!root)
		return;
	cdsl_nrbtreeRootInit(&root->glb_rbroot);
	cdsl_nrbtreeRootInit(&root->gran_tree);
}

void nwtbuddy_nodeInit(nwtBuddyNode_t* node, void* addr, size_t sz) {
	if(!node)
		return;
	uint16_t gran = GRAN_MAX;   // set maximum granularity
	while(sz < (gran * 128)) gran >>= 1;

	uint16_t hdr_occp = sizeof(nwtBuddyNode_t) / gran;
	if(sizeof(nwtBuddyNode_t) % gran)
		hdr_occp++;

	node->gran = gran;
	node->free_cnt = sz / gran;

	cdsl_nrbtreeNodeInit(&node->glb_rbnode, (trkey_t) addr);
	node->left = node->right = NULL;
	node->map[0] = node->map[1] = -1;
	uint64_t hdr_occp_msk = 0;

	node->free_cnt -= hdr_occp;
	while(hdr_occp--) {
		hdr_occp_msk <<= 1;
		hdr_occp_msk |= 1;
	}
	node->map[0] &= ~hdr_occp_msk;
}

void nwtbuddy_addNode(nwtBuddyRoot_t* root, nwtBuddyNode_t* node) {
	if(!root || !node)
		return;
	nwtBuddyGranulRoot_t* gran_root = (nwtBuddyGranulRoot_t*) cdsl_nrbtreeLookup(&root->gran_tree, node->gran);
	if(!gran_root) {
		gran_root = nwtbuddy_reclaim_chunk_from_node(node);
		if(!gran_root) {
			fprintf(stderr, "can't create buddy granular[%d] root\n",node->gran);
			exit(-1);
		}
		nwtbuddy_granRootInit(gran_root,node->gran);
		cdsl_nrbtreeInsert(&root->gran_tree, &gran_root->rbnode);
	}
	gran_root->entry = insert_rc(gran_root->entry, node);
}

void* nwtbuddy_reclaim_chunk(nwtBuddyRoot_t* root, size_t sz) {
	return NULL;
}


int nwtbuddy_return_chunk(void* chunk) {
	return 0;
}

static nwtBuddyNode_t* insert_rc(nwtBuddyNode_t* cur, nwtBuddyNode_t* item) {
	if(!cur)
		return item;
	if((uint64_t) cur < (uint64_t) item) {
		cur->right = insert_rc(cur->right, item);
		if(cur->right->free_cnt > cur->free_cnt)
			cur = rotate_left(cur);
	}else {
		cur->left = insert_rc(cur->left, item);
		if(cur->left->free_cnt > cur->free_cnt)
			cur = rotate_right(cur);
	}
	return cur;
}



static nwtBuddyNode_t* rotate_left(nwtBuddyNode_t* node) {
	if(!node)	return NULL;
	nwtBuddyNode_t* np = node->left;
	node->left = np->right;
	np->right = node;
	return np;

}

static nwtBuddyNode_t* rotate_right(nwtBuddyNode_t* node) {
	if(!node)	return NULL;
	nwtBuddyNode_t* np = node->right;
	node->right = np->left;
	np->left = node;
	return np;
}


static void* nwtbuddy_reclaim_chunk_from_node(nwtBuddyNode_t* node) {
	if(!node)
		return NULL;
	if(!node->free_cnt)
		return NULL;
	uint16_t* map = node->map;
	uint8_t shift_cnt, offset = 0;
	uint16_t mask = 1;
	while(!~map[offset]) {
		offset++;
	}
	shift_cnt = 0;
	while(!(map[offset] & mask)) {
		shift_cnt++;
		mask <<= 1;
	}
	map[offset] &= ~mask;
	uint8_t* base = (uint8_t*) node;
	base += (((offset * MAP_OFFSET) + shift_cnt) * node->gran);
	return base;
}

static void nwtbuddy_granRootInit(nwtBuddyGranulRoot_t* root, uint16_t gran) {

}

