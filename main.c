/*
 * main.c
 *
 *  Created on: Jun 13, 2015
 *      Author: innocentevil
 */


#include "wtree.h"
#include "malloc.h"
#include <stdio.h>
#include "cdsl_rbtree.h"

static uint8_t heap1[1 << 16];
static uint8_t heap2[1 << 16];
static rb_treeNode_t* root;
static wt_alloc_t alloc1;
static wt_alloc_t alloc2;
static wt_heaproot_t heap_root;

int main(void){

	wtreeHeap_initCacheRoot(&heap_root);
	wtreeHeap_initCacheNode(&alloc1,heap1,sizeof(heap1));
	wtreeHeap_initCacheNode(&alloc2,heap2,sizeof(heap2));

	wtreeHeap_addCache(&heap_root,&alloc1);
	wtreeHeap_addCache(&heap_root,&alloc2);

	rb_treeNode_t* buffer;
	root = NULL;
	size_t sz = 0;
	size_t totalAllocSz = 0;
	size_t totalFreeSz = 0;
	int test_cnt = 10000;
	while(test_cnt--){
//		printf("Test count : %d\n",test_cnt);
		totalAllocSz = 0;
		totalFreeSz = 0;
		int alloccnt = 500;
		while (alloccnt--) {
//			printf("Alloc Count : %d\n",alloccnt);
			sz = rand() % (sizeof(heap1) >> 6) + sizeof(rb_treeNode_t);
			buffer = wtreeHeap_malloc(&heap_root,sz);
			if (buffer) {
//				printf("allocated : addr -> %d , size -> %d\n ", buffer, sz);
//				wtreeHeap_print();
				totalAllocSz += sz;
				cdsl_rbtreeNodeInit(buffer, sz);
				cdsl_rbtreeInsert(&root, buffer);
			}else {
//				printf("Alloc Failed /w size of %d\n",sz);
//				wtreeHeap_print();
				buffer = cdsl_rbtreeDeleteMax(&root);
				if (buffer) {
//					printf("Freed Chunk : addr -> %d, size -> %d\n", buffer, buffer->key);
					totalFreeSz += buffer->key;
					wtreeHeap_free(&heap_root,buffer);
				}
	//			wtreeHeap_print(&alloc);
			}
		}
//		printf("Total Allocated Size : %d, Total Freed Size : %d\n",totalAllocSz,totalFreeSz);
//		printf("Total Size in Heap : %d\n",wtreeHeap_size(&alloc));
//		cdsl_rbtreePrint(&root);
		buffer = cdsl_rbtreeDeleteMax(&root);
		if(buffer){
			do {
//				printf("chunk addr %d, chunk size : %d\n", (uint32_t) buffer,buffer->key);
				wtreeHeap_free(&heap_root,buffer);
//				wtreeHeap_print();
				buffer = cdsl_rbtreeDeleteMax(&root);
			} while (buffer);
//			cdsl_rbtreePrint(&root);
		}
	}
	return 0;
}
