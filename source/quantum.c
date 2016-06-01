/*
 * quantum.c
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#include "quantum.h"


void nwtQ_rootInit(nwtQRoot_t* root) {
	if(!root)
		return;
}

void nwtQ_addSegment(nwtQRoot_t* root, void* addr, size_t sz) {

}

void* nwtQ_reclaim_chunk(nwtQRoot_t* root, size_t sz) {

}

int nwtQ_return_chunk(void* chunk) {

}

void nwtQ_purgeCache() {

}
