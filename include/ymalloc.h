/*
 * ymalloc.h
 *
 *  Created on: Mar 30, 2016
 *      Author: innocentevil
 */

#ifndef INCLUDE_YMALLOC_H_
#define INCLUDE_YMALLOC_H_


#ifdef __cplusplus
extern "C" {
#endif


extern void* ymalloc(size_t);
extern void yfree(void*);


#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_YMALLOC_H_ */
