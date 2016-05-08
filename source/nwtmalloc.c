/*
 * nwtmalloc.c
 *
 *  Created on: May 8, 2016
 *      Author: innocentevil
 */


#include "nwtmalloc.h"
#include "nwtree.h"
#include <pthread.h>

#define MIN_CACHE_SIZE       ((size_t) 1 << 14)


static pthread_key_t cache_key;
typedef struct {
	nwtreeRoot_t    root;
	size_t          total_sz;
	size_t          free_sz;
} nwt_cache_t;

static void nwt_cache_dest(void* cache);

void nwt_init()
{
	pthread_key_create(&cache_key, nwt_cache_dest);
}

void* nwt_malloc(size_t sz)
{
	nwt_cache_t* cache = pthread_getspecific(cache_key);
	if(!cache) {

	}
}

void* nwt_realloc(void* chnk, size_t sz)
{

}

void* nwt_calloc(size_t sz)
{

}

void nwt_free(void* chnk)
{

}

void nwt_purgeCache()
{

}

void nwt_purgeCacheForce()
{

}

static void nwt_cache_dest(void* cache)
{

}

