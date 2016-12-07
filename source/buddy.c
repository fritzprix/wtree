/*
 * buddy.c
 *
 *  Created on: 2016. 11. 26.
 *      Author: innocentevil
 */



#include "buddy.h"


static void* buddy_split(buddyRoot* root, void* chunk, int fromOrder);

void buddy_root_init(buddyRoot* root, wt_adapter* adapter) {
	if(!root || !adapter) {
		return;
	}
	int i;
	root->adapter = adapter;
	for(i = 0; i < BUDDY_MAX_ORDER; i++) {
		cdsl_slistEntryInit(&root->bucket[i]);
	}
	root->free_sz = 0;
	root->total_sz = 0;
}

void* buddy_reclaim_chunk(buddyRoot* root, size_t sz) {
	if(!root) {
		return NULL;
	}
	if((sz < BUDDY_MIN_SIZE) || (sz > BUDDY_MAX_SIZE)) {
		return NULL;
	}
	void* chunk;
	int cur_order ,order = BUDDY_MIN_ORDER;
	while(!((1 << order) > sz))  order++;
	cur_order = order;
	while(!cdsl_slistIsEmpty(&root->bucket[cur_order])) {
		cur_order++;
		if(cur_order == BUDDY_MAX_ORDER) {
			size_t asz;
			chunk = root->adapter->onallocate(BUDDY_MAX_SIZE, &asz, NULL);
			while(cur_order != order) {
				chunk = buddy_split(root, chunk, cur_order--);
			}
			return chunk;
		}
	}
	return (void*) cdsl_slistRemoveHead(&root->bucket[order]);
}

int buddy_free_chunk(buddyRoot* root, void* ptr, size_t sz) {
	if(!root) {
		return -1;
	}
	if((sz < BUDDY_MIN_SIZE) || (sz > BUDDY_MAX_SIZE)) {
		return -1;
	}
	int order = BUDDY_MIN_ORDER;
	while(!((1 << order)) > sz)  order++;

}

static void* buddy_split(buddyRoot* root, void* chunk, int fromOrder) {
	size_t splt_sz = (1 << (fromOrder - 1));
	cdsl_slistNodeInit(chunk);
	cdsl_slistPutHead(&root->bucket[fromOrder - 1], chunk);
	chunk = (void*) ((size_t) chunk + splt_sz);
	return chunk;
}

