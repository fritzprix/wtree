/*
 * common.h
 *
 *  Created on: Aug 6, 2016
 *      Author: innocentevil
 */




#ifndef INCLUDE_TEST_COMMON_H_
#define INCLUDE_TEST_COMMON_H_

#include "cdsl_nrbtree.h"
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct test_report {
	double rand_malloc_only_time;
	double rand_free_only_time;
	double repeat_malloc_free_time;
	double malloc_time;
	double free_time;
	double realloc_time;
	double memalign_time;
	double repeat_deep_malloc_free_time_fixed_mid_size;
	double repeat_deep_malloc_free_time_fixed_small_size;
	double repeat_deep_malloc_free_time_rnd_size;
};


typedef struct  {
	nrbtreeNode_t node;
	int           age;
	char          firstname[10];
	char          lastname[20];
	char          email[20];
} large_person_t;

typedef struct {
	nrbtreeNode_t node;
	int           age;
}small_person_t;

extern void print_report(const char* test_name, struct test_report* report, int lcnt, int tcnt, int thread_cnt, size_t mx_req_sz, size_t mx_realloc_sz);
#ifdef __DBG
#define dbg_print(...)      printf( __VA_ARGS__)
#else
#define dbg_print(...)
#endif


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_TEST_COMMON_H_ */
