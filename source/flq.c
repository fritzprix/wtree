/*
 * flq.c
 *
 *  Created on: 2016. 11. 16.
 *      Author: innocentevil
 */

#include "flq.h"
#include "cdsl_nrbtree.h"
#include "cdsl_slist.h"

#ifndef FLQ_NODE_MIN_SIZE
#define FLQ_NODE_MIN_SIZE    ((size_t) 4)
#endif

static void flq_add_node(flqRoot_t* root, flqNode_t* node);



void flq_rootInit(flqRoot_t* root, const flqAdapter_t* adapter) {
	if(!root) {
		return;
	}
	root->adapter = adapter;
	cdsl_nrbtreeRootInit(&root->addr_tree);
	cdsl_nrbtreeRootInit(&root->q_tree);
}

void flq_nodeInit(flqNode_t* node, size_t address, size_t init_alc_pos,size_t sz, uint32_t qsz) {
	if(!node || !address || !sz) {
		return;
	}
	trkey_t qkey = 0;
	node->free_sz = node->sz = sz;
	node->base_addr = address;
	node->alc_pos = init_alc_pos;

	while((qsz >> qkey) > 0) qkey++;

	cdsl_nrbtreeNodeInit(&node->addr_rbn, address);
	cdsl_nrbtreeNodeInit(&node->q_rbn, qkey);
	cdsl_slistEntryInit(&node->free_lhead);
}


void* flq_allocate(flqRoot_t* root, size_t qsz) {
	if(!root || (qsz < FLQ_NODE_MIN_SIZE)) {
		return NULL;
	}

	trkey_t key = 0;
	while((qsz >> key) > 0) key++;
	flqNode_t* qnode = cdsl_nrbtreeLookup(&root->q_tree, key);
	if(!qnode) {
		if(!root->adapter->on_new_node) {
			return NULL;
		}
		qnode = root->adapter->on_new_node(root, qsz);
		if(!qnode) {
			return NULL;
		}

		flq_add_node(root, qnode);
	} else {
		qnode = container_of(qnode, flqNode_t, q_rbn);
	}

	slistNode_t* chunk_head = NULL;
	if(cdsl_slistIsEmpty(&qnode->free_lhead)){
		chunk_head = cdsl_slistDequeue(&qnode->free_lhead);
		return (void*) chunk_head;
	} else if(qnode->alc_pos >= qnode->sz) {
	}
	void* chunk = (void*) (qnode->base_addr + qnode->alc_pos);
	return chunk;
}

void flq_free(flqRoot_t* root, void* ptr) {

}


static void flq_add_node(flqRoot_t* root, flqNode_t* node) {

}

