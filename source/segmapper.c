/*
 * segmapper.c
 *
 *  Created on: Jun 23, 2016
 *      Author: innocentevil
 */

#include "segmapper.h"




void segmapper_init(segmentMapper_t* mapper) {

}

void segmapper_add_cache(segmentMapper_t* mapper, trkey_t cache_id, segmentCache_t* segment_cache) {

}

BOOL segmapper_is_from_cache(segmentMapper_t* mapper, trkey_t cache_id, void* addr) {
	if(!mapper || !addr)
		return FALSE;
}


void segmapper_map(trkey_t cache_id, size_t sz) {

}

void segmapper_unmap(trkey_t cache_id, void* addr, size_t sz) {

}

void segmapper_purge_target_cache(segmentMapper_t* mapper, trkey_t cache_id) {

}

void segmapper_purge_caches(segmentMapper_t* mapper) {

}

void segmapper_cleanup(segmentMapper_t* mapper) {

}
