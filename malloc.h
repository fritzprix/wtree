/*
 * malloc.h
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */

#ifndef MALLOC_H_
#define MALLOC_H_

#include <stddef.h>

extern void wtreeHeap_init(void* addr,size_t size);
extern void * wtreeHeap_malloc(size_t sz);
extern void wtreeHeap_free(void* ptr);
extern void wtreeHeap_print();


#endif /* MALLOC_H_ */
