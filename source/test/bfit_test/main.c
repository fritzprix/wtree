

#include "bestfit.h"
#include "segment.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

static void* test_bfit(void* );

#ifndef SEGMENT_UNIT_SIZE
#define SEGMNET_UNIT_SIZE                (1 << 24)
#endif

#ifndef BFITCACHE_UNIT_SIZE
#define BFITCACHE_UNIT_SIZE               (1 << 21)
#endif

#define TEST_CNT                           100
#define LOOP_CNT                           100
#define MAX_REQ_SIZE                   1 << 20

static DECLARE_ONALLOCATE(ext_segment_mapper);
static DECLARE_ONFREE(ext_segment_unmapper);

static DECLARE_ONALLOCATE(bfitcache_mapper);
static DECLARE_ONFREE(bfitcache_unmapper);

typedef struct  {
	nrbtreeNode_t node;
	int           age;
	char          firstname[10];
	char          lastname[20];
	char          email[20];
}person_t;

struct test_report {
	double rand_malloc_only_time;
	double rand_free_only_time;
	double repeat_malloc_free_time;
	double malloc_time;
	double free_time;
	double realloc_time;
	double repeat_deep_malloc_free_time_fix_size;
	double repeat_deep_malloc_free_time_rnd_size;
};

const trkey_t key_bfcache = 1;



int main(void) {
	pthread_t pids[10];
	int i;
	struct test_report treport;
	for(i = 0;i < 10;i++) {
		pthread_create(&pids[i], NULL, test_bfit, &treport);
		pthread_join(pids[i], NULL);
	}
	printf("successfully finished\n");
	return 0;
}


static void* test_bfit(void* arg) {
	segmentRoot_t sgroot;
	bfitRoot_t bfroot;
	nrbtreeRoot_t root;
	struct test_report* report = (struct test_report*) arg;
	clock_t start,end;
	clock_t sub_start,sub_end;
	struct timespec startts,endts;
	int rn;
	int cnt;
	person_t* p = NULL;

	cdsl_nrbtreeRootInit(&root);
	segment_root_init(&sgroot, NULL,ext_segment_mapper, ext_segment_unmapper);
	segment_create_cache(&sgroot, key_bfcache);
	bfit_root_init(&bfroot, &sgroot, bfitcache_mapper,bfitcache_unmapper);


	clock_gettime(CLOCK_REALTIME,&startts);
	uint32_t seed = (uint32_t) startts.tv_sec;
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		rn = (rand_r(&seed) % (1 << 20)) + 2;
		p = bfit_reclaim_chunk(&bfroot,rn);
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		bfit_free_chunk(&bfroot, p);
		report->rand_free_only_time += sub_end - sub_start;
	}
	clock_gettime(CLOCK_REALTIME, &endts);
	double dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_malloc_free_time = (double) (dt) / CLOCKS_PER_SEC;
	report->rand_free_only_time /= CLOCKS_PER_SEC;
	report->rand_malloc_only_time /= CLOCKS_PER_SEC;


	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = bfit_reclaim_chunk(&bfroot, sizeof(person_t));
		p->age = cnt;
		cdsl_nrbtreeNodeInit(&p->node,cnt);
		cdsl_nrbtreeInsert(&root, &p->node);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->malloc_time = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	for(cnt = 0;cnt < TEST_CNT;cnt++){
		p = (person_t*) cdsl_nrbtreeDelete(&root, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
		bfit_free_chunk(&bfroot, p);
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->free_time = dt;

	int loop_cnt;
	clock_gettime(CLOCK_REALTIME,&startts);
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = bfit_reclaim_chunk(&bfroot, sizeof(person_t));
			p->age = cnt;
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node);
		}

		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			bfit_free_chunk(&bfroot, p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_fix_size = dt;



	clock_gettime(CLOCK_REALTIME,&startts);
	seed = (uint32_t) startts.tv_sec;
	for(loop_cnt = 0;loop_cnt < LOOP_CNT; loop_cnt++) {
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			rn = (rand_r(&seed) % MAX_REQ_SIZE) + sizeof(nrbtreeNode_t) & ~3;
			p = bfit_reclaim_chunk(&bfroot, rn);
			cdsl_nrbtreeNodeInit(&p->node,cnt);
			cdsl_nrbtreeInsert(&root, &p->node);
		}
		for(cnt = 0;cnt < TEST_CNT;cnt++){
			p = (person_t*) cdsl_nrbtreeDelete(&root, cnt);
			if(!p)
			{
				fprintf(stderr,"abnormal pointer from tree !!\n");
			}
			bfit_free_chunk(&bfroot, p);
		}
	}
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->repeat_deep_malloc_free_time_rnd_size = dt;


	clock_gettime(CLOCK_REALTIME,&startts);
	p = (person_t*) bfit_reclaim_chunk(&bfroot, 1);
	for(cnt = 1;cnt < TEST_CNT;cnt <<= 1)
	{
		p = (person_t*) bfit_grows_chunk(&bfroot, p, cnt);
		if(!p)
		{
			fprintf(stderr,"abnormal pointer from tree !!\n");
		}
	}
	bfit_free_chunk(&bfroot, p);
	clock_gettime(CLOCK_REALTIME,&endts);
	dt = ((((endts.tv_nsec - startts.tv_nsec)) + ((endts.tv_sec - startts.tv_sec) * 1E+9)) / 1E+9);
	report->realloc_time = dt;
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
