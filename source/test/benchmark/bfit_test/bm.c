

#include "bestfit.h"
#include "segment.h"
#include "common.h"


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <jemalloc/jemalloc.h>


static void* test_bfit(void* );

#ifndef SEGMENT_UNIT_SIZE
#define SEGMNET_UNIT_SIZE                (1 << 24)
#endif

#ifndef BFITCACHE_UNIT_SIZE
#define BFITCACHE_UNIT_SIZE               (1 << 21)
#endif

#define LOOP_CNT                           40
#define TEST_CNT                           40000
#define MAX_REQ_SIZE                       8192
#define TH_CNT                             10



static DECLARE_ONALLOCATE(ext_segment_mapper);
static DECLARE_ONFREE(ext_segment_unmapper);

static DECLARE_ONALLOCATE(bfitcache_mapper);
static DECLARE_ONFREE(bfitcache_unmapper);


const trkey_t key_bfcache = 1;
static pthread_t thrs[TH_CNT];
static struct test_report reports[TH_CNT];


static void perf_test_nmalloc(void);
static void perf_test_oldmalloc(void);



int perform_benchmark(void) {

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

	int cnt;
	large_person_t* p = NULL;
	struct test_report* report = (struct test_report*) arg;
	rbtreeRoot_t root;
	cdsl_rbtreeRootInit(&root);

	size_t t_sz;
	clock_t sub_start,sub_end;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	uint32_t seed = (uint32_t) startts.tv_sec;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		t_sz = (size_t) (rand_r(&seed) % (1 << 20)) + 2;
		p = malloc(t_sz);
		report->rand_malloc_only_time +=  sub_end - sub_start;
		p->size = cnt;
		cdsl_rbtreeNodeInit(&p->node,cnt);
		free(p);
		report->rand_free_only_time += sub_end - sub_start;
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	double dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_malloc_free_time = (double) (dt) / CLOCKS_PER_SEC;
	report->rand_free_only_time /= CLOCKS_PER_SEC;
	report->rand_malloc_only_time /= CLOCKS_PER_SEC;


	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = malloc(sizeof(large_person_t));
		p->size = cnt;
		cdsl_rbtreeNodeInit(&p->node,cnt);
		cdsl_rbtreeInsert(&root, &p->node);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->malloc_time = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (large_person_t*) cdsl_rbtreeDelete(&root, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		free(p);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->free_time = dt;

	int loop_cnt;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = malloc(sizeof(large_person_t));
			p->size = cnt;
			cdsl_rbtreeNodeInit(&p->node,cnt);
			cdsl_rbtreeInsert(&root, &p->node);
		}

		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_rbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			free(p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fixed_mid_size = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			t_sz = (size_t)((rand_r(&seed) % MAX_REQ_SIZE) + sizeof(rbtreeNode_t)) & ~3;
			p = malloc(t_sz);
			cdsl_rbtreeNodeInit(&p->node,cnt);
			cdsl_rbtreeInsert(&root, &p->node);
		}
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_rbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			free(p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_rnd_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	p = (large_person_t*) malloc(1);
	for(cnt = 1;cnt < TEST_CNT;cnt <<= 1)
	{
		p = (large_person_t*) realloc(p, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
	}
	free(p);
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->realloc_time = dt;

	return (void*) report;
}

static void* test_bfit(void* arg) {
	segmentRoot_t sgroot;
	bfitRoot_t* bfroot;
	rbtreeRoot_t root;
	struct test_report* report = (struct test_report*) arg;
	clock_t start,end;
	clock_t sub_start,sub_end;
	struct timespec startts,endts;
	int rn;
	int cnt;
	large_person_t* p = NULL;

	cdsl_rbtreeRootInit(&root);
	segment_root_init(&sgroot, NULL,ext_segment_mapper, ext_segment_unmapper);
	segment_create_cache(&sgroot, key_bfcache);
	bfroot = bfit_root_create(&sgroot, bfitcache_mapper,bfitcache_unmapper);


	clock_gettime(CLOCK_REALTIME,&startts);
	uint32_t seed = (uint32_t) startts.tv_sec;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = (rand_r(&seed) % (1 << 20)) + 2;
		p = bfit_reclaim_chunk(bfroot,rn);
		p->size = (size_t) cnt;
		cdsl_rbtreeNodeInit(&p->node,cnt);
		bfit_free_chunk(bfroot, p);
		report->rand_free_only_time += sub_end - sub_start;
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	double dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_malloc_free_time = (double) (dt) / CLOCKS_PER_SEC;
	report->rand_free_only_time /= CLOCKS_PER_SEC;
	report->rand_malloc_only_time /= CLOCKS_PER_SEC;


	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = bfit_reclaim_chunk(bfroot, sizeof(large_person_t));
		p->size = cnt;
		cdsl_rbtreeNodeInit(&p->node,cnt);
		cdsl_rbtreeInsert(&root, &p->node);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->malloc_time = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (large_person_t*) cdsl_rbtreeDelete(&root, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		bfit_free_chunk(bfroot, p);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->free_time = dt;

	int loop_cnt;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = bfit_reclaim_chunk(bfroot, sizeof(large_person_t));
			p->size = cnt;
			cdsl_rbtreeNodeInit(&p->node,cnt);
			cdsl_rbtreeInsert(&root, &p->node);
		}

		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_rbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			bfit_free_chunk(bfroot, p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fixed_mid_size = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			rn = ((rand_r(&seed) % MAX_REQ_SIZE) + sizeof(rbtreeNode_t)) & ~3;
			if(rn < 0)
				printf("wtf!\n");
			p = bfit_reclaim_chunk(bfroot, rn);
			cdsl_rbtreeNodeInit(&p->node,cnt);
			cdsl_rbtreeInsert(&root, &p->node);
		}
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_rbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			bfit_free_chunk(bfroot, p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_rnd_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	p = (large_person_t*) bfit_reclaim_chunk(bfroot, 1);
	for(cnt = 1;cnt < TEST_CNT;cnt <<= 1)
	{
		p = (large_person_t*) bfit_grows_chunk(bfroot, p, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
	}
	bfit_free_chunk(bfroot, p);
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->realloc_time = dt;

	small_chunk_t* sp;
	size_t sz;
	for (sz = sizeof(small_chunk_t); sz < (sizeof(small_chunk_t) << 1) ; sz++) {
		for (cnt = 2; cnt < TEST_CNT; cnt <<= 1) {
			sp = (small_chunk_t*) bfit_reclaim_aligned_chunk(bfroot,sz, cnt);
			if (!sp) {
				fprintf(stderr, "OOM\n");
				exit(-1);
			}
			cdsl_rbtreeNodeInit(&sp->node, (trkey_t) sp);
			cdsl_rbtreeInsert(&root, &sp->node);
		}
		while ((sp = (small_chunk_t*) cdsl_rbtreeDeleteMax(&root))) {
			sp = container_of(sp, small_chunk_t, node);
			bfit_free_chunk(bfroot, sp);
		}
	}

	segment_cleanup(&sgroot);
	return NULL;
}


static DECLARE_ONALLOCATE(ext_segment_mapper) {
	size_t seg_sz = SEGMNET_UNIT_SIZE;
	while(seg_sz < total_sz) seg_sz <<= 1;
	*rsz = seg_sz;
	return mmap(NULL, seg_sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static DECLARE_ONFREE(ext_segment_unmapper) {
	munmap(addr, sz);
	return TRUE;
}


static DECLARE_ONALLOCATE(bfitcache_mapper) {
	size_t cache_sz = BFITCACHE_UNIT_SIZE;
	segmentRoot_t* seg_root  = (segmentRoot_t*) ext_ctx;
	while(cache_sz < total_sz) cache_sz <<= 1;
	*rsz = cache_sz;
	return segment_map(seg_root, key_bfcache, cache_sz);
}

static DECLARE_ONFREE(bfitcache_unmapper){
	segmentRoot_t* seg_root = (segmentRoot_t*) ext_ctx;
	segment_unmap(seg_root, key_bfcache, addr, sz);
	return TRUE;
}



static void perf_test_nmalloc(void)
{
	int i;
	struct test_report rpt = {0,};
	for(i = 0;i < TH_CNT;i++) {
		pthread_create(&thrs[i], NULL, test_bfit, &reports[i]);
	}
	for(i = 0;i < TH_CNT;i++) {
		pthread_join(thrs[i], NULL);
		rpt.free_time += reports[i].free_time;
		rpt.malloc_time += reports[i].malloc_time;
		rpt.rand_malloc_only_time += reports[i].rand_malloc_only_time;
		rpt.rand_free_only_time += reports[i].rand_free_only_time;
		rpt.repeat_malloc_free_time += reports[i].repeat_malloc_free_time;
		rpt.repeat_deep_malloc_free_time_fixed_mid_size += reports[i].repeat_deep_malloc_free_time_fixed_mid_size;
		rpt.repeat_deep_malloc_free_time_rnd_size += reports[i].repeat_deep_malloc_free_time_rnd_size;
		rpt.realloc_time += reports[i].realloc_time;
	}
	print_report("new malloc",&rpt, LOOP_CNT, TEST_CNT, TH_CNT, MAX_REQ_SIZE, 0);

}

static void perf_test_oldmalloc(void)
{
	int i;
	struct test_report rpt = {0,};
	for(i = 0;i < TH_CNT;i++) {
		pthread_create(&thrs[i], NULL, malloc_test,&reports[i]);
	}
	for(i = 0;i < TH_CNT;i++) {
		pthread_join(thrs[i], NULL);
		rpt.free_time += reports[i].free_time;
		rpt.malloc_time += reports[i].malloc_time;
		rpt.rand_malloc_only_time += reports[i].rand_malloc_only_time;
		rpt.rand_free_only_time += reports[i].rand_free_only_time;
		rpt.repeat_malloc_free_time += reports[i].repeat_malloc_free_time;
		rpt.repeat_deep_malloc_free_time_fixed_mid_size += reports[i].repeat_deep_malloc_free_time_fixed_mid_size;
		rpt.repeat_deep_malloc_free_time_rnd_size += reports[i].repeat_deep_malloc_free_time_rnd_size;
		rpt.realloc_time += reports[i].realloc_time;
	}
	print_report("old malloc",&rpt, LOOP_CNT, TEST_CNT, TH_CNT, MAX_REQ_SIZE, 0);

}
