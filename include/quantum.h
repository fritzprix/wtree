/*
 * quantum.h
 *
 *  Created on: Jun 1, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_QUANTUM_H_
#define INCLUDE_QUANTUM_H_





#ifdef __cplusplus
extern "C" {
#endif


#define QUANTUM_SPACE           ((uint16_t) 2)
#define QUANTUM_MAX             ((uint16_t) 128)
#define MAP_OFFSET              ((uint16_t) 16)



typedef struct {
	nrbtreeNode_t    addr_rbroot;
	nrbtreeNode_t    quantum_tree;
}quantumRoot_t;

extern void quantum_root_init(quantumRoot_t* root);
extern void quantum_add_segment(quantumRoot_t* root, void* addr,size_t sz);
extern void* quantum_reclaim_chunk(quantumRoot_t* root, size_t sz);
extern int quantum_return_chunk(void* chunk);
extern void quantum_purge_cache(quantumRoot_t* root);



#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_QUANTUM_H_ */
