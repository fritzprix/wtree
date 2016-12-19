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

typedef struct {
	nrbtreeNode_t node;
	size_t        size;
}small_chunk_t;

typedef struct  {
	union {
		struct {
			nrbtreeNode_t node;
			size_t        size;
		};
		small_chunk_t    small_chunk;
	};
	char          dummy[50];
} large_person_t;



// function template for memory allocator functions
typedef void produce_t(void* pt_ctx, void*, size_t);
typedef void* consume_t(void* pt_ctx, size_t*);
typedef BOOL validator_t(void* pt_ctx, void*,size_t);

typedef void* malloc_t(size_t);
typedef void* realloc_t(void*, size_t);
typedef void* memalign_t(size_t, size_t);
typedef void free_t(void*);

extern int perform_benchmark(void);

extern BOOL perform_consecutive_random_malloc(void* pt_ctx, malloc_t malloc_func, produce_t on_produce, size_t min_size, size_t randomize_factor, long test_count,double* time_taken_sec);
extern BOOL perform_consecutive_fix_malloc(void* pt_ctx, malloc_t malloc_func, produce_t on_produce, size_t size, long test_count, double* time_taken_sec);
extern BOOL perform_consecutive_free(void* pt_ctx, free_t free_func, consume_t on_consume,validator_t on_validate,double* time_taken_sec);
extern BOOL perform_consecutive_malloc_free_pair(void* pt_ctx, malloc_t malloc_func, free_t free_func, size_t size, long test_count, double* time_taken_sec);
extern BOOL perform_consecutive_free_memalign(void* pt_ctx, free_t free_func, consume_t on_consume,validator_t on_validate,double* time_taken_sec);
extern BOOL perform_consecutive_random_malloc_free_pair(void* pt_ctx, malloc_t malloc_func, free_t free_func, size_t min_size, size_t randomize_factor, long test_count, double* time_taken_sec);
extern BOOL perfrom_consecutive_memalign(void* pt_ctx, memalign_t memalign_func, produce_t on_produce, size_t align_min, size_t align_max,size_t size,long test_count, double* time_taken_sec);
extern BOOL perform_consecutive_memalign_random(void* pt_ctx, memalign_t memalign_func, produce_t on_produce, size_t align_min, size_t align_max,size_t min_size, size_t randomize_factor, long test_count, double* time_taken_sec);
extern BOOL perform_consecutive_realloc(void* pt_ctx, realloc_t realloc_func, free_t free_func,  size_t start_sz, size_t end_sz , double* time_taken_sec);

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
