/*
 * wtree.h
 *
 *  Created on: Sep 5, 2016
 *      Author: innocentevil
 */

#ifndef WT_H_
#define WT_H_

#include "autogen.h"


#ifdef BAREMETAL
#define PRINT(...)
#define PRINT_ERR(...)
#define EXIT(e)
#else
#include <stdio.h>
#include <stdlib.h>
#define PRINT(...)           printf(__VA_ARGS__)
#undef PRINT_ERR
#define PRINT_ERR(...)       fprintf(__VA_ARGS__)
#undef EXIT
#define EXIT(e)              exit(e)
#endif

#ifndef BOOL
#define BOOL                 uint8_t
#endif

#define unlikely(cond)       __builtin_expect((cond), 0)
#define likely(cond)         __builtin_expect((cond), 1)

#endif /* WT_H_ */
