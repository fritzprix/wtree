/*
 * flq.h
 *
 *  Created on: 2016. 11. 15.
 *      Author: innocentevil
 */

#ifndef INCLUDE_FLQ_H_
#define INCLUDE_FLQ_H_

#include "cdsl_slist.h"
#include "cdsl_nrbtree.h"

/*
 *  free list based quantized allocator
 *
 */

#ifdef __cplusplus
extern "C" {
#endif


typedef struct flq_root flqRoot_t;
typedef struct flq_node flqNode_t;

typedef flqNode_t* (*flq_on_new_cache_node)(flqRoot_t* root, size_t qsz);
typedef void (*flq_on_cache_node_full)(flqRoot_t* root, flqNode_t* full);

typedef struct flq_adapter{
	flq_on_new_cache_node   on_new_node;
	flq_on_cache_node_full  on_node_full;
} flqAdapter_t;

struct flq_root {
	nrbtreeRoot_t       addr_tree;
	nrbtreeRoot_t       q_tree;
	flqAdapter_t*       adapter;
};

struct flq_node {
	nrbtreeNode_t       addr_rbn;
	nrbtreeNode_t       q_rbn;
	uint16_t            alc_pos;
	uint16_t            base_addr;
	uint16_t            sz;
	uint16_t            free_sz;
	slistEntry_t        free_lhead;
};



extern void flq_rootInit(flqRoot_t* root, const flqAdapter_t* adapter);
/**
 *
 */
extern void flq_nodeInit(flqNode_t* node, size_t address, size_t init_alc_pos, size_t sz);
extern void* flq_allocate(flqRoot_t* root, size_t qsz);
extern void flq_free(flqRoot_t* root, void* ptr);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_FLQ_H_ */
