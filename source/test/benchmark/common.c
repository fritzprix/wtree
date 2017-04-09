/*
 * common.c
 *
 *  Created on: Aug 6, 2016
 *      Author: innocentevil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>




#include "test/common.h"


void print_report(const char* test_name, struct test_report* report, int lcnt, int tcnt, int thread_cnt, size_t mx_req_sz, size_t mx_realloc_sz)
{
	printf("\n==== TEST CONDITION for [%s] ====\n", test_name);
	printf("LOOP Count : %d\n",lcnt);
	printf("TEST SIZE : %d\n", tcnt);
	printf("REQ Size VARIANCE %zu\n", mx_req_sz);
	printf("REALLOC MAX Size %zu\n", mx_realloc_sz);
	printf("# of Thread : %d\n",thread_cnt);
	printf("==== START OF TEST REPORT[%s] ====\n", test_name);
	printf("total time taken for repeated malloc & free of random size : %f\n",report->repeat_malloc_free_time);
	printf("total time taken for malloc in above loop %f\n", report->rand_malloc_only_time);
	printf("total time taken for free in above loop %f\n", report->rand_free_only_time);
	printf("total time taken for consecutive malloc of fixed size chunk  : %f\n",report->malloc_time);
	printf("total time taken for consecutive free of fixed size chunk : %f\n", report->free_time);
	printf("total time taken for looping of each consecutive mallocs & frees of fixed mid-sized chunk : %f\n", report->repeat_deep_malloc_free_time_fixed_mid_size);
	printf("total time taken for looping of each consecutive mallocs & frees of fixed small-sized chunk : %f\n", report->repeat_deep_malloc_free_time_fixed_small_size);
	printf("total time taken for looping of each consecutive mallocs & frees of random sized chunk : %f\n", report->repeat_deep_malloc_free_time_rnd_size);
	printf("total realloc time : %f\n", report->realloc_time);
	printf("total memalign time : %f\n", report->memalign_time);
	printf("==== END OF TEST REPORT[%s] ====\n\n", test_name);

}


BOOL perform_consecutive_random_malloc (
		void* pt_ctx,
		malloc_t malloc_func,
		produce_t on_produce,
		size_t min_size,
		size_t randomize_factor,
		long test_count,
		double* time_taken_sec) {

	if(!malloc_func) return FALSE;
	long count;
	void* ptr;
	size_t randomized_size;
	uint32_t rand_seed;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	rand_seed = (uint32_t) startts.tv_sec;
	for(count = 0; count < test_count; count++) {
		randomized_size = randomize_factor? (rand_r(&rand_seed) % randomize_factor) : 0;
		if(randomized_size < min_size) {
			randomized_size = min_size;
		}
		ptr = malloc_func(randomized_size);
		if(!ptr) {
			return FALSE;
		}
		if(on_produce) on_produce(pt_ctx, ptr, randomized_size);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	if(time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}

BOOL perform_consecutive_fix_malloc (
		void* pt_ctx,
		malloc_t malloc_func,
		produce_t on_produce,
		size_t size,
		long test_count,
		double* time_taken_sec) {

	if(!malloc_func) return FALSE;
	long count;
	void* ptr;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(count = 0; count < test_count; count++) {
		ptr = malloc_func(size);
		if (!ptr) {
			return FALSE;
		}
		if (on_produce) {
			on_produce(pt_ctx, ptr, size);
		}
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	if (time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
				+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}

BOOL perform_consecutive_free_memalign (
		void* pt_ctx,
		free_t free_func,
		consume_t on_consume,
		validator_t on_validate,
		double* time_taken_sec) {

	if(!on_consume || !free_func) return FALSE;
	void* ptr;
	size_t sz;
	BOOL result = TRUE;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	int cnt = 0;
	while((ptr = on_consume(pt_ctx, &sz))) {
		if(on_validate) {
			result &= on_validate(pt_ctx, ptr, sz);
		}
		free_func(ptr);
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	if (time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
				+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return result;
}


BOOL perform_consecutive_free (
		void* pt_ctx,
		free_t free_func,
		consume_t on_consume,
		validator_t on_validate,
		double* time_taken_sec) {

	if(!on_consume || !free_func) return FALSE;
	void* ptr;
	size_t sz;
	BOOL result = TRUE;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	while((ptr = on_consume(pt_ctx, &sz))) {
		if(on_validate) {
			result &= on_validate(pt_ctx, ptr, sz);
		}
		free_func(ptr);
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	if (time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
				+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return result;
}


BOOL perform_consecutive_malloc_free_pair(
		void* pt_ctx,
		malloc_t malloc_func,
		free_t free_func,
		size_t size,
		long test_count,
		double* time_taken_sec) {

	if(!malloc_func || !free_func) return FALSE;
	long count;
	void* ptr;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(count = 0; count < test_count; count++) {
		ptr = malloc_func(size);
		if (!ptr) {
			return FALSE;
		}
		free_func(ptr);
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	if (time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
				+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}

BOOL perform_consecutive_random_malloc_free_pair(
		void* pt_ctx,
		malloc_t malloc_func,
		free_t free_func,
		size_t min_size,
		size_t randomize_factor,
		long test_count,
		double* time_taken_sec) {

	if(!malloc_func || !free_func) return FALSE;
	long count;
	void* ptr;
	size_t randomized_size;
	uint32_t rand_seed;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	rand_seed = (uint32_t) startts.tv_sec;
	for(count = 0; count < test_count; count++) {
		randomized_size = (randomize_factor? (rand_r(&rand_seed) % randomize_factor) : 0) + min_size;
		ptr = malloc_func(randomized_size);
		if(!ptr) {
			return FALSE;
		}
		free_func(ptr);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	if(time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}

BOOL perfrom_consecutive_memalign(
		void* pt_ctx,
		memalign_t memalign_func,
		produce_t on_produce,
		size_t align_min,
		size_t align_max,
		size_t size,
		long test_count,
		double* time_taken_sec) {

	if(!memalign_func) return FALSE;
	long count;
	void* ptr;
	size_t alignment;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(count = 0; count < test_count; count++) {
		for(alignment = align_min; alignment < align_max; alignment <<= 1) {
			ptr = memalign_func(alignment, size);
			if(!ptr) {
				return FALSE;
			}
			if(on_produce) {
				on_produce(pt_ctx, ptr, size);
			}
		}
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	if (time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
				+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}

BOOL perform_consecutive_memalign_random(
		void* pt_ctx,
		memalign_t memalign_func,
		produce_t on_produce,
		size_t align_min,
		size_t align_max,
		size_t min_size,
		size_t randomize_factor,
		long test_count,
		double* time_taken_sec) {

	if(!memalign_func) return FALSE;
	long count;
	void* ptr;
	size_t alignment, size;
	struct timespec startts, endts;
	clock_gettime(CLOCK_REALTIME, &startts);
	uint32_t rand_seed = (uint32_t) startts.tv_sec;

	for(count = 0; count < test_count; count++) {
//		printf("COUNT %d\n", count);
		for(alignment = align_min; alignment < align_max ; alignment <<= 1) {
			size = (rand_r(&rand_seed) % randomize_factor) + min_size;
			ptr = memalign_func(alignment, size);
			if(!ptr) {
				return FALSE;
			}
			if(on_produce) {
				on_produce(pt_ctx, ptr, size);
			}
		}
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	if(time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
						+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}

BOOL perform_consecutive_realloc(
		void* pt_ctx,
		realloc_t realloc_func,
		free_t free_func,
		size_t start_sz,
		size_t end_sz,
		double* time_taken_sec) {

	if(!realloc_func || !free_func) return FALSE;
	long count;
	void* ptr = NULL;
	size_t alignment, size;
	struct timespec startts, endts;
	clock_gettime(CLOCK_REALTIME, &startts);

	for(size = start_sz; size < end_sz; size <<= 1) {
		ptr = realloc_func(ptr, size);
		if(!ptr) {
			return FALSE;
		}
	}
	free_func(ptr);
	clock_gettime(CLOCK_REALTIME, &endts);
	if(time_taken_sec) {
		*time_taken_sec += ((((endts.tv_nsec - startts.tv_nsec))
						+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	}
	return TRUE;
}
