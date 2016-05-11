/*
 * main.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */


#include "nwtmalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cdsl_nrbtree.h"
#include "nwtree.h"

#define TEST_CNT    10000
#define TH_CNT      50

typedef struct  {
	nrbtreeNode_t node;
	int           age;
	char          firstname[10];
	char          lastname[20];
	char          email[20];
}person_t;

struct test_report {
	double repeat_malloc_free_time;
	double malloc_time;
	double free_time;
	double realloc_time;
};

static void* malloc_test(void* );
static void* ymalloc_test(void* );
static void basic_poc(void);
static void print_report(const char* test_name, struct test_report* report);

static pthread_t thrs[TH_CNT];
static struct test_report reports[TH_CNT];

static DECLARE_PURGE_CALLBACK(onpurge);

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
			nwt_init();
			perf_test_nmalloc();
		}
		else
		{
			perror("fork fail\n");
			exit(-1);
		}
	}
	else if(pid == 0) {
		perf_test_oldmalloc();
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
//	printf("========== old malloc(libc default) test begin =============\n");
	int cnt;
	person_t* p = NULL;
	struct test_report* report = (struct test_report*) arg;
	nrbtreeRoot_t root;
	cdsl_nrbtreeRootInit(&root);

	clock_t start,end;

	start = clock();
	int rn;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = (rand() % 1000) & ~7;
		p = malloc(rn);
		free(p);
	}
	end = clock();
	report->repeat_malloc_free_time = (double) (end - start) / CLOCKS_PER_SEC;
//	printf("simple repeat malloc & free : %f\n",(double) (end - start) / CLOCKS_PER_SEC);

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = malloc(sizeof(person_t));
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node);
	}
	end = clock();
	report->malloc_time = (double) (end - start) / CLOCKS_PER_SEC;
//	printf("malloc finished %f!!\n", report->malloc_time);

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
	start = clock();
	p = (person_t*) malloc(1);
	for(cnt = 1;cnt < TEST_CNT;cnt++)
	{
		p = (person_t*) realloc(p, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
	}
	free(p);
	end = clock();
	report->realloc_time = (double) (end - start) / CLOCKS_PER_SEC;
//	printf("free finished %f!!\n", report->free_time);
//	printf("========== old malloc(libc default) test end =============\n");

	return (void*) report;
}

static void* ymalloc_test(void* arg)
{

//	printf("========== new malloc(wtmalloc) test begin =============\n");
	int cnt;
	person_t* p = NULL;
	struct test_report* report = (struct test_report*) arg;
	nrbtreeRoot_t root;
	cdsl_nrbtreeRootInit(&root);

	clock_t start,end;

	start = clock();
	int rn;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = (rand() % 1000);
		p = malloc(rn);
		free(p);
	}
	end = clock();
	report->repeat_malloc_free_time = (double) (end - start) / CLOCKS_PER_SEC;

//	printf("simple repeat malloc & free : %f\n",(double) (end - start) / CLOCKS_PER_SEC);

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = nwt_malloc(sizeof(person_t));
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node);
	}
	end = clock();
	report->malloc_time = (double) (end - start) / CLOCKS_PER_SEC;
//	printf("malloc finished %f!!\n", report->malloc_time);

	start = clock();
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (person_t*) cdsl_nrbtreeDelete(&root, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		nwt_free(p);
	}
	end = clock();
	report->free_time = (double) (end - start) / CLOCKS_PER_SEC;

	start = clock();
	p = (person_t*) nwt_malloc(1);
	for(cnt = 1;cnt < TEST_CNT;cnt++)
	{
		p = (person_t*) nwt_realloc(p, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
	}
	nwt_free(p);
	end = clock();
	report->realloc_time = (double) (end - start) / CLOCKS_PER_SEC;

//	printf("free finished %f!!\n", report->free_time);
//	printf("========== new malloc(wtmalloc) test end =============\n");
	return (void*) report;
}

static void basic_poc(void)
{
	int i;
	nwtreeRoot_t root;
	nrbtreeRoot_t rbroot;
	cdsl_nrbtreeRootInit(&rbroot);
	uint32_t base = 0;
	uint32_t sz = 0;
	void* chk = NULL;
	nwtree_rootInit(&root);
	size_t fsz,tsz = 0;
	nwtreeNode_t* node;
	nrbtreeNode_t* trnode = NULL;

	printf("========== Start Allocate Segment with mmap ===========\n");
	for(i = 0;i < 20;i++)
	{
		/*
		 *  grows heap size by add segment from mmap operation
		 */
		sz = ((rand() % 4096) + 512) & ~(0x1FF);
		chk = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		printf("Add segment /w size : %u @ %lu --> " , sz, (uint64_t) chk);
		nwtree_baseNodeInit(chk,chk,sz);   // initialize segment header
		nwtree_addNode(&root, chk);        // add initialized segment into heap
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
	}
	printf("========== Repeat reclaim chunk & free ===========\n");
	for(i = 0;i < 20;i++)
	{
		sz = ((rand() % 2048) + 512);
		sz &= ~3;
		chk = nwtree_reclaim_chunk(&root, sz);  // allocate random size
		printf("Reclaimed chunk size : %u --> ", sz);
		node = (nwtreeNode_t*) chk;
		if(!chk)
		{
			printf("fail to alloc\n");
		}
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
		nwtree_nodeInit(node, chk, sz);
		nwtree_addNode(&root, node);
		fsz = nwtree_freeSize(&root);
		printf("After free operation Free size : %zu\n",fsz);
	}
	nwtree_purge(&root, onpurge);
	tsz = nwtree_totalSize(&root);
	fsz = nwtree_freeSize(&root);
	printf("===========  purge operation ================\n");
	printf("After Purge operation -> ");
	printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
	printf("========== Start Allocate Segment with mmap ===========\n");
	for(i = 0;i < 20;i++)
	{
		sz = ((rand() % 4096) + 512) & ~(0x1FF);
		chk = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		printf("Add segment /w size : %d @ %lu --> " , sz, (uint64_t) chk);
		nwtree_baseNodeInit(chk,chk,sz);
		nwtree_addNode(&root, chk);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
	}
	for(i = 0;i < 10;i++)
	{
		sz = ((rand() % 2048) + 512);
		sz &= ~3;
		chk = nwtree_reclaim_chunk(&root, sz);
		printf("Reclaimed chunk size : %u --> ", sz);
		trnode = (nrbtreeNode_t*) chk;
		if(!chk)
		{
			printf("fail to alloc\n");
		}
		cdsl_nrbtreeNodeInit(trnode,sz);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
		cdsl_nrbtreeInsert(&rbroot,trnode);
	}
	nwtree_purge(&root, onpurge);
	tsz = nwtree_totalSize(&root);
	fsz = nwtree_freeSize(&root);
	printf("===========  purge operation ================\n");
	printf("After Purge operation -> ");
	printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
	while(!cdsl_nrbtreeIsEmpty(&rbroot))
	{
		trnode =  cdsl_nrbtreeDeleteMax(&rbroot);
		if(!trnode)
		{
			perror("unexpected null!\n");
			exit(-1);
		}
		node = (nwtreeNode_t*) trnode;
		nwtree_nodeInit(node, trnode, trnode->key);
		printf("free chunk /w size of %lu -> ",trnode->key);
		nwtree_addNode(&root, node);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
	}
	for(i = 0;i < 10;i++)
	{
		sz = ((rand() % 2048) + 512);
		sz &= ~3;
		chk = nwtree_reclaim_chunk(&root, sz);
		printf("Reclaimed chunk size : %u --> ", sz);
		trnode = (nrbtreeNode_t*) chk;
		if(!chk)
		{
			printf("fail to alloc\n");
		}
		cdsl_nrbtreeNodeInit(trnode,sz);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
		cdsl_nrbtreeInsert(&rbroot,trnode);
	}
	tsz = nwtree_totalSize(&root);
	fsz = nwtree_freeSize(&root);
	printf("Total Size : %zu & free size : %zu\n",tsz,fsz);
	nwtree_purgeAll(&root,onpurge);
	tsz = nwtree_totalSize(&root);
	fsz = nwtree_freeSize(&root);
	printf("After PurgeForce Total Size : %zu & free size : %zu\n",tsz,fsz);
}

static void print_report(const char* test_name, struct test_report* report)
{
	printf("\n==== START OF TEST REPORT[%s] ====\n", test_name);
	printf("total malloc free repeatition time : %f\n",report->repeat_malloc_free_time);
	printf("total malloc time : %f\n",report->malloc_time);
	printf("total free time : %f\n", report->free_time);
	printf("total realloc time : %f\n", report->realloc_time);
	printf("==== END OF TEST REPORT[%s] ====\n", test_name);

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
		rpt.repeat_malloc_free_time += reports[i].repeat_malloc_free_time;
		rpt.realloc_time += reports[i].realloc_time;
	}
	print_report("new malloc",&rpt);
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
		rpt.repeat_malloc_free_time += reports[i].repeat_malloc_free_time;
		rpt.realloc_time += reports[i].realloc_time;
	}
	print_report("old malloc", &rpt);
}



static DECLARE_PURGE_CALLBACK(onpurge)
{
	munmap(node->base,node->base_size);
	return TRUE;
}
