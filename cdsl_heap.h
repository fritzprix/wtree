/*
 * heap.h
 *
 *  Created on: 2015. 5. 3.
 *      Author: innocentevil
 */

#ifndef CDSL_HEAP_H_
#define CDSL_HEAP_H_

#if defined(__cplusplus)
extern "C" {
#endif




#include <stdint-gcc.h>
typedef struct heap_node heapNode_t;
typedef heapNode_t* (*heapEvaluate)(heapNode_t* a, heapNode_t* b);
typedef void (*heapPrint)(heapNode_t* item);

struct heap_node{
	heapNode_t*	right;
	heapNode_t*	left;
	uint8_t 	flipper;
};

extern int cdsl_heapEnqueue(heapNode_t** heap,heapNode_t* item,heapEvaluate eval);
extern heapNode_t* cdsl_heapDeqeue(heapNode_t** heap,heapEvaluate eval);
extern void cdsl_heapPrint(heapNode_t** heap,heapPrint prt);

#if defined(__cplusplus)
}
#endif

#endif /* HEAP_H_ */
