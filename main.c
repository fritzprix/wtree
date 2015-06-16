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

static uint8_t heap[1 << 16];
static rb_treeNode_t* root;

int main(void){
	wtreeHeap_init(heap,sizeof(heap));
	rb_treeNode_t* buffer;
	root = NULL;
	size_t sz = 0;
	size_t tsz = 1;
	int test_cnt = 1;
	while(test_cnt--){
		printf("Test count : %d\n",test_cnt);
		tsz = 0;
		int alloccnt = 100;
		while (alloccnt--) {
			printf("Alloc Count : %d\n",alloccnt);
			sz = rand() % (sizeof(heap) >> 3) + sizeof(rb_treeNode_t);
			buffer = wtreeHeap_malloc(sz);
			if (buffer) {
				printf("allocated : addr -> %d , size -> %d\n ", buffer, sz);
				wtreeHeap_print();
				tsz += sz;
				cdsl_rbtreeNodeInit(buffer, sz);
				cdsl_rbtreeInsert(&root, buffer);
			}else {

				printf("Alloc Failed /w size of %d\n",sz);
				wtreeHeap_print();
				buffer = cdsl_rbtreeDeleteMax(&root);
				printf("Freed Chunk : addr -> %d, size -> %d\n",buffer, buffer->key);
				wtreeHeap_free(buffer);
				wtreeHeap_print();
			}
		}
		cdsl_rbtreePrint(&root);
		buffer = cdsl_rbtreeDeleteMax(&root);
		if(buffer){
			do {
				printf("chunk addr %d, chunk size : %d\n", (uint32_t) buffer,buffer->key);
				wtreeHeap_free(buffer);
				wtreeHeap_print();
				buffer = cdsl_rbtreeDeleteMax(&root);
			} while (buffer);
			cdsl_rbtreePrint(&root);
		}
	}
	return 0;
}
