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
#include <sys/mman.h>
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

typedef struct {
	nrbtreeNode_t   node;
	uint64_t        base;
}mem_alloc_t;
static nwtreeNode_t nodes[20];
static nrbtreeNode_t rbnodes[20];
static mem_alloc_t alloc_nodes[20];
const char* MARKER = "HELLO_MALLOC!!";
static DECLARE_PURGE_CALLBACK(onpurge);



int main(void){

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

	for(i = 0;i < 20;i++)
	{
		cdsl_nrbtreeNodeInit(&rbnodes[i],i);
		cdsl_nrbtreeInsert(&rbroot,&rbnodes[i]);
	}
	while(!cdsl_nrbtreeIsEmpty(&rbroot))
	{
		trnode = cdsl_nrbtreeDeleteMax(&rbroot);
		printf("node key : %lu\n",trnode->key);
	}
	printf("========== Start Allocate Segment with mmap ===========\n");
	for(i = 0;i < 20;i++)
	{
		sz = ((rand() % 4096) + 512) & ~(0x1FF);
		chk = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		printf("Add segment /w size : %d @ %lu --> " , sz, (uint64_t) chk);
		nwtree_baseNodeInit(&nodes[i],chk,sz);
		nwtree_addNode(&root, &nodes[i]);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %u & free size : %u\n",tsz,fsz);
	}
	printf("========== Repeat reclaim chunk & free ===========\n");
	for(i = 0;i < 20;i++)
	{
		sz = ((rand() % 2048) + 512);
		chk = nwtree_reclaim_chunk(&root, sz);
		printf("Reclaimed chunk size : %u --> ", sz);
		node = (nwtreeNode_t*) chk;
		if(!chk)
		{
			printf("fail to alloc\n");
		}
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %u & free size : %u\n",tsz,fsz);
		nwtree_nodeInit(node, chk, sz);
		nwtree_addNode(&root, node);
		fsz = nwtree_freeSize(&root);
		printf("After free operation Free size : %u\n",fsz);
	}
	nwtree_purge(&root, onpurge);
	tsz = nwtree_totalSize(&root);
	fsz = nwtree_freeSize(&root);
	printf("===========  purge operation ================\n");
	printf("After Purge operation -> ");
	printf("Total Size : %u & free size : %u\n",tsz,fsz);

	printf("========== Start Allocate Segment with mmap ===========\n");
	for(i = 0;i < 20;i++)
	{
		sz = ((rand() % 4096) + 512) & ~(0x1FF);
		chk = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		printf("Add segment /w size : %d @ %lu --> " , sz, (uint64_t) chk);
		nwtree_baseNodeInit(&nodes[i],chk,sz);
		nwtree_addNode(&root, &nodes[i]);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %u & free size : %u\n",tsz,fsz);
	}
	for(i = 0;i < 10;i++)
	{
		sz = ((rand() % 2048) + 512);
		chk = nwtree_reclaim_chunk(&root, sz);
		printf("Reclaimed chunk size : %u --> ", sz);
		trnode = (nrbtreeNode_t*) chk;
		if(!chk)
		{
			printf("fail to alloc\n");
		}
		cdsl_nrbtreeNodeInit(&alloc_nodes[i].node,sz);
		alloc_nodes[i].base = (uint64_t) chk;
		memcpy(chk, MARKER, strlen(MARKER));
		printf("Written Markker : %s\n",chk);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %u & free size : %u\n",tsz,fsz);
		cdsl_nrbtreeInsert(&rbroot,&alloc_nodes[i].node);
	}
	nwtree_purge(&root, onpurge);
	tsz = nwtree_totalSize(&root);
	fsz = nwtree_freeSize(&root);
	printf("===========  purge operation ================\n");
	printf("After Purge operation -> ");
	printf("Total Size : %u & free size : %u\n",tsz,fsz);
	mem_alloc_t* alloc_node;
	while(!cdsl_nrbtreeIsEmpty(&rbroot))
	{
		alloc_node = (mem_alloc_t*) cdsl_nrbtreeDeleteMax(&rbroot);
		if(!alloc_node)
		{
			perror("unexpected null!\n");
			exit(-1);
		}
		node = (nwtreeNode_t*) alloc_node->base;
		if(memcmp(node, MARKER,sizeof(MARKER)))
		{
			perror("unexpected corruption!\n");
			exit(-1);
		}
		nwtree_nodeInit(alloc_node->base, alloc_node->base, alloc_node->node.key);
		printf("free chunk /w size of %u -> ",alloc_node->node.key);
		nwtree_addNode(&root, node);
		tsz = nwtree_totalSize(&root);
		fsz = nwtree_freeSize(&root);
		printf("Total Size : %u & free size : %u\n",tsz,fsz);
	}
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

static DECLARE_PURGE_CALLBACK(onpurge)
{
	munmap(node->base,node->base_size);
	return TRUE;
}
