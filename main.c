/*
 * main.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */


#include "wtree.h"
#include "wtmalloc.h"
#include "ymalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "cdsl_nrbtree.h"
#include "nwtree.h"

#define TEST_CNT    1000
#define TH_CNT      1

typedef struct  {
	nrbtreeNode_t node;
	int           age;
}person_t;

struct test_report {
	double malloc_time;
	double free_time;
};

static void* malloc_test(void* );
static void* ymalloc_test(void* );

static pthread_t thrs[TH_CNT];

static int test_malloc_perf();


static nwtreeNode_t nodes[20];

int main(void){

	int i;
	nwtreeRoot_t root;
	uint32_t base = 0;
	uint32_t sz = 0;
	nwtree_rootInit(&root);
	for(i = 0;i < 20;i++)
	{
		sz = (rand() % 4096) + 512;
		nwtree_nodeInit(&nodes[i],base,sz);
		base += sz;
		nwtree_addNode(&root, &nodes[i]);
	}
	nwtree_print(&root);
//	test_malloc_perf();
	return 0;
}



static int test_malloc_perf()
{
	int th_cnt;
	struct test_report* report;
	double av_alloc_time;
	double av_free_time;
	for(th_cnt = 0;th_cnt < TH_CNT;th_cnt++)
	{
		pthread_create(&thrs[th_cnt], NULL, malloc_test, NULL);
	}
	for(th_cnt = 0;th_cnt < TH_CNT;th_cnt++)
	{
		pthread_join(thrs[th_cnt],&report);
		av_alloc_time += report->malloc_time;
		av_free_time += report->free_time;
	}
	printf("AV MALLOC Time : %f\n",av_alloc_time);
	printf("AV FREE Time : %f\n",av_free_time);


	av_alloc_time = 0;
	av_free_time = 0;
	for(th_cnt = 0;th_cnt < TH_CNT;th_cnt++)
	{
		pthread_create(&thrs[th_cnt], NULL, ymalloc_test, NULL);
	}
	for(th_cnt = 0;th_cnt < TH_CNT;th_cnt++)
	{
		pthread_join(thrs[th_cnt],&report);
		av_alloc_time += report->malloc_time;
		av_free_time += report->free_time;
	}
	printf("AV yMALLOC Time : %f\n",av_alloc_time);
	printf("AV yFREE Time : %f\n",av_free_time);
	return 0;
}


static void* malloc_test(void* arg)
{
	int cnt;
	person_t* p = NULL;
	struct test_report* report;
	report = malloc(sizeof(report));
	nrbtreeRoot_t root;
	cdsl_nrbtreeRootInit(&root);

	clock_t start,end;

	start = clock();
	int rn;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = rand() % 1000;
		p = malloc(rn);
		free(p);
	}
	end = clock();
	printf("simple repeat malloc & free : %f\n",(double) (end - start) / CLOCKS_PER_SEC);

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = malloc(sizeof(person_t));
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node);
	}
	end = clock();
	report->malloc_time = (double) (end - start) / CLOCKS_PER_SEC;
	printf("malloc finished!!\n");

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (person_t*) cdsl_nrbtreeDelete(&root, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		free(p);
	}
	end = clock();
	report->free_time = (double) (end - start) / CLOCKS_PER_SEC;
	return (void*) report;
}

static void* ymalloc_test(void* arg)
{
	int cnt;
	person_t* p = NULL;
	struct test_report* report;
	report = ymalloc(sizeof(report));
	nrbtreeRoot_t root;
	cdsl_nrbtreeRootInit(&root);

	clock_t start,end;

	start = clock();
	int rn;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = rand() % 1000;
		p = ymalloc(rn);
		yfree(p);
	}
	end = clock();
	printf("simple repeat malloc & free : %f\n",(double) (end - start) / CLOCKS_PER_SEC);

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = ymalloc(sizeof(person_t));
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node);
	}
	end = clock();
	report->malloc_time = (double) (end - start) / CLOCKS_PER_SEC;
	printf("malloc finished!!\n");

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (person_t*) cdsl_nrbtreeDelete(&root, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		yfree(p);
	}
	end = clock();
	report->free_time = (double) (end - start) / CLOCKS_PER_SEC;
	return (void*) report;
}

