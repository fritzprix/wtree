/*
 * malloc.h
 *
 *  Created on: Aug 12, 2016
 *      Author: innocentevil
 */

#ifndef MALLOC_H_
#define MALLOC_H_

#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define malloc               yam_malloc
#define realloc              yam_realloc
#define free                 yam_free
#define calloc               yam_calloc
#define memalign             yam_memalign


#ifdef __cplusplus
}
#endif

#endif /* MALLOC_H_ */
