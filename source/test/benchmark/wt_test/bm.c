/*
 * main.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */


#include "wtmalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <jemalloc/jemalloc.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "cdsl_nrbtree.h"
#include "wtree.h"

#define LOOP_CNT     40
#define TEST_CNT     40000
#define MAX_REQ_SIZE 4096
#define TH_CNT       8



static void* malloc_test(void* );
static void* ymalloc_test(void* );

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
		wt_init();
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
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node, FALSE);
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
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node, FALSE);
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



	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			rn = rand_r(&seed) % MAX_REQ_SIZE + sizeof(nrbtreeNode_t) & ~3;
			p = malloc(rn);
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node, FALSE);
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

static void* ymalloc_test(void* arg)
{

	int cnt;
	large_person_t* p = NULL;
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
		p = wt_malloc(rn);
		report->rand_malloc_only_time +=  sub_end - sub_start;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		wt_free(p);
		report->rand_free_only_time += sub_end - sub_start;
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	double dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_malloc_free_time = (double) (dt) / CLOCKS_PER_SEC;
	report->rand_free_only_time /= CLOCKS_PER_SEC;
	report->rand_malloc_only_time /= CLOCKS_PER_SEC;


	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = wt_malloc(sizeof(large_person_t));
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node, FALSE);
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
		wt_free(p);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->free_time = dt;

	int loop_cnt;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = wt_malloc(sizeof(large_person_t));
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node, FALSE);
		}

		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			wt_free(p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fixed_mid_size = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			rn = rand_r(&seed) % MAX_REQ_SIZE + sizeof(nrbtreeNode_t) & ~3;
			p = wt_malloc(rn);
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node, FALSE);
		}
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (large_person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			wt_free(p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_rnd_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	p = (large_person_t*) wt_malloc(1);
	for(cnt = 1;cnt < TEST_CNT;cnt <<= 1)
	{
		p = (large_person_t*) wt_realloc(p, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
	}
	wt_free(p);
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->realloc_time = dt;

	return (void*) report;
}


static void perf_test_nmalloc(void)
{
	int i;
	struct test_report rpt = {0,};
	for(i = 0;i < TH_CNT;i++) {
		pthread_create(&thrs[i], NULL, ymalloc_test, &reports[i]);
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
	print_report("new malloc",&rpt,LOOP_CNT, TEST_CNT, TH_CNT, MAX_REQ_SIZE, 0);
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
	print_report("old malloc", &rpt, LOOP_CNT, TEST_CNT, TH_CNT, MAX_REQ_SIZE, 0);
}


