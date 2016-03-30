/*
 * ymalloc.c
 *
 *  Created on: Mar 30, 2016
 *      Author: innocentevil
 */


#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>
#include "ymalloc.h"
#include "cdsl_nrbtree.h"


struct ymem_cb {
	nrbtreeNode_t node;
	wtreeRoot_t	  ymem_heap_root;
	size_t		  total_sz;
	size_t		  free_sz;
};



struct ymem_root {
	pthread_mutex_t lock;
	nrbtreeRoot_t __root;
	size_t sz;
};


static struct ymem_cb* ymem_create_heap_node(size_t sz);

static struct ymem_root ROOT = {
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.__root = {0,},
		.sz = 0
};



void* ymalloc(size_t sz)
{
	pthread_t tid = pthread_self();
	struct ymem_cb* wt_heap = cdsl_nrbtreeLookup(&ROOT.__root,tid);
	if(!wt_heap){
		/*
		 * there is no per-thread heap root
		 * initialize per-thread heap
		 */
		wt_heap = ymem_create_heap_node(sz);
		/*
		 * add new heap node into static single node
		 */

		pthread_mutex_lock(&ROOT.lock);
		cdsl_nrbtreeInsert(&ROOT.__root, &wt_heap->ymem_heap_root);
		ROOT.sz += wt_heap->total_sz;
		pthread_mutex_unlock(&ROOT.lock);
	}else{

	}
}

void yfree(void* ptr)
{

}
