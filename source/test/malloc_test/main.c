

#include "yamalloc.h"
#include "segment.h"
#include "test/common.h"

#include <jemalloc/jemalloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>


static void* test_ym(void* );

#define LOOP_CNT                           40
#define TEST_CNT                           40000
#define REALLOC_MAX_SIZE                   (1 << 20)
#define MAX_REQ_SIZE                       8192
#define TH_CNT                             20



const trkey_t key_bfcache = 1;
static pthread_t thrs[TH_CNT];
static struct test_report reports[TH_CNT];


static void perf_test_nmalloc(void);
static void perf_test_oldmalloc(void);



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

	int cnt;
	large_person_t* p = NULL;
	small_person_t* sp = NULL;
	struct test_report* report = (struct test_report*) arg;
	nrbtreeRoot_t root;
	cdsl_nrbtreeRootInit(&root);

	int rn;
	clock_t sub_start,sub_end;
	struct timespec startts,endts;
	clock_gettime(CLOCK_REALTIME,&startts);
	uint32_t seed = (uint32_t) startts.tv_sec;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = (rand_r(&seed) % (1 << 20)) + 2;
		p = malloc(rn);
		report->rand_malloc_only_time +=  sub_end - sub_start;
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
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
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->malloc_time = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
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
			p->age = cnt;
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node);
		}

		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
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

	clock_gettime(CLOCK_REALTIME, &startts);
	for (loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		for (cnt = 0; cnt < TEST_CNT; cnt++) {
			sp = malloc(sizeof(small_person_t));
			sp->age = cnt;
			cdsl_nrbtreeNodeInit(&sp->node, cnt);
			cdsl_nrbtreeInsert(&root, &sp->node);
		}

		for (cnt = 0; cnt < TEST_CNT; cnt++) {
			sp = (small_person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if (!sp) {
				fprintf(stderr, "abnormal pointer from tree !!\n");
			}
			free(sp);
		}
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec))
			+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fixed_small_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			rn = ((rand_r(&seed) % MAX_REQ_SIZE) + sizeof(nrbtreeNode_t)) & ~3;
			p = malloc(rn);
			if(!(malloc_usable_size(p) > 0))
				exit(-1);
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node);
		}
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
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
	sp = (small_person_t*) malloc(sizeof(small_person_t));
	for(cnt = sizeof(small_person_t);cnt < REALLOC_MAX_SIZE;cnt <<= 1)
	{
		sp = (small_person_t*) realloc(sp, cnt);
		if(!sp)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		cdsl_nrbtreeNodeInit(&sp->node,0);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	free(sp);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->realloc_time = dt;

	size_t sz;
	clock_gettime(CLOCK_REALTIME, &startts);
	for (sz = sizeof(small_person_t); sz < (sizeof(small_person_t) << 1);sz++) {
		for (cnt = 2; cnt < TEST_CNT; cnt <<= 1) {
			sp = (small_person_t*) memalign(cnt, sz);
			if (!sp) {
				fprintf(stderr, "OOM\n");
				exit(-1);
			}
			cdsl_nrbtreeNodeInit(&sp->node, (trkey_t) sp);
			cdsl_nrbtreeInsert(&root, &sp->node);
		}
		while ((sp = (small_person_t*) cdsl_nrbtreeDeleteMax(&root))) {
			sp = container_of(sp, small_person_t, node);
			free(sp);
		}
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->memalign_time = dt;

	return (void*) report;
}

static void* test_ym(void* arg) {
	nrbtreeRoot_t root;
	struct test_report* report = (struct test_report*) arg;
	clock_t start,end;
	clock_t sub_start,sub_end;
	struct timespec startts,endts;
	int rn;
	int cnt;
	large_person_t* lp = NULL;
	small_person_t* sp = NULL;

	cdsl_nrbtreeRootInit(&root);


	clock_gettime(CLOCK_REALTIME,&startts);
	uint32_t seed = (uint32_t) startts.tv_sec;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = (rand_r(&seed) % (1 << 20)) + 2;
		lp = yam_malloc(rn);
		lp->age = cnt;
		cdsl_nrbtreeNodeInit(&lp->node,cnt);
		yam_free(lp);
		report->rand_free_only_time += sub_end - sub_start;
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	double dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_malloc_free_time = (double) (dt) / CLOCKS_PER_SEC;
	report->rand_free_only_time /= CLOCKS_PER_SEC;
	report->rand_malloc_only_time /= CLOCKS_PER_SEC;


	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		lp = yam_malloc(sizeof(large_person_t));
		lp->age = cnt;
		cdsl_nrbtreeNodeInit(&lp->node,cnt);
		cdsl_nrbtreeInsert(&root, &lp->node);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->malloc_time = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		lp = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
		if(!lp)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		yam_free(lp);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->free_time = dt;

	int loop_cnt;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			lp = yam_malloc(sizeof(large_person_t));
			lp->age = cnt;
			cdsl_nrbtreeNodeInit(&lp->node,cnt);
			cdsl_nrbtreeInsert(&root, &lp->node);
		}

		for(cnt = 0;cnt < TEST_CNT;cnt++){
			lp = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if(!lp)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			yam_free(lp);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fixed_mid_size = dt;

	clock_gettime(CLOCK_REALTIME, &startts);
	for (loop_cnt = 0; loop_cnt < LOOP_CNT; loop_cnt++) {
		for (cnt = 0; cnt < TEST_CNT; cnt++) {
			sp = yam_malloc(sizeof(small_person_t));
			sp->age = cnt;
			cdsl_nrbtreeNodeInit(&sp->node, cnt);
			cdsl_nrbtreeInsert(&root, &sp->node);
		}

		for (cnt = 0; cnt < TEST_CNT; cnt++) {
			sp = (small_person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if (!sp) {
				fprintf(stderr, "abnormal pointer from tree !!\n");
			}
			yam_free(sp);
		}
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec))
			+ ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fixed_small_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			rn = ((rand_r(&seed) % MAX_REQ_SIZE) + sizeof(nrbtreeNode_t)) & ~3;
			if(rn < 0)
				printf("wtf!\n");
			lp = yam_malloc(rn);
			if(!(yam_malloc_size(lp) > 0))
				exit(-1);
			cdsl_nrbtreeNodeInit(&lp->node,cnt);
			cdsl_nrbtreeInsert(&root, &lp->node);
		}
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			lp = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if(!lp)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			yam_free(lp);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_rnd_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	sp = (small_person_t*) yam_malloc(sizeof(small_person_t));
	for(cnt = sizeof(small_person_t);cnt < REALLOC_MAX_SIZE;cnt <<= 1)
	{
		sp = (small_person_t*) yam_realloc(sp, cnt);
		if(!sp)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		cdsl_nrbtreeNodeInit(&sp->node,0);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	yam_free(sp);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->realloc_time = dt;

	size_t sz;
	clock_gettime(CLOCK_REALTIME,&startts);
	for (sz = sizeof(small_person_t); sz < (sizeof(small_person_t) << 1) ; sz++) {
		for (cnt = 2; cnt < TEST_CNT; cnt <<= 1) {
			sp = (small_person_t*) yam_memalign(cnt, sz);
			if (!sp) {
				fprintf(stderr, "OOM\n");
				exit(-1);
			}
			cdsl_nrbtreeNodeInit(&sp->node, (trkey_t) sp);
			cdsl_nrbtreeInsert(&root, &sp->node);
		}
		while ((sp = (small_person_t*) cdsl_nrbtreeDeleteMax(&root))) {
			sp = container_of(sp, small_person_t, node);
			yam_free(sp);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->memalign_time= dt;

	return (void*) report;
}



static void perf_test_nmalloc(void)
{
	int i,j;
	struct test_report rpt = {0,};
	for (j = 0; j < 5; j++) {
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
