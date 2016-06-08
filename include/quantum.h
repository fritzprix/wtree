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

typedef struct quantum_node quantumNode_t;

struct quantum_node {
	quantumNode_t   *left,*right;
	uint32_t         free_cnt;
	uint16_t         quantum;
	nrbtreeNode_t    addr_rbnode;
	uint64_t         map[32];
};

typedef struct {
	nrbtreeNode_t    addr_rbroot;
	nrbtreeNode_t    quantum_tree;
}quantumRoot_t;

extern void quantum_rootInit(quantumRoot_t* root);


extern void nwtQ_rootInit(nwtQRoot_t* root);
extern void nwtQ_addSegment(nwtQRoot_t* root, void* addr, size_t sz);
extern void* nwtQ_reclaim_chunk(nwtQRoot_t* root, size_t sz);
extern int nwtQ_return_chunk(void* chunk);
extern void nwtQ_purgeCache();



#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_QUANTUM_H_ */
