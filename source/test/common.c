/*
 * common.c
 *
 *  Created on: Aug 6, 2016
 *      Author: innocentevil
 */


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
