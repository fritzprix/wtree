

#include "yamalloc.h"
#include "segment.h"
#include "test/common.h"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>


#include <jemalloc/jemalloc.h>

#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>


static void* test_ym(void* );

#define LOOP_CNT                           40
#define TEST_CNT                           40000
#define REALLOC_MAX_SIZE                   (1 << 20)
#define MAX_REQ_SIZE                       8192
#define TH_CNT                             20
#define MEM_ALIGN_TEST_CNT                 1000


const trkey_t key_bfcache = 1;
static pthread_t thrs[TH_CNT];
static struct test_report reports[TH_CNT];


/**
 * typedef void produce_t(void*, size_t);
typedef void* consume_t(size_t*);
typedef BOOL validator_t(void*,size_t);
 */
static void on_produce(void*, void*, size_t);
static void* on_consume(void*, size_t*);
static BOOL on_validate(void*, void*, size_t);
struct test_context {
	nrbtreeRoot_t root;
	int cnt;
};


static void perf_test_nmalloc(void);
static void perf_test_oldmalloc(void);

static int memchck(void*, int v, size_t sz);



int main(void){

	pid_t pid = fork();
	if(pid > 0) {
		wait(NULL);
		pid = fork();
		if(pid > 0) {
			wait(NULL);
			return 0;
		}
		else if(pid == 0) {
			perf_test_oldmalloc();
		}
		else
		{
			perror("fork fail\n");
			exit(-1);
		}
	}
	else if(pid == 0) {
		perf_test_nmalloc();
	}
	else
	{
		perror("fork fail\n");
		exit(-1);
	}
	return 0;
}


static void* malloc_test(void* arg)
{
	struct test_context tctx;
	cdsl_nrbtreeRootInit(&tctx.root);
	large_person_t* lp = NULL;
	small_chunk_t* sp = NULL;
	int loop_cnt;

	struct test_report* report = (struct test_report*) arg;
	double time_taken_sec = 0;

	perform_consecutive_random_malloc_free_pair(&tctx, malloc, free, 2, MAX_REQ_SIZE, TEST_CNT, &report->repeat_malloc_free_time);

	perform_consecutive_fix_malloc(&tctx, malloc, on_produce,sizeof(large_person_t), TEST_CNT, &report->malloc_time);
	perform_consecutive_free(&tctx, free, on_consume, on_validate, &report->free_time);
	assert(cdsl_nrbtreeIsEmpty(&tctx.root));

	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_fix_malloc(&tctx, malloc, on_produce, sizeof(large_person_t), TEST_CNT, &report->repeat_deep_malloc_free_time_fixed_mid_size);
		perform_consecutive_free(&tctx, free, on_consume, on_validate, &report->repeat_deep_malloc_free_time_fixed_mid_size);
	}

	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_fix_malloc(&tctx, malloc, on_produce, sizeof(small_chunk_t), TEST_CNT, &report->repeat_deep_malloc_free_time_fixed_small_size);
		perform_consecutive_free(&tctx, free, on_consume, on_validate, &report->repeat_deep_malloc_free_time_fixed_small_size);
	}
	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_random_malloc(&tctx, malloc, on_produce, sizeof(small_chunk_t), MAX_REQ_SIZE, TEST_CNT, &time_taken_sec);
		perform_consecutive_free(&tctx, free, on_consume, on_validate, &report->repeat_deep_malloc_free_time_rnd_size);
	}

	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_realloc(&tctx, realloc, free, sizeof(small_chunk_t), REALLOC_MAX_SIZE, &report->realloc_time);
	}
/*
	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_memalign_random(&tctx, memalign, on_produce, sizeof(small_chunk_t), TEST_CNT, sizeof(small_chunk_t), MAX_REQ_SIZE, MEM_ALIGN_TEST_CNT,&report->memalign_time);
		perform_consecutive_free_memalign(&tctx, free, on_consume, on_validate, &report->memalign_time);
	}
*/
	return (void*) report;
}

static void* test_ym(void* arg) {
	struct test_context tctx;
	cdsl_nrbtreeRootInit(&tctx.root);
	large_person_t* lp = NULL;
	small_chunk_t* sp = NULL;
	int loop_cnt;

	struct test_report* report = (struct test_report*) arg;
	double time_taken_sec = 0;

	perform_consecutive_random_malloc_free_pair(&tctx, yam_malloc, yam_free, 2, MAX_REQ_SIZE, TEST_CNT, &report->repeat_malloc_free_time);

	perform_consecutive_fix_malloc(&tctx, yam_malloc, on_produce,sizeof(large_person_t), TEST_CNT, &report->malloc_time);
	perform_consecutive_free(&tctx, yam_free, on_consume, on_validate, &report->free_time);
	assert(cdsl_nrbtreeIsEmpty(&tctx.root));


	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_fix_malloc(&tctx, yam_malloc, on_produce, sizeof(large_person_t), TEST_CNT, &report->repeat_deep_malloc_free_time_fixed_mid_size);
		perform_consecutive_free(&tctx, yam_free, on_consume, on_validate, &report->repeat_deep_malloc_free_time_fixed_mid_size);
	}

	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_fix_malloc(&tctx, yam_malloc, on_produce, sizeof(small_chunk_t), TEST_CNT, &report->repeat_deep_malloc_free_time_fixed_small_size);
		perform_consecutive_free(&tctx, yam_free, on_consume, on_validate, &report->repeat_deep_malloc_free_time_fixed_small_size);
	}

	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_random_malloc(&tctx, yam_malloc, on_produce, sizeof(small_chunk_t), MAX_REQ_SIZE, TEST_CNT, &time_taken_sec);
		perform_consecutive_free(&tctx, yam_free, on_consume, on_validate, &report->repeat_deep_malloc_free_time_rnd_size);
	}

	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_realloc(&tctx, yam_realloc, yam_free, sizeof(small_chunk_t), REALLOC_MAX_SIZE, &report->realloc_time);
	}

/*
	for(loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		perform_consecutive_memalign_random(&tctx, yam_memalign, on_produce, sizeof(small_chunk_t), TEST_CNT, sizeof(small_chunk_t), MAX_REQ_SIZE, MEM_ALIGN_TEST_CNT,&report->memalign_time);
		perform_consecutive_free_memalign(&tctx, yam_free, on_consume, on_validate, &report->memalign_time);
	}
*/
	return (void*) report;
}



static void perf_test_nmalloc(void)
{
	int i,j;
	struct test_report rpt = {0,};
	for (j = 0; j < 5; j++) {
		printf("LOOP %d\n", j);
		for (i = 0; i < TH_CNT; i++) {
			pthread_create(&thrs[i], NULL, test_ym, &reports[i]);
		}
		for (i = 0; i < TH_CNT; i++) {
			pthread_join(thrs[i], NULL);
			rpt.free_time += reports[i].free_time;
			rpt.malloc_time += reports[i].malloc_time;
			rpt.rand_malloc_only_time += reports[i].rand_malloc_only_time;
			rpt.rand_free_only_time += reports[i].rand_free_only_time;
			rpt.repeat_malloc_free_time += reports[i].repeat_malloc_free_time;
			rpt.repeat_deep_malloc_free_time_fixed_mid_size +=
					reports[i].repeat_deep_malloc_free_time_fixed_mid_size;
			rpt.repeat_deep_malloc_free_time_fixed_small_size +=
					reports[i].repeat_deep_malloc_free_time_fixed_small_size;
			rpt.repeat_deep_malloc_free_time_rnd_size +=
					reports[i].repeat_deep_malloc_free_time_rnd_size;
			rpt.realloc_time += reports[i].realloc_time;
			rpt.memalign_time += reports[i].memalign_time;
		}
	}
	print_report("new malloc",&rpt, LOOP_CNT, TEST_CNT, TH_CNT, MAX_REQ_SIZE, REALLOC_MAX_SIZE);
}

static void perf_test_oldmalloc(void)
{
	int j,i;
	struct test_report rpt = {0,};
	for(j = 0;j < 5; j++) {
		printf("LOOP %d\n", j);
		for (i = 0; i < TH_CNT; i++) {
			pthread_create(&thrs[i], NULL, malloc_test, &reports[i]);
		}
		for (i = 0; i < TH_CNT; i++) {
			pthread_join(thrs[i], NULL);
			rpt.free_time += reports[i].free_time;
			rpt.malloc_time += reports[i].malloc_time;
			rpt.rand_malloc_only_time += reports[i].rand_malloc_only_time;
			rpt.rand_free_only_time += reports[i].rand_free_only_time;
			rpt.repeat_malloc_free_time += reports[i].repeat_malloc_free_time;
			rpt.repeat_deep_malloc_free_time_fixed_mid_size +=
					reports[i].repeat_deep_malloc_free_time_fixed_mid_size;
			rpt.repeat_deep_malloc_free_time_fixed_small_size +=
					reports[i].repeat_deep_malloc_free_time_fixed_small_size;
			rpt.repeat_deep_malloc_free_time_rnd_size +=
					reports[i].repeat_deep_malloc_free_time_rnd_size;
			rpt.realloc_time += reports[i].realloc_time;
			rpt.memalign_time += reports[i].memalign_time;
		}
	}
	print_report("old malloc",&rpt, LOOP_CNT, TEST_CNT, TH_CNT, MAX_REQ_SIZE, REALLOC_MAX_SIZE);
}



static void on_produce(void* test_ctx, void* ptr, size_t sz) {
	struct test_context* tctx = (struct test_context*) test_ctx;
	small_chunk_t* sp = (small_chunk_t*) ptr;
	cdsl_nrbtreeNodeInit(&sp->node, (trkey_t) ptr);
	cdsl_nrbtreeInsert(&tctx->root, &sp->node, FALSE);
	sp->size = sz;
}

static void* on_consume(void* test_ctx, size_t* sz) {
	struct test_context* tctx = (struct test_context*) test_ctx;
	small_chunk_t* sp = (small_chunk_t*) cdsl_nrbtreeDeleteMin(&tctx->root);
	if(!sp) {
		*sz = 0;
		return NULL;
	}
	sp = container_of(sp, small_chunk_t, node);
	*sz = sp->size;
	return sp;
}

static BOOL on_validate(void* test_ctx, void* ptr, size_t sz) {
	struct test_context* tctx = (struct test_context*) test_ctx;
	return TRUE;
}
