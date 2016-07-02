/*!
 * bin.h
 * bin is process-wide bounded cache which relieve contention in unmap request from multiple thread
 * bin has hierarchical structure consist of two elements, bin root and bin cache
 * bin root is entry point for the bin cache and bin cache is actual cache component which is shared among
 * the threads.
 *
 *  Created on: Jul 2, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_BIN_H_
#define INCLUDE_BIN_H_


#include <pthread.h>
#include "wtree.h"
#include "cdsl_nrbtree.h"


#ifdef __cplusplus
extern "C" {
#endif


#ifndef BIN_SPREAD_FACTOR
#define BIN_SPREAD_FACTOR     8
#endif

typedef struct bin_root binRoot_t;

typedef struct {
	binRoot_t*         root;
	pthread_mutex_t    lock;
	wtreeRoot_t        bin_cache;
	int                ref_cnt;
} binCache_t;

struct bin_root {
	size_t             cache_max_sz;
	wt_unmap_func_t    unmapper;
	binCache_t         caches[BIN_SPREAD_FACTOR];
	void*              ext_ctx;
};



extern void bin_root_init(binRoot_t* bin, size_t max_sz, wt_unmap_func_t unmapper,void* ext_ctx);
extern binCache_t* bin_cache_bind(binRoot_t* bin, trkey_t key);
extern void bin_cache_unbind(binRoot_t* bin, binCache_t* cache);
extern void bin_root_dispose(binRoot_t* bin, binCache_t* cache, void* addr, size_t sz);
extern void* bin_root_recycle(binRoot_t* bin, binCache_t* cache, size_t sz);
extern void bin_purge(binRoot_t* bin);
extern void bin_cleanup(binRoot_t* bin);



#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BIN_H_ */
