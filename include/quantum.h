/*
 * quantum.h
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_QUANTUM_H_
#define INCLUDE_QUANTUM_H_


#include "wtree.h"
#include "cdsl_nrbtree.h"

#ifdef __cplusplus
extern "C" {
#endif


#define QUANTUM_SPACE           ((uint16_t) 2)
#define QUANTUM_MAX             ((uint16_t) 48)
#define QUANTUM_COUNT_MAX		((uint16_t) 2048)


typedef struct {
	nrbtreeRoot_t    addr_rbroot;
	nrbtreeRoot_t    quantum_tree;
	wtreeRoot_t      quantum_pool;
	wt_map_func_t    mapper;
	wt_unmap_func_t  unmapper;
}quantumRoot_t;

extern void quantum_root_init(quantumRoot_t* root,wt_map_func_t mapper, wt_unmap_func_t unmapper);
extern void quantum_add_segment(quantumRoot_t* root, void* addr,size_t sz);
extern void* quantum_reclaim_chunk(quantumRoot_t* root, size_t sz);
extern int quantum_free_chunk(quantumRoot_t* root,void* chunk);
extern void quantum_purge_cache(quantumRoot_t* root);



#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_QUANTUM_H_ */
