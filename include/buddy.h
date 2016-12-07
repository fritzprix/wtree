/*
 * buddy.h
 *
 *  Created on: 2016. 11. 26.
 *      Author: innocentevil
 */

#ifndef INCLUDE_BUDDY_H_
#define INCLUDE_BUDDY_H_

#include "cdsl_slist.h"
#include "cdsl_nrbtree.h"

#ifndef BUDDY_MAX_ORDER
#define BUDDY_MAX_ORDER     6
#endif

#ifndef BUDDY_MIN_ORDER
#define BUDDY_MIN_ORDER     2
#endif

#define BUDDY_MAX_SIZE      (1 << BUDDY_MAX_ORDER)
#define BUDDY_MIN_SIZE      (sizeof(slistNode_t))


#ifdef __cplusplus
extern "C" {
#endif

/**
 * buddy allocator
 *
 */

typedef struct buddy_root {
	size_t         total_sz;
	size_t         free_sz;
	wt_adapter    *adapter;
	slistEntry_t   bucket[BUDDY_MAX_ORDER];
} buddyRoot;


void buddy_root_init(buddyRoot* root, wt_adapter* adapter);
void* buddy_reclaim_chunk(buddyRoot* root, size_t sz);
int buddy_free_chunk(buddyRoot* root, void* ptr, size_t sz);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BUDDY_H_ */
